// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_READ_FILE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_READ_FILE_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash::file_system_provider::operations {

// Bridge between fileapi read file and providing extension's read fil request.
// Created per request.
class ReadFile : public Operation {
 public:
  ReadFile(RequestDispatcher* dispatcher,
           const ProvidedFileSystemInfo& file_system_info,
           int file_handle,
           scoped_refptr<net::IOBuffer> buffer,
           int64_t offset,
           int length,
           ProvidedFileSystemInterface::ReadChunkReceivedCallback callback);

  ReadFile(const ReadFile&) = delete;
  ReadFile& operator=(const ReadFile&) = delete;

  ~ReadFile() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  int file_handle_;
  scoped_refptr<net::IOBuffer> buffer_;
  int64_t offset_;
  int length_;
  int64_t current_offset_;
  ProvidedFileSystemInterface::ReadChunkReceivedCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_READ_FILE_H_
