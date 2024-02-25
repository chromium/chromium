// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_DELETE_ENTRY_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_DELETE_ENTRY_H_

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "storage/browser/file_system/async_file_util.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash::file_system_provider::operations {

// Deletes an entry. If |recursive| is passed and the entry is a directory, then
// all contents of it (recursively) will be deleted too. Created per request.
class DeleteEntry : public Operation {
 public:
  DeleteEntry(RequestDispatcher* dispatcher,
              const ProvidedFileSystemInfo& file_system_info,
              const base::FilePath& entry_path,
              bool recursive,
              storage::AsyncFileUtil::StatusCallback callback);

  DeleteEntry(const DeleteEntry&) = delete;
  DeleteEntry& operator=(const DeleteEntry&) = delete;

  ~DeleteEntry() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  base::FilePath entry_path_;
  bool recursive_;
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_DELETE_ENTRY_H_
