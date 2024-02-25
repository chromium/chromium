// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CLOSE_FILE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CLOSE_FILE_H_

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash::file_system_provider::operations {

// Opens a file for either read or write, with optionally creating the file
// first. Note, that this is part of storage::CreateOrOpen file, but it does
// not download the file locally. Created per request.
class CloseFile : public Operation {
 public:
  CloseFile(RequestDispatcher* dispatcher,
            const ProvidedFileSystemInfo& file_system_info,
            int open_request_id,
            storage::AsyncFileUtil::StatusCallback callback);

  CloseFile(const CloseFile&) = delete;
  CloseFile& operator=(const CloseFile&) = delete;

  ~CloseFile() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  int open_request_id_;
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CLOSE_FILE_H_
