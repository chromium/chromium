// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_OPEN_FILE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_OPEN_FILE_H_

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "storage/browser/file_system/async_file_util.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash::file_system_provider::operations {

// Opens a file for either read or write. The file must exist, otherwise the
// operation will fail. Created per request.
class OpenFile : public Operation {
 public:
  OpenFile(RequestDispatcher* dispatcher,
           const ProvidedFileSystemInfo& file_system_info,
           const base::FilePath& file_path,
           OpenFileMode mode,
           ProvidedFileSystemInterface::OpenFileCallback callback);

  OpenFile(const OpenFile&) = delete;
  OpenFile& operator=(const OpenFile&) = delete;

  ~OpenFile() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  base::FilePath file_path_;
  OpenFileMode mode_;
  ProvidedFileSystemInterface::OpenFileCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_OPEN_FILE_H_
