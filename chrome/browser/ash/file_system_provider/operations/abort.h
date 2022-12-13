// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_ABORT_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_ABORT_H_

#include <memory>

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash {
namespace file_system_provider {
namespace operations {

// Aborts an operation. Created per request.
class Abort : public Operation {
 public:
  Abort(EventDispatcher* dispatcher,
        const ProvidedFileSystemInfo& file_system_info,
        int operation_request_id,
        storage::AsyncFileUtil::StatusCallback callback);

  Abort(const Abort&) = delete;
  Abort& operator=(const Abort&) = delete;

  ~Abort() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 std::unique_ptr<RequestValue> result,
                 bool has_more) override;
  void OnError(int request_id,
               std::unique_ptr<RequestValue> result,
               base::File::Error error) override;

 private:
  int operation_request_id_;
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_ABORT_H_
