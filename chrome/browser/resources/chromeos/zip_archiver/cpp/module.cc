// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <clocale>
#include <memory>
#include <sstream>
#include <utility>

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/javascript_message_sender_interface.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/request.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/utility/threading/lock.h"
#include "ppapi/utility/threading/simple_thread.h"

namespace {

typedef std::map<std::string, std::unique_ptr<Volume>>::const_iterator
    volume_iterator;
typedef std::map<int, std::unique_ptr<Compressor>>::const_iterator
    compressor_iterator;

// An internal implementation of JavaScriptMessageSenderInterface. This class
// handles all communication from the module to the JavaScript code. Thread
// safety is ensured only for PNaCl, not NaCl. See crbug.com/412692 and
// crbug.com/413513.
class JavaScriptMessageSender : public JavaScriptMessageSenderInterface {
 public:
  // JavaScriptMessageSender does not own the instance pointer.
  explicit JavaScriptMessageSender(pp::Instance* instance)
      : instance_(instance) {}

  virtual void SendFileSystemError(const std::string& file_system_id,
                                   const std::string& request_id,
                                   const std::string& message) {
    JavaScriptPostMessage(
        request::CreateFileSystemError(file_system_id, request_id, message));
  }

  virtual void SendCompressorError(int compressor_id,
                                   const std::string& message) {
    JavaScriptPostMessage(
        request::CreateCompressorError(compressor_id, message));
  }

  virtual void SendFileChunkRequest(const std::string& file_system_id,
                                    const std::string& request_id,
                                    int64_t offset,
                                    int64_t bytes_to_read) {
    PP_DCHECK(offset >= 0);
    PP_DCHECK(bytes_to_read > 0);
    JavaScriptPostMessage(request::CreateReadChunkRequest(
        file_system_id, request_id, offset, bytes_to_read));
  }

  virtual void SendPassphraseRequest(const std::string& file_system_id,
                                     const std::string& request_id) {
    JavaScriptPostMessage(
        request::CreateReadPassphraseRequest(file_system_id, request_id));
  }

  virtual void SendReadMetadataDone(const std::string& file_system_id,
                                    const std::string& request_id,
                                    const pp::VarDictionary& metadata) {
    JavaScriptPostMessage(request::CreateReadMetadataDoneResponse(
        file_system_id, request_id, metadata));
  }

  virtual void SendOpenFileDone(const std::string& file_system_id,
                                const std::string& request_id) {
    JavaScriptPostMessage(
        request::CreateOpenFileDoneResponse(file_system_id, request_id));
  }

  virtual void SendCloseFileDone(const std::string& file_system_id,
                                 const std::string& request_id,
                                 const std::string& open_request_id) {
    JavaScriptPostMessage(request::CreateCloseFileDoneResponse(
        file_system_id, request_id, open_request_id));
  }

  virtual void SendReadFileDone(const std::string& file_system_id,
                                const std::string& request_id,
                                const pp::VarArrayBuffer& array_buffer,
                                bool has_more_data) {
    JavaScriptPostMessage(request::CreateReadFileDoneResponse(
        file_system_id, request_id, array_buffer, has_more_data));
  }

  virtual void SendConsoleLog(const std::string& file_system_id,
                              const std::string& request_id,
                              const std::string& src_file,
                              int src_line,
                              const std::string& src_func,
                              const std::string& message) {
    JavaScriptPostMessage(request::CreateConsoleLog(
        file_system_id, request_id, src_file, src_line, src_func, message));
  }

  virtual void SendCreateArchiveDone(int compressor_id) {
    JavaScriptPostMessage(
        request::CreateCreateArchiveDoneResponse(compressor_id));
  }

  virtual void SendReadFileChunk(int compressor_id, int64_t length) {
    JavaScriptPostMessage(
        request::CreateReadFileChunkRequest(compressor_id, length));
  }

  virtual void SendWriteChunk(int compressor_id,
                              const pp::VarArrayBuffer& array_buffer,
                              int64_t offset,
                              int64_t length) {
    JavaScriptPostMessage(request::CreateWriteChunkRequest(
        compressor_id, array_buffer, offset, length));
  }

  virtual void SendAddToArchiveDone(int compressor_id) {
    JavaScriptPostMessage(
        request::CreateAddToArchiveDoneResponse(compressor_id));
  }

  virtual void SendCloseArchiveDone(int compressor_id) {
    JavaScriptPostMessage(
        request::CreateCloseArchiveDoneResponse(compressor_id));
  }

