// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/request.h"

#include <sstream>

namespace {

// Creates a basic request with the mandatory fields.
pp::VarDictionary CreateBasicRequest(const int operation,
                                     const std::string& file_system_id,
                                     const std::string& request_id) {
  pp::VarDictionary request;
  request.Set(request::key::kOperation, operation);
  request.Set(request::key::kFileSystemId, file_system_id);
  request.Set(request::key::kRequestId, request_id);
  return request;
}

}  // namespace

// Return true if the given operation is related to packing.
bool request::IsPackRequest(int operation) {
  return request::MINIMUM_PACK_REQUEST_VALUE <= operation ||
         operation == request::COMPRESSOR_ERROR;
}

pp::VarDictionary request::CreateReadMetadataDoneResponse(
    const std::string& file_system_id,
    const std::string& request_id,
    const pp::VarDictionary& metadata) {
  pp::VarDictionary response =
      CreateBasicRequest(READ_METADATA_DONE, file_system_id, request_id);
  response.Set(request::key::kMetadata, metadata);
  return response;
}

pp::VarDictionary request::CreateReadChunkRequest(
    const std::string& file_system_id,
    const std::string& request_id,
    int64_t offset,
    int64_t length) {
  pp::VarDictionary request =
      CreateBasicRequest(READ_CHUNK, file_system_id, request_id);

  std::stringstream ss_offset;
  ss_offset << offset;
  request.Set(request::key::kOffset, ss_offset.str());

  std::stringstream ss_length;
  ss_length << length;
  request.Set(request::key::kLength, ss_length.str());
  return request;
}

pp::VarDictionary request::CreateReadPassphraseRequest(
    const std::string& file_system_id,
    const std::string& request_id) {
  pp::VarDictionary request =
      CreateBasicRequest(READ_PASSPHRASE, file_system_id, request_id);
  return request;
}

pp::VarDictionary request::CreateOpenFileDoneResponse(
    const std::string& file_system_id,
    const std::string& request_id) {
  return CreateBasicRequest(OPEN_FILE_DONE, file_system_id, request_id);
}

pp::VarDictionary request::CreateCloseFileDoneResponse(
    const std::string& file_system_id,
    const std::string& request_id,
    const std::string& open_request_id) {
  pp::VarDictionary response =
      CreateBasicRequest(CLOSE_FILE_DONE, file_system_id, request_id);
  response.Set(request::key::kOpenRequestId, open_request_id);
  return response;
}

pp::VarDictionary request::CreateReadFileDoneResponse(
    const std::string& file_system_id,
    const std::string& request_id,
    const pp::VarArrayBuffer& array_buffer,
    bool has_more_data) {
  pp::VarDictionary response =
      CreateBasicRequest(READ_FILE_DONE, file_system_id, request_id);
  response.Set(request::key::kReadFileData, array_buffer);
  response.Set(request::key::kHasMoreData, has_more_data);
  return response;
}

pp::VarDictionary request::CreateCreateArchiveDoneResponse(
    const int compressor_id) {
  pp::VarDictionary request;
  request.Set(request::key::kOperation, CREATE_ARCHIVE_DONE);
  request.Set(request::key::kCompressorId, compressor_id);
  return request;
}

pp::VarDictionary request::CreateReadFileChunkRequest(const int compressor_id,
                                                      const int64_t length) {
  pp::VarDictionary request;
  request.Set(request::key::kOperation, READ_FILE_CHUNK);
  request.Set(request::key::kCompressorId, compressor_id);

  std::stringstream ss_length;
  ss_length << length;
  request.Set(request::key::kLength, ss_length.str());
  return request;
}

pp::VarDictionary request::CreateWriteChunkRequest(
    int compressor_id,
    const pp::VarArrayBuffer& array_buffer,
    int64_t offset,
    int64_t length) {
  pp::VarDictionary request;
  request.Set(request::key::kOperation, WRITE_CHUNK);
  request.Set(request::key::kCompressorId, compressor_id);

  request.Set(request::key::kChunkBuffer, array_buffer);

  std::stringstream ss_offset;
  ss_offset << offset;
  request.Set(request::key::kOffset, ss_offset.str());
  std::stringstream ss_length;
  ss_length << length;
  request.Set(request::key::kLength, ss_length.str());
  return request;
}

pp::VarDictionary request::CreateAddToArchiveDoneResponse(int compressor_id) {
  pp::VarDictionary request;
  request.Set(request::key::kOperation, ADD_TO_ARCHIVE_DONE);
  request.Set(request::key::kCompressorId, compressor_id);
  return request;
}

pp::VarDictionary request::CreateCloseArchiveDoneResponse(int compressor_id) {
  pp::VarDictionary request;
  request.Set(request::key::kOperation, CLOSE_ARCHIVE_DONE);
  request.Set(request::key::kCompressorId, compressor_id);
  return request;
}

pp::VarDictionary request::CreateCancelArchiveDoneResponse(int compressor_id) {
  pp::VarDictionary request;
  request.Set(request::key::kOperation, CANCEL_ARCHIVE_DONE);
  request.Set(request::key::kCompressorId, compressor_id);
  return request;
}

pp::VarDictionary request::CreateFileSystemError(
    const std::string& file_system_id,
    const std::string& request_id,
    const std::string& error) {
  pp::VarDictionary request =
      CreateBasicRequest(FILE_SYSTEM_ERROR, file_system_id, request_id);
  request.Set(request::key::kError, error);
  return request;
}

pp::VarDictionary request::CreateConsoleLog(const std::string& file_system_id,
                                            const std::string& request_id,
                                            const std::string& src_file,
                                            int src_line,
                                            const std::string& src_func,
                                            const std::string& message) {
  pp::VarDictionary request =
      CreateBasicRequest(CONSOLE_LOG, file_system_id, request_id);
  request.Set(request::key::kSrcFile, src_file);
  request.Set(request::key::kSrcLine, src_line);
  request.Set(request::key::kSrcFunc, src_func);
  request.Set(request::key::kMessage, message);
  return request;
}

pp::VarDictionary request::CreateCompressorError(int compressor_id,
                                                 const std::string& error) {
  pp::VarDictionary request;
  request.Set(request::key::kOperation, COMPRESSOR_ERROR);
  request.Set(request::key::kCompressorId, compressor_id);
  request.Set(request::key::kError, error);
  return request;
}

int64_t request::GetInt64FromString(const pp::VarDictionary& dictionary,
                                    const std::string& request_key) {
  std::stringstream ss_int64(dictionary.Get(request_key).AsString());
  int64_t int64_value;
  ss_int64 >> int64_value;
  return int64_value;
}
