#include "benchmark.h"
#include "bench_evaluation.h"
#include "utils.h"

#include <json/json.h>

#include <string>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <map>

#include "itkImageFileReader.h"
#include "itkBinaryThresholdImageFilter.h"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

// system depedant Unix include to create folders....
#ifdef __unix__ //
  #include <sys/stat.h>
  #include <sys/types.h>
#else // assume windows
  #include <conio.h>
  #include <dir.h>
  #include <process.h>
  #include <stdio.h>
#endif

int main(int argc, char** argv)
{
  // ------------------
  // Reading arguments 
  // ------------------

  namespace po = boost::program_options;
  // parsing arguments
  po::options_description general_opt("Allowed options are ");
  general_opt.add_options()
    ("help,h", "display this message")
    ("input,i",po::value<std::string>(),"Input image ")
    ("parametersFile,p",po::value<std::string>()->default_value("parameters.json"),"ParameterFile : input json file");
  bool parsingOK = true;
  po::variables_map vm;

  try{
    po::store(po::parse_command_line(argc,argv,general_opt),vm);
  }catch(const std::exception& ex)
    {
      parsingOK = false;
      std::cout<<"Error checking program option"<<ex.what()<< std::endl;
    }

  po::notify(vm);
  if( !parsingOK || vm.count("help") || argc<=1 )
    {
      std::cout<<"\n Usage : benchmark [input file] [optionnal parameterFile] \n\n"
	       <<" input : input file listing patient name, image,mask and ground truth\n"
	       <<" parametersFile : input Json parameter file. If none provided, default is parameters.json\n"<<std::endl;
    
      return 0;
    }

  std::string inputFileName = vm["input"].as<std::string>();
  std::string parameterFileName = vm["parametersFile"].as<std::string>();

  // -----------------
  // Reading JSON file
  // -----------------

  std::ifstream configFile(parameterFileName,std::ifstream::binary);
  if(!configFile.is_open())
    {
      std::cout<<"couldn't find "<<parameterFileName<<" file...aborting"<<std::endl;
      exit(-1);
    }
      
  std::cout<<"opening JSON file...";
  Json::Value root;
  configFile >> root;
  configFile.close();
  std::cout<<"done\n"<<std::endl;

  // -----------------
  // Reading inputFileList
  // -----------------

  std::ifstream f;
  f.open(inputFileName);
  std::string patientName;
  std::string imgName;
  std::string maskName;
  std::string gtName;

  std::string benchDir = "bench/";

  // parsing csv file name using JSON file Name
  
  size_t pos = parameterFileName.find(".");
  int backSlash = parameterFileName.find_last_of("/");
  std::string benchName;
  if(backSlash)
  {
    benchName = parameterFileName.substr(backSlash+1,pos-backSlash-1);
  }
  else
  {
    benchName = parameterFileName.substr (0,pos);
  }
  std::string csvFileName = benchDir + benchName + ".csv"; // we want everything before ".json" and we replace extension
  // opening resultFileStream
  std::ofstream csvFileStream;
  std::cout<<"creating csv file :"<<csvFileName<<std::endl;
  csvFileStream.open(csvFileName, ios::out | ios::trunc); // if the file already exists, we discard content
  if( csvFileStream.is_open() )
  {
    csvFileStream <<"SerieName,Name,Threshold,TP,TN,FP,FN,sensitivity,specificity,precision,accuracy,Dice,MCC"<<std::endl;
  } 
  else{ 
    throw "Error opening csv file....";
  }


  //creating root directory
  #ifdef __unix__
    mkdir(benchDir.c_str(),S_IRWXG | S_IRWXO | S_IRWXU);
  #else
    mkdir("bench");
  #endif

  using ImageType = itk::Image<double,3>;
  using GroundTruthImageType = itk::Image<int,3>;
  using MaskImageType = itk::Image<int,3>;

  using DicomImageType = itk::Image<int16_t,3>;
  using DicomGroundTruthImageType = itk::Image<uint8_t,3>;
  using DicomMaskImageType = itk::Image<uint8_t,3>;
  
  while(std::getline(f,patientName))
  {
    std::getline(f,imgName);
    std::getline(f,maskName);
    std::getline(f,gtName);

    std::cout<<patientName<<std::endl;
    std::cout<<imgName<<std::endl;
    std::cout<<maskName<<std::endl;
    std::cout<<gtName<<std::endl;

    //creating root directory
    #ifdef __unix__
      mkdir( (benchDir+benchName).c_str(),S_IRWXG | S_IRWXO | S_IRWXU);
    #else
      mkdir("bench/"+patientName);
    #endif

    //creating subdirectory with patient
    #ifdef __unix__
      mkdir( (benchDir+benchName+"/"+patientName).c_str(),S_IRWXG | S_IRWXO | S_IRWXU);
    #else
      mkdir("bench/"+patientName);
    #endif

    // reading groundTruthImage path, if it is Directory, we assume all inputs are full DICOM 16 bits
    // Mask is only useful for statistics during segmentation assessment, 
    // drawback : Computation is done on full image with ircad DB, advantages : No registration required, no heavy refactoring needed
    
    if( vUtils::isDir( gtName ) ) // boolean choice for now, 0 is nifti & 1 is DICOM 
    {
      
      std::cout<<"Using dicom groundTruth data...."<<std::endl;
      DicomGroundTruthImageType::Pointer groundTruth = vUtils::readImage<DicomGroundTruthImageType>(gtName,false);
      DicomMaskImageType::Pointer maskImage = vUtils::readImage<DicomMaskImageType>(maskName,false);
      
      Benchmark<DicomImageType,DicomGroundTruthImageType,DicomMaskImageType> b(root,imgName,csvFileStream,groundTruth,maskImage);
      b.SetOutputDirectory(benchDir+benchName+"/"+patientName);
      b.SetPatientDirectory(benchName+"/"+patientName);
      b.SetDicomInput();
      b.run();
    }
    else
    {
      std::cout<<"Using NIFTI groundtruth data...."<<std::endl;
      GroundTruthImageType::Pointer groundTruth = vUtils::readImage<GroundTruthImageType>(gtName,false);
      MaskImageType::Pointer maskImage = vUtils::readImage<MaskImageType>(maskName,false);
      
      Benchmark<ImageType,GroundTruthImageType,MaskImageType> b(root,imgName,csvFileStream,groundTruth,maskImage);
      b.SetOutputDirectory(benchDir+benchName+"/"+patientName);
      b.SetPatientDirectory(benchName+"/"+patientName);
      b.SetNiftiInput();
      b.run();
    }
    if( !f.is_open())
    {
      std::cout<<"we are doomed"<<std::endl;
    }
  }
  f.close();
  csvFileStream.close();
}
