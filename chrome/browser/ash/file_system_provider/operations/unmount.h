// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_UNMOUNT_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_UNMOUNT_H_

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/operations/operation.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash::file_system_provider {

class ProvidedFileSystemInfo;

namespace operations {

// Bridge between fileManagerPrivate's unmount operation and providing
// extension's unmount request. Created per request.
class Unmount : public Operation {
 public:
  Unmount(RequestDispatcher* dispatcher,
          const ProvidedFileSystemInfo& file_system_info,
          storage::AsyncFileUtil::StatusCallback callback);

  Unmount(const Unmount&) = delete;
  Unmount& operator=(const Unmount&) = delete;

  ~Unmount() override;

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

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_UNMOUNT_H_
