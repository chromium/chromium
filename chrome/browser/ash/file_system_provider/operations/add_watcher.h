// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_ADD_WATCHER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_ADD_WATCHER_H_

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

// Adds a watcher. If |recursive| is true, than also watches for all of the
// child entries in within, recursively. Recursive must not be set to true for
// files.
class AddWatcher : public Operation {
 public:
  AddWatcher(RequestDispatcher* dispatcher,
             const ProvidedFileSystemInfo& file_system_info,
             const base::FilePath& entry_path,
             bool recursive,
             storage::AsyncFileUtil::StatusCallback callback);

  AddWatcher(const AddWatcher&) = delete;
  AddWatcher& operator=(const AddWatcher&) = delete;

  ~AddWatcher() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  const base::FilePath entry_path_;
  const bool recursive_;
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_ADD_WATCHER_H_
