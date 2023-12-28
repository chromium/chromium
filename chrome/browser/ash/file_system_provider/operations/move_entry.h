// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_MOVE_ENTRY_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_MOVE_ENTRY_H_

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

// Copies an entry (recursively if a directory). Created per request.
class MoveEntry : public Operation {
 public:
  MoveEntry(RequestDispatcher* dispatcher,
            const ProvidedFileSystemInfo& file_system_info,
            const base::FilePath& source_path,
            const base::FilePath& target_path,
            storage::AsyncFileUtil::StatusCallback callback);

  MoveEntry(const MoveEntry&) = delete;
  MoveEntry& operator=(const MoveEntry&) = delete;

  ~MoveEntry() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  base::FilePath source_path_;
  base::FilePath target_path_;
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_MOVE_ENTRY_H_
