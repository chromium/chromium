// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_EXECUTE_ACTION_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_EXECUTE_ACTION_H_

#include <string>
#include <vector>

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "storage/browser/file_system/async_file_util.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash::file_system_provider::operations {

// Bridge between chrome.fileManagerPrivate.executeCustomAction operation and
// the providing extension's onExecuteActionRequested event. Created per
// request.
class ExecuteAction : public Operation {
 public:
  ExecuteAction(RequestDispatcher* dispatcher,
                const ProvidedFileSystemInfo& file_system_info,
                const std::vector<base::FilePath>& entry_path,
                const std::string& action_id,
                storage::AsyncFileUtil::StatusCallback callback);

  ExecuteAction(const ExecuteAction&) = delete;
  ExecuteAction& operator=(const ExecuteAction&) = delete;

  ~ExecuteAction() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  const std::vector<base::FilePath> entry_paths_;
  const std::string action_id_;
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_EXECUTE_ACTION_H_
