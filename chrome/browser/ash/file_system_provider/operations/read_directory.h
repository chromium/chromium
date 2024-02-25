// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_READ_DIRECTORY_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_READ_DIRECTORY_H_

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "storage/browser/file_system/async_file_util.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash::file_system_provider::operations {

// Bridge between fileapi read directory operation and providing extension's
// read directory request. Created per request.
class ReadDirectory : public Operation {
 public:
  ReadDirectory(RequestDispatcher* dispatcher,
                const ProvidedFileSystemInfo& file_system_info,
                const base::FilePath& directory_path,
                storage::AsyncFileUtil::ReadDirectoryCallback callback);

  ReadDirectory(const ReadDirectory&) = delete;
  ReadDirectory& operator=(const ReadDirectory&) = delete;

  ~ReadDirectory() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  base::FilePath directory_path_;
  const storage::AsyncFileUtil::ReadDirectoryCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_READ_DIRECTORY_H_
