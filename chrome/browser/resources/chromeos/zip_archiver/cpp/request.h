// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_REQUEST_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_REQUEST_H_

#include <cstdint>
#include <string>

#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"

// Defines the protocol messsage used to communicate between JS and NaCL.
// This must be consistent with the JS side js/request.js.
namespace request {

// Defines request keys. The keys should be unique and must be the same as the
// keys defined on the JS side.
namespace key {

// Mandatory keys for all unpacking requests.
const char kOperation[] = "operation";  // Should be a request::Operation.
const char kFileSystemId[] = "file_system_id";  // Should be a string.
const char kRequestId[] = "request_id";         // Should be a string.

// Optional keys unique to unpacking operations.
const char kMetadata[] = "metadata";  // Should be a pp:VarDictionary.
const char kArchiveSize[] =
    "archive_size";  // Should be a string as int64_t is not support by pp::Var.
const char kIndex[] = "index";        // Should be a string as int64_t is not
                                      // supported by pp::Var.
const char kEncoding[] = "encoding";  // Should be a string.
const char kOpenRequestId[] = "open_request_id";  // Should be a string, just
                                                  // like kRequestId.
const char kReadFileData[] = "read_file_data";    // Should be a
                                                  // pp::VarArrayBuffer.
const char kHasMoreData[] = "has_more_data";      // Should be a bool.
const char kPassphrase[] = "passphrase";          // Should be a string.

// Mandatory keys for all packing requests.
const char kCompressorId[] = "compressor_id";  // Should be an int.

// Optional keys unique to packing operations.
const char kEntryId[] = "entry_id";                    // Should be an int.
const char kPathname[] = "pathname";                   // Should be a string.
const char kFileSize[] = "file_size";                  // Should be a string.
const char kIsDirectory[] = "is_directory";            // Should be a bool.
// Local time in milliseconds since UNIX epoch, as a string.
const char kModificationTime[] = "modification_time";
const char kHasError[] = "has_error";                  // Should be a bool.

// Optional keys used for both packing and unpacking operations.
const char kError[] = "error";               // Should be a string.
const char kChunkBuffer[] = "chunk_buffer";  // Should be a pp::VarArrayBuffer.
const char kOffset[] = "offset";     // Should be a string as int64_t is not
                                     // supported by pp::Var.
const char kLength[] = "length";     // Should be a string as int64_t is not
                                     // supported by pp::Var.
const char kSrcFile[] = "src_file";  // Should be a string.
const char kSrcLine[] = "src_line";  // Should be a string.
const char kSrcFunc[] = "src_func";  // Should be a string.
const char kMessage[] = "message";   // Should be a string.
}  // namespace key

// Defines request operations. These operations must be the same as the
// operations defined on the JS side (js/request.js).
enum Operation {
  // Unpack operations.
  READ_METADATA = 0,
  READ_METADATA_DONE = 1,
  READ_CHUNK = 2,
  READ_CHUNK_DONE = 3,
  READ_CHUNK_ERROR = 4,
  READ_PASSPHRASE = 5,
  READ_PASSPHRASE_DONE = 6,
  READ_PASSPHRASE_ERROR = 7,
  CLOSE_VOLUME = 8,
  OPEN_FILE = 9,
  OPEN_FILE_DONE = 10,
  CLOSE_FILE = 11,
  CLOSE_FILE_DONE = 12,
  READ_FILE = 13,
  READ_FILE_DONE = 14,
  CONSOLE_LOG = 15,
  CONSOLE_DEBUG = 16,

  // Pack operations.
  CREATE_ARCHIVE = 17,
  CREATE_ARCHIVE_DONE = 18,
  ADD_TO_ARCHIVE = 19,
  ADD_TO_ARCHIVE_DONE = 20,
  READ_FILE_CHUNK = 21,
  READ_FILE_CHUNK_DONE = 22,
  WRITE_CHUNK = 23,
  WRITE_CHUNK_DONE = 24,
  CLOSE_ARCHIVE = 25,
  CLOSE_ARCHIVE_DONE = 26,
  CANCEL_ARCHIVE = 27,
  CANCEL_ARCHIVE_DONE = 28,
  RELEASE_COMPRESSOR = 29,

  // Errors.
  FILE_SYSTEM_ERROR = -1,  // Errors specific to a file system.
  COMPRESSOR_ERROR = -2    // Errors specific to a compressor.
};

// Operations greater than or equal to this value are for packing.
const int MINIMUM_PACK_REQUEST_VALUE = 17;

// Return true if the given operation is related to packing.
bool IsPackRequest(int operation);

// Creates a response to READ_METADATA request.
pp::VarDictionary CreateReadMetadataDoneResponse(
    const std::string& file_system_id,
    const std::string& request_id,
    const pp::VarDictionary& metadata);

// Creates a request for a file chunk from JavaScript.
pp::VarDictionary CreateReadChunkRequest(const std::string& file_system_id,
                                         const std::string& request_id,
                                         int64_t offset,
                                         int64_t length);

// Creates a request for a passphrase for a file from JavaScript.
pp::VarDictionary CreateReadPassphraseRequest(const std::string& file_system_id,
                                              const std::string& request_id);

// Creates a response to OPEN_FILE request.
pp::VarDictionary CreateOpenFileDoneResponse(const std::string& file_system_id,
                                             const std::string& request_id);

// Creates a response to CLOSE_FILE request.
pp::VarDictionary CreateCloseFileDoneResponse(
    const std::string& file_system_id,
    const std::string& request_id,
    const std::string& open_request_id);

// Creates a response to READ_FILE request.
pp::VarDictionary CreateReadFileDoneResponse(
    const std::string& file_system_id,
    const std::string& request_id,
    const pp::VarArrayBuffer& array_buffer,
    bool has_more_data);

pp::VarDictionary CreateCreateArchiveDoneResponse(int compressor_id);

pp::VarDictionary CreateReadFileChunkRequest(int compressor_id, int64_t length);

pp::VarDictionary CreateWriteChunkRequest(
    int compressor_id,
    const pp::VarArrayBuffer& array_buffer,
    int64_t offset,
    int64_t length);

pp::VarDictionary CreateAddToArchiveDoneResponse(int compressor_id);

pp::VarDictionary CreateCloseArchiveDoneResponse(int compressor_id);

pp::VarDictionary CreateCancelArchiveDoneResponse(int compressor_id);

// Creates a file system error.
pp::VarDictionary CreateFileSystemError(const std::string& file_system_id,
                                        const std::string& request_id,
                                        const std::string& error);

pp::VarDictionary CreateConsoleLog(const std::string& file_system_id,
                                   const std::string& request_id,
                                   const std::string& src_file,
                                   int src_line,
                                   const std::string& src_func,
                                   const std::string& message);

// Creates a compressor error.
pp::VarDictionary CreateCompressorError(int compressor_id,
                                        const std::string& error);

// Obtains a int64_t from a string value inside dictionary based on a
// request::Key.
int64_t GetInt64FromString(const pp::VarDictionary& dictionary,
                           const std::string& request_key);

}  // namespace request

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_REQUEST_H_
