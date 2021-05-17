// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_IMPL_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager.h"
#include "chrome/browser/ash/borealis/infra/expected.h"

namespace borealis {
// Amount of space, in bytes, that borealis needs to leave free on the host.
extern const int64_t kTargetBufferBytes;
// Amount of space, in bytes, that should be left available on the VM disk.
extern const int64_t kDiskHeadroomBytes;

// Service responsible for managing borealis' disk space.
class BorealisDiskManagerImpl : public BorealisDiskManager {
 public:
  // Provider for returning the number of free bytes left on the host device.
  class FreeSpaceProvider {
   public:
    FreeSpaceProvider() = default;
    virtual ~FreeSpaceProvider() = default;
    virtual void Get(base::OnceCallback<void(int64_t)> callback);
  };

  explicit BorealisDiskManagerImpl(const BorealisContext* context);
  BorealisDiskManagerImpl(const BorealisDiskManagerImpl&) = delete;
  BorealisDiskManagerImpl& operator=(const BorealisDiskManagerImpl&) = delete;
  ~BorealisDiskManagerImpl() override;

  // TODO(174592560): add more explicit error handling when metrics are
  // introduced.
  void GetDiskInfo(
      base::OnceCallback<void(Expected<GetDiskInfoResponse, std::string>)>
          callback) override;
  void RequestSpace(uint64_t bytes_requested,
                    base::OnceCallback<void(Expected<uint64_t, std::string>)>
                        callback) override {}
  void ReleaseSpace(uint64_t bytes_to_release,
                    base::OnceCallback<void(Expected<uint64_t, std::string>)>
                        callback) override {}

  void SetFreeSpaceProviderForTesting(
      std::unique_ptr<FreeSpaceProvider> provider) {
    free_space_provider_ = std::move(provider);
  }

 private:
  // Struct for storing information about the VM's disk (space is considered in
  // bytes).
  struct BorealisDiskInfo;

  // Transition class representing the process of getting information about the
  // host and VM disk. Returns an error (string) or a result (BorealisDiskInfo).
  class BuildDiskInfo;

  // Handles the results of a GetDiskInfo request.
  void BuildGetDiskInfoResponse(
      base::OnceCallback<void(Expected<GetDiskInfoResponse, std::string>)>
          callback,
      Expected<std::unique_ptr<BorealisDiskInfo>, std::string>
          disk_info_or_error);

  std::unique_ptr<BuildDiskInfo> build_disk_info_transition_;
  const BorealisContext* const context_;
  std::unique_ptr<FreeSpaceProvider> free_space_provider_;
  base::WeakPtrFactory<BorealisDiskManagerImpl> weak_factory_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_IMPL_H_