  virtual void SendCancelArchiveDone(int compressor_id) {
    JavaScriptPostMessage(
        request::CreateCancelArchiveDoneResponse(compressor_id));
  }

 private:
  // Posts a message to JavaScript. This is prone to races in case of using
  // NaCl instead of PNaCl. See crbug.com/413513.
  void JavaScriptPostMessage(const pp::VarDictionary& message) {
    instance_->PostMessage(message);
  }

  pp::Instance* instance_;
};

}  // namespace

// An instance for every "embed" in the web page. For this extension only one
// "embed" is necessary.
class NaclArchiveInstance : public pp::Instance {
 public:
  explicit NaclArchiveInstance(PP_Instance instance)
      : pp::Instance(instance),
        instance_handle_(instance),
        message_sender_(this) {}

  virtual ~NaclArchiveInstance() = default;

  // Handler for messages coming in from JS via postMessage().
  virtual void HandleMessage(const pp::Var& var_message) {
    PP_DCHECK(var_message.is_dictionary());
    pp::VarDictionary var_dict(var_message);

    PP_DCHECK(var_dict.Get(request::key::kOperation).is_int());
    int operation = var_dict.Get(request::key::kOperation).AsInt();

    if (request::IsPackRequest(operation))
      HandlePackMessage(var_dict, operation);
    else
      HandleUnpackMessage(var_dict, operation);
  }

 private:
  // Processes unpack messages.
  void HandleUnpackMessage(const pp::VarDictionary& var_dict,
                           const int operation) {
    PP_DCHECK(var_dict.Get(request::key::kFileSystemId).is_string());
    std::string file_system_id =
        var_dict.Get(request::key::kFileSystemId).AsString();

    PP_DCHECK(var_dict.Get(request::key::kRequestId).is_string());
    std::string request_id = var_dict.Get(request::key::kRequestId).AsString();

    // Processes operation.
    switch (operation) {
      case request::READ_METADATA: {
        ReadMetadata(var_dict, file_system_id, request_id);
        break;
      }

      case request::READ_CHUNK_DONE:
        ReadChunkDone(var_dict, file_system_id, request_id);
        break;

      case request::READ_CHUNK_ERROR:
        ReadChunkError(file_system_id, request_id);
        break;

      case request::READ_PASSPHRASE_DONE:
        ReadPassphraseDone(var_dict, file_system_id, request_id);
        break;

      case request::READ_PASSPHRASE_ERROR:
        ReadPassphraseError(file_system_id, request_id);
        break;

      case request::OPEN_FILE:
        OpenFile(var_dict, file_system_id, request_id);
        break;

      case request::CLOSE_FILE:
        CloseFile(var_dict, file_system_id, request_id);
        break;

      case request::READ_FILE:
        ReadFile(var_dict, file_system_id, request_id);
        break;

      case request::CLOSE_VOLUME: {
        PP_DCHECK(volumes_.find(file_system_id) != volumes_.end());
        volumes_.erase(file_system_id);
        break;
      }

      default:
        PP_NOTREACHED();
    }
  }

  // Processes pack messages.
  void HandlePackMessage(const pp::VarDictionary& var_dict,
                         const int operation) {
    PP_DCHECK(var_dict.Get(request::key::kCompressorId).is_int());
    int compressor_id = var_dict.Get(request::key::kCompressorId).AsInt();

    switch (operation) {
      case request::CREATE_ARCHIVE: {
        CreateArchive(compressor_id);
        break;
      }

      case request::ADD_TO_ARCHIVE: {
        AddToArchive(var_dict, compressor_id);
        break;
      }

      case request::READ_FILE_CHUNK_DONE: {
        ReadFileChunkDone(var_dict, compressor_id);
        break;
      }

      case request::WRITE_CHUNK_DONE: {
        WriteChunkDone(var_dict, compressor_id);
        break;
      }

      case request::CLOSE_ARCHIVE: {
        CloseArchive(var_dict, compressor_id);
        break;
      }

      case request::CANCEL_ARCHIVE: {
        CancelArchive(var_dict, compressor_id);
        break;
      }

      case request::RELEASE_COMPRESSOR: {
        ReleaseCompressor(compressor_id);
        break;
      }

      default:
        PP_NOTREACHED();
    }
  }

