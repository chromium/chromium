// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_MESSAGE_SENDER_INTERFACE_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_MESSAGE_SENDER_INTERFACE_H_

#include <cstdint>
#include <string>

#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"

// Creates and sends messages to JavaScript. Messages are send asynchronously.
class JavaScriptMessageSenderInterface {
 public:
  virtual ~JavaScriptMessageSenderInterface() {}

  virtual void SendFileSystemError(const std::string& file_system_id,
                                   const std::string& request_id,
                                   const std::string& message) = 0;

  virtual void SendCompressorError(int compressor_id,
                                   const std::string& message) = 0;

  virtual void SendFileChunkRequest(const std::string& file_system_id,
                                    const std::string& request_id,
                                    int64_t offset,
                                    int64_t bytes_to_read) = 0;

  virtual void SendPassphraseRequest(const std::string& file_system_id,
                                     const std::string& request_id) = 0;

  virtual void SendReadMetadataDone(const std::string& file_system_id,
                                    const std::string& request_id,
                                    const pp::VarDictionary& metadata) = 0;

  virtual void SendOpenFileDone(const std::string& file_system_id,
                                const std::string& request_id) = 0;

  virtual void SendCloseFileDone(const std::string& file_system_id,
                                 const std::string& request_id,
                                 const std::string& open_request_id) = 0;

  virtual void SendReadFileDone(const std::string& file_system_id,
                                const std::string& request_id,
                                const pp::VarArrayBuffer& array_buffer,
                                bool has_more_data) = 0;

  virtual void SendConsoleLog(const std::string& file_system_id,
                              const std::string& request_id,
                              const std::string& src_file,
                              int src_line,
                              const std::string& src_func,
                              const std::string& message) = 0;

  virtual void SendCreateArchiveDone(int compressor_id) = 0;

  virtual void SendReadFileChunk(int compressor_id_, int64_t file_size) = 0;

  virtual void SendWriteChunk(int compressor_id,
                              const pp::VarArrayBuffer& array_buffer,
                              int64_t offset,
                              int64_t length) = 0;

  virtual void SendAddToArchiveDone(int compressor_id) = 0;

  virtual void SendCloseArchiveDone(int compressor_id) = 0;

  virtual void SendCancelArchiveDone(int compressor_id) = 0;
};

#define CONSOLE_LOG(fsid, rid, msg) \
  SendConsoleLog(fsid, rid, __FILE__, __LINE__, __func__, msg)

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_MESSAGE_SENDER_INTERFACE_H_
