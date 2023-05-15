// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_DISPATCHER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_DISPATCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager.h"

namespace borealis {

// This service dispatches work to the disk manager owned by the running
// Borealis context (if any). Other services are able to access this service
// (BorealisDiskManagerDispatcher) in order to dispatch commands directly to the
// disk manager (which is created at run-time and doesn't live forever). If no
// disk manager/context exists, the dispatcher will return an error.
class BorealisDiskManagerDispatcher {
 public:
  BorealisDiskManagerDispatcher();
  virtual ~BorealisDiskManagerDispatcher() = default;

  virtual void GetDiskInfo(
      const std::string& origin_vm_name,
      const std::string& origin_container_name,
      base::OnceCallback<
          void(base::expected<BorealisDiskManager::GetDiskInfoResponse,
                              Described<BorealisGetDiskInfoResult>>)> callback);

  virtual void RequestSpace(
      const std::string& origin_vm_name,
      const std::string& origin_container_name,
      uint64_t bytes_requested,
      base::OnceCallback<
          void(base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>
          callback);

  virtual void ReleaseSpace(
      const std::string& origin_vm_name,
      const std::string& origin_container_name,
      uint64_t bytes_to_release,
      base::OnceCallback<
          void(base::expected<uint64_t, Described<BorealisResizeDiskResult>>)>
          callback);

  virtual void SetDiskManagerDelegate(BorealisDiskManager* disk_manager);
  virtual void RemoveDiskManagerDelegate(BorealisDiskManager* disk_manager);

 private:
  // Verifies the origin of the request and checks that a delegate exists.
  std::string ValidateRequest(const std::string& origin_vm_name,
                              const std::string& origin_container_name);

  // Not owned by us.
  raw_ptr<BorealisDiskManager, ExperimentalAsh> disk_manager_delegate_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_DISPATCHER_H_
