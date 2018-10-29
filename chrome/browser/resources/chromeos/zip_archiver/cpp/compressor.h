// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_H_

#include <ctime>
#include <memory>

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"

class CompressorArchive;
class CompressorStream;
class JavaScriptCompressorRequestorInterface;
class JavaScriptMessageSenderInterface;

// Handles all packing operations like creating archive objects and writing data
// onto the archive.
class Compressor {
 public:
  Compressor(const pp::InstanceHandle& instance_handle /* Used for workers. */,
             int compressor_id,
             JavaScriptMessageSenderInterface* message_sender);

  virtual ~Compressor();

  // Initializes the compressor.
  bool Init();

  // Creates an archive object.
  void CreateArchive();

  // Adds an entry to the archive.
  void AddToArchive(const pp::VarDictionary& dictionary);

  // Processes a file chunk sent from JavaScript.
  void ReadFileChunkDone(const pp::VarDictionary& dictionary);

  // Receives a write chunk response from JavaScript.
  void WriteChunkDone(const pp::VarDictionary& dictionary);

  // Releases all resources obtained by minizip.
  void CloseArchive(const pp::VarDictionary& dictionary);

  // Cancels the compression process.
  void CancelArchive(const pp::VarDictionary& dictionary);

  // A getter function for the message sender.
  JavaScriptMessageSenderInterface* message_sender() { return message_sender_; }

  // A getter function for the requestor.
  JavaScriptCompressorRequestorInterface* requestor() {
    return requestor_.get();
  }

  // A getter function for the compressor id.
  int compressor_id() { return compressor_id_; }

 private:
  // A callback helper for AddToArchive.
  void AddToArchiveCallback(int32_t, const pp::VarDictionary& dictionary);

  // A callback helper for CloseArchive.
  void CloseArchiveCallback(int32_t, bool has_error);

  // The compressor id of this compressor.
  int compressor_id_;

  // An object that sends messages to JavaScript.
  JavaScriptMessageSenderInterface* message_sender_;

  // A worker for jobs that require blocking operations or a lot of processing
  // time. Those shouldn't be done on the main thread. The jobs submitted to
  // this thread are executed in order, so a new job must wait for the last job
  // to finish.
  pp::SimpleThread worker_;

  // Callback factory used to submit jobs to worker_.
  pp::CompletionCallbackFactory<Compressor> callback_factory_;

  // A requestor for making calls to JavaScript.
  std::unique_ptr<JavaScriptCompressorRequestorInterface> requestor_;

  // An instance that takes care of all IO operations.
  std::unique_ptr<CompressorStream> compressor_stream_;

  // Minizip wrapper instance per compressor, shared across all operations.
  std::unique_ptr<CompressorArchive> compressor_archive_;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_H_
