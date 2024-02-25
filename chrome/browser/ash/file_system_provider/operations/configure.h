// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CONFIGURE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CONFIGURE_H_

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash::file_system_provider {

class ProvidedFileSystemInfo;

namespace operations {

// Bridge between fileManagerPrivate's configure operation and providing
// extension's configure request. Created per request.
class Configure : public Operation {
 public:
  Configure(RequestDispatcher* dispatcher,
            const ProvidedFileSystemInfo& file_system_info,
            storage::AsyncFileUtil::StatusCallback callback);

  Configure(const Configure&) = delete;
  Configure& operator=(const Configure&) = delete;

  ~Configure() override;

  // Operation overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;

 private:
  storage::AsyncFileUtil::StatusCallback callback_;
};

}  // namespace operations
}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_CONFIGURE_H_
