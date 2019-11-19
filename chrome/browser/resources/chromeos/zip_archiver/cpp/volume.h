// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_archive.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/lock.h"
#include "ppapi/utility/threading/simple_thread.h"

class JavaScriptMessageSenderInterface;
class JavaScriptRequestorInterface;

// A factory that creates VolumeArchive(s). Useful for testing.
class VolumeArchiveFactoryInterface {
 public:
  virtual ~VolumeArchiveFactoryInterface() {}

  // Creates a new VolumeArchive.
  virtual std::unique_ptr<VolumeArchive> Create(
      std::unique_ptr<VolumeReader> reader) = 0;
};

// A factory that creates VolumeReader(s). Useful for testing.
class VolumeReaderFactoryInterface {
 public:
  virtual ~VolumeReaderFactoryInterface() {}

  // Creates a new VolumeReader. Returns nullptr if failed.
  virtual std::unique_ptr<VolumeReader> Create(int64_t archive_size) = 0;
};

// Handles all operations like reading metadata and reading files from a single
// Volume.
class Volume {
 public:
  Volume(const pp::InstanceHandle& instance_handle /* Used for workers. */,
         const std::string& file_system_id,
         JavaScriptMessageSenderInterface* message_sender);

  // Used by tests to create custom VolumeArchive and VolumeReader objects.
  // VolumeArchiveFactory and VolumeReaderFactory should be allocated with new
  // and the ownership will be passed to Volume on constructing it.
  Volume(const pp::InstanceHandle& instance_handle /* Used for workers. */,
         const std::string& file_system_id,
         JavaScriptMessageSenderInterface* message_sender,
         std::unique_ptr<VolumeArchiveFactoryInterface> volume_archive_factory,
         std::unique_ptr<VolumeReaderFactoryInterface> volume_reader_factory);

  virtual ~Volume();

  // Initializes the volume.
  bool Init();

  // Reads archive metadata using minizip.
  void ReadMetadata(const std::string& request_id,
                    const std::string& encoding,
                    int64_t archive_size);

  // Processes a successful archive chunk read from JavaScript. Read offset
  // represents the offset from where the data contained in array_buffer starts.
  void ReadChunkDone(const std::string& nacl_request_id,
                     const pp::VarArrayBuffer& array_buffer,
                     int64_t read_offset);

  // Processes an invalid archive chunk read from JavaScript.
  void ReadChunkError(const std::string& nacl_request_id);

  // Processes a successful passphrase read from JavaScript.
  void ReadPassphraseDone(const std::string& nacl_request_id,
                          const std::string& passphrase);

  // Processes an error when requesting a passphrase from JavaScript.
  void ReadPassphraseError(const std::string& nacl_request_id);

  // Opens a file.
  void OpenFile(const std::string& request_id,
                int64_t index,
                const std::string& encoding,
                int64_t archive_size);

  // Closes a file.
  void CloseFile(const std::string& request_id,
                 const std::string& open_request_id);

  // Reads a file contents from offset to offset + length. dictionary
  // should contain the open_request_id, the offset and the length with
  // the keys as defined in "request" namespace, and they should have
  // valid types. The reason for not passing them directly is that
  // pp::CompletionCallbackFactory can create a callback with a maximum of
  // 3 parameters, not 4 as needed here (including request_id).
  void ReadFile(const std::string& request_id,
                const pp::VarDictionary& dictionary);

  JavaScriptMessageSenderInterface* message_sender() { return message_sender_; }
  JavaScriptRequestorInterface* requestor() { return requestor_.get(); }
  std::string file_system_id() { return file_system_id_; }

 private:
  // Encapsulates arguments to OpenFileCallback, as NewCallback supports binding
  // up to three arguments, while here we have four.
  struct OpenFileArgs;

  // A callback helper for ReadMetadata.
  void ReadMetadataCallback(int32_t result,
                            const std::string& request_id,
                            const std::string& encoding,
                            int64_t archive_size);

  // A callback helper for OpenFile.
  void OpenFileCallback(int32_t result, const OpenFileArgs& args);

  // A callback helper for CloseFile.
  void CloseFileCallback(int32_t result,
                         const std::string& request_id,
                         const std::string& open_request_id);

  // A callback helper for ReadFile.
  void ReadFileCallback(int32_t result,
                        const std::string& request_id,
                        const pp::VarDictionary& dictionary);

  // Creates a new archive object for this volume.
  VolumeArchive* CreateVolumeArchive(const std::string& request_id,
                                     const std::string& encoding,
                                     int64_t archive_size);

  // Clears job.
  void ClearJob();

  // Minizip wrapper instance per volume, shared across all operations.
  std::unique_ptr<VolumeArchive> volume_archive_;

  // The file system id for this volume.
  const std::string file_system_id_;

  // An object that sends messages to JavaScript. Not owned.
  JavaScriptMessageSenderInterface* message_sender_;

  // A worker for jobs that require blocking operations or a lot of processing
  // time. Those shouldn't be done on the main thread. The jobs submitted to
  // this thread are executed in order, so a new job must wait for the last job
  // to finish.
  // TODO(cmihail): Consider using multiple workers in case of many jobs to
  // improve execution speedup. In case multiple workers are added
  // synchronization between workers might be needed.
  pp::SimpleThread worker_;

  // Callback factory used to submit jobs to worker_.
  // See "Detailed Description" Note at:
  // https://developer.chrome.com/native-client/
  //     pepper_dev/cpp/classpp_1_1_completion_callback_factory
  //
  // As a minus this would require ugly synchronization between the main thread
  // and the function that is executed on worker_ construction. Current
  // implementation is simimlar to examples in $NACL_SDK_ROOT and according to
  // https://chromiumcodereview.appspot.com/lint_patch/issue10790078_24001_25013
  // it should be safe (see TODO(dmichael)). That's because both worker_ and
  // callback_factory_ will be alive during the life of Volume and deleting a
  // Volume is permitted only if there are no requests in progress on
  // JavaScript side (this means no Callbacks in progress).
  pp::CompletionCallbackFactory<Volume> callback_factory_;

  // Request ID of the current reader instance.
  std::string reader_request_id_;

  pp::Lock job_lock_;  // A lock for guarding members related to jobs.

  // A requestor for making calls to JavaScript.
  std::unique_ptr<JavaScriptRequestorInterface> requestor_;

  // A factory for creating VolumeArchive.
  std::unique_ptr<VolumeArchiveFactoryInterface> volume_archive_factory_;

  // A factory for creating VolumeReader.
  std::unique_ptr<VolumeReaderFactoryInterface> volume_reader_factory_;

  // A map that converts index of file in the volume to pathname.
  std::map<int, std::string> index_to_pathname_;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_H_
