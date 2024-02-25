// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_GET_ACTIONS_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_GET_ACTIONS_H_

#include <vector>

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash::file_system_provider::operations {

// Bridge between fileapi get actions operation and providing extension's get
// actions request. Created per request.
class GetActions : public Operation {
 public:
  GetActions(RequestDispatcher* dispatcher,
             const ProvidedFileSystemInfo& file_system_info,
             const std::vector<base::FilePath>& entry_paths,
             ProvidedFileSystemInterface::GetActionsCallback callback);

  GetActions(const GetActions&) = delete;
  GetActions& operator=(const GetActions&) = delete;

  ~GetActions() override;

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
  ProvidedFileSystemInterface::GetActionsCallback callback_;
};

}  // namespace ash::file_system_provider::operations

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_GET_ACTIONS_H_
