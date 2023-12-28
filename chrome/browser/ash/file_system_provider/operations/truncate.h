// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_TRUNCATE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_TRUNCATE_H_

#include <stdint.h>

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

// Creates a file. If the file already exists, then the operation will fail with
// the FILE_ERROR_EXISTS error. Created per request.
class Truncate : public Operation {
 public:
  Truncate(RequestDispatcher* dispatcher,
           const ProvidedFileSystemInfo& file_system_info,
           const base::FilePath& file_path,
           int64_t length,
           storage::AsyncFileUtil::StatusCallback callback);

  Truncate(const Truncate&) = delete;
  Truncate& operator=(const Truncate&) = delete;

  ~Truncate() override;

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
  int64_t length_;
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_TRUNCATE_H_