  // Reads the metadata for the corresponding volume for file_system_id. This
  // should be called only once and before any other operation like OpenFile,
  // ReadFile, etc.
  // Reading metadata or opening a file could work even if the Volume exists
  // or not, but as the JavaScript code doesn't use this feature there is no
  // reason to allow it. If the logic on JavaScript changes then this can be
  // updated. But in current design if we read metadata for an existing Volume,
  // then there is a programmer error on JavaScript side.
  void ReadMetadata(const pp::VarDictionary& var_dict,
                    const std::string& file_system_id,
                    const std::string& request_id) {
    // Should not call ReadMetadata for a Volume already present in NaCl.
    PP_DCHECK(volumes_.find(file_system_id) == volumes_.end());

    std::unique_ptr<Volume> volume = std::make_unique<Volume>(
        instance_handle_, file_system_id, &message_sender_);
    if (!volume->Init()) {
      message_sender_.SendFileSystemError(
          file_system_id, request_id,
          "Could not create a volume for: " + file_system_id + ".");
      return;
    }
    Volume* raw_volume = volume.get();
    volumes_[file_system_id] = std::move(volume);

    PP_DCHECK(var_dict.Get(request::key::kEncoding).is_string());
    PP_DCHECK(var_dict.Get(request::key::kArchiveSize).is_string());

    raw_volume->ReadMetadata(
        request_id, var_dict.Get(request::key::kEncoding).AsString(),
        request::GetInt64FromString(var_dict, request::key::kArchiveSize));
  }

  void ReadChunkDone(const pp::VarDictionary& var_dict,
                     const std::string& file_system_id,
                     const std::string& request_id) {
    PP_DCHECK(var_dict.Get(request::key::kChunkBuffer).is_array_buffer());
    pp::VarArrayBuffer array_buffer(var_dict.Get(request::key::kChunkBuffer));

    PP_DCHECK(var_dict.Get(request::key::kOffset).is_string());
    int64_t read_offset =
        request::GetInt64FromString(var_dict, request::key::kOffset);

    volume_iterator iterator = volumes_.find(file_system_id);
    // Volume was unmounted so ignore the read chunk operation.
    // Possible scenario for read ahead.
    if (iterator == volumes_.end())
      return;
    iterator->second->ReadChunkDone(request_id, array_buffer, read_offset);
  }

  void ReadChunkError(const std::string& file_system_id,
                      const std::string& request_id) {
    volume_iterator iterator = volumes_.find(file_system_id);
    // Volume was unmounted so ignore the read chunk operation.
    // Possible scenario for read ahead.
    if (iterator == volumes_.end())
      return;
    iterator->second->ReadChunkError(request_id);
  }

  void ReadPassphraseDone(const pp::VarDictionary& var_dict,
                          const std::string& file_system_id,
                          const std::string& request_id) {
    PP_DCHECK(var_dict.Get(request::key::kPassphrase).is_string());
    std::string passphrase(var_dict.Get(request::key::kPassphrase).AsString());

    volume_iterator iterator = volumes_.find(file_system_id);
    // Volume was unmounted so ignore the read passphrase operation.
    if (iterator == volumes_.end())
      return;
    iterator->second->ReadPassphraseDone(request_id, passphrase);
  }

  void ReadPassphraseError(const std::string& file_system_id,
                           const std::string& request_id) {
    volume_iterator iterator = volumes_.find(file_system_id);
    // Volume was unmounted so ignore the read chunk operation.
    if (iterator == volumes_.end())
      return;
    iterator->second->ReadPassphraseError(request_id);
  }

  void OpenFile(const pp::VarDictionary& var_dict,
                const std::string& file_system_id,
                const std::string& request_id) {
    PP_DCHECK(var_dict.Get(request::key::kIndex).is_string());
    int64_t index = request::GetInt64FromString(var_dict, request::key::kIndex);

    PP_DCHECK(var_dict.Get(request::key::kEncoding).is_string());
    std::string encoding(var_dict.Get(request::key::kEncoding).AsString());

    PP_DCHECK(var_dict.Get(request::key::kArchiveSize).is_string());
    int64_t archive_size =
        request::GetInt64FromString(var_dict, request::key::kArchiveSize);

    volume_iterator iterator = volumes_.find(file_system_id);
    PP_DCHECK(iterator != volumes_.end());  // Should call OpenFile after
                                            // ReadMetadata.
    iterator->second->OpenFile(request_id, index, encoding, archive_size);
  }

  void CloseFile(const pp::VarDictionary& var_dict,
                 const std::string& file_system_id,
                 const std::string& request_id) {
    PP_DCHECK(var_dict.Get(request::key::kOpenRequestId).is_string());
    std::string open_request_id(
        var_dict.Get(request::key::kOpenRequestId).AsString());

    volume_iterator iterator = volumes_.find(file_system_id);
    PP_DCHECK(iterator !=
              volumes_.end());  // Should call CloseFile after OpenFile.

    iterator->second->CloseFile(request_id, open_request_id);
  }

