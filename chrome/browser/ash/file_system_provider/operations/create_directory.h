// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CREATE_DIRECTORY_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CREATE_DIRECTORY_H_

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "storage/browser/file_system/async_file_util.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash::file_system_provider::operations {

// Creates a directory. If |recursive| is set to true, then creates also all
// non-existing directories on the path. The operation will fail if the
// directory already exists. Created per request.
class CreateDirectory : public Operation {
 public:
  CreateDirectory(RequestDispatcher* dispatcher,
                  const ProvidedFileSystemInfo& file_system_info,
                  const base::FilePath& directory_path,
                  bool recursive,
                  storage::AsyncFileUtil::StatusCallback callback);

  CreateDirectory(const CreateDirectory&) = delete;
  CreateDirectory& operator=(const CreateDirectory&) = delete;

  ~CreateDirectory() override;

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
  bool recursive_;
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CREATE_DIRECTORY_H_