  void ReadFile(const pp::VarDictionary& var_dict,
                const std::string& file_system_id,
                const std::string& request_id) {
    PP_DCHECK(var_dict.Get(request::key::kOpenRequestId).is_string());
    PP_DCHECK(var_dict.Get(request::key::kOffset).is_string());
    PP_DCHECK(var_dict.Get(request::key::kLength).is_string());

    volume_iterator iterator = volumes_.find(file_system_id);
    PP_DCHECK(iterator !=
              volumes_.end());  // Should call ReadFile after OpenFile.

    // Passing the entire dictionary because pp::CompletionCallbackFactory
    // cannot create callbacks with more than 3 parameters. Here we need 4:
    // request_id, open_request_id, offset and length.
    iterator->second->ReadFile(request_id, var_dict);
  }

  // Requests minizip to create an archive object for the given compressor_id.
  void CreateArchive(int compressor_id) {
    std::unique_ptr<Compressor> compressor(
        new Compressor(instance_handle_, compressor_id, &message_sender_));
    if (!compressor->Init()) {
      std::stringstream ss;
      ss << compressor_id;
      message_sender_.SendCompressorError(
          compressor_id,
          "Could not create a compressor for compressor id: " + ss.str() + ".");
      return;
    }

    compressors_[compressor_id] = std::move(compressor);
    compressors_[compressor_id]->CreateArchive();
  }

  void AddToArchive(const pp::VarDictionary& var_dict, int compressor_id) {
    compressor_iterator iterator = compressors_.find(compressor_id);
    PP_DCHECK(iterator != compressors_.end());

    iterator->second->AddToArchive(var_dict);
  }

  void ReadFileChunkDone(const pp::VarDictionary& var_dict,
                         const int compressor_id) {
    compressor_iterator iterator = compressors_.find(compressor_id);
    PP_DCHECK(iterator != compressors_.end());

    iterator->second->ReadFileChunkDone(var_dict);
  }

  void WriteChunkDone(const pp::VarDictionary& var_dict, int compressor_id) {
    compressor_iterator iterator = compressors_.find(compressor_id);
    PP_DCHECK(iterator != compressors_.end());

    iterator->second->WriteChunkDone(var_dict);
  }

  void CloseArchive(const pp::VarDictionary& var_dict, int compressor_id) {
    compressor_iterator iterator = compressors_.find(compressor_id);

    if (iterator != compressors_.end())
      iterator->second->CloseArchive(var_dict);
  }

  void CancelArchive(const pp::VarDictionary& var_dict, int compressor_id) {
    compressor_iterator iterator = compressors_.find(compressor_id);

    if (iterator != compressors_.end())
      iterator->second->CancelArchive(var_dict);
  }

  void ReleaseCompressor(int compressor_id) {
    compressors_.erase(compressor_id);
  }

  // A map that holds for every opened archive its instance. The key is the file
  // system id of the archive.
  std::map<std::string, std::unique_ptr<Volume>> volumes_;

  // A map from compressor ids to compressors.
  std::map<int, std::unique_ptr<Compressor>> compressors_;

  // An pp::InstanceHandle used to create pp::SimpleThread in Volume.
  pp::InstanceHandle instance_handle_;

  // An object used to send messages to JavaScript.
  JavaScriptMessageSender message_sender_;
};

// The Module class. The browser calls the CreateInstance() method to create
// an instance of your NaCl module on the web page. The browser creates a new
// instance for each <embed> tag with type="application/x-pnacl" or
// type="application/x-nacl".
class NaclArchiveModule : public pp::Module {
 public:
  NaclArchiveModule() : pp::Module() {}
  virtual ~NaclArchiveModule() {}

  // Create and return a NaclArchiveInstance object.
  // @param[in] instance The browser-side instance.
  // @return the plugin-side instance.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new NaclArchiveInstance(instance);
  }
};

namespace pp {

// Factory function called by the browser when the module is first loaded.
// The browser keeps a singleton of this module.  It calls the
// CreateInstance() method on the object you return to make instances.  There
// is one instance per <embed> tag on the page.  This is the main binding
// point for your NaCl module with the browser.
Module* CreateModule() {
  std::setlocale(LC_ALL, "en_US.UTF-8");
  return new NaclArchiveModule();
}

}  // namespace pp
