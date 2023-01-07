// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager.h"
#include "chrome/browser/ash/borealis/borealis_service.h"

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
    virtual void Get(
        base::OnceCallback<void(absl::optional<int64_t>)> callback);
  };

  explicit BorealisDiskManagerImpl(const BorealisContext* context);
  BorealisDiskManagerImpl(const BorealisDiskManagerImpl&) = delete;
  BorealisDiskManagerImpl& operator=(const BorealisDiskManagerImpl&) = delete;
  ~BorealisDiskManagerImpl() override;

  void GetDiskInfo(
      base::OnceCallback<void(
          Expected<GetDiskInfoResponse, Described<BorealisGetDiskInfoResult>>)>
          callback) override;
  void RequestSpace(
      uint64_t bytes_requested,
      base::OnceCallback<void(
          Expected<uint64_t, Described<BorealisResizeDiskResult>>)> callback)
      override;
  void ReleaseSpace(
      uint64_t bytes_to_release,
      base::OnceCallback<void(
          Expected<uint64_t, Described<BorealisResizeDiskResult>>)> callback)
      override;
  void SyncDiskSize(
      base::OnceCallback<void(Expected<BorealisSyncDiskSizeResult,
                                       Described<BorealisSyncDiskSizeResult>>)>
          callback) override;

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

  // Transition class representing the process of resizing the disk. Returns an
  // error (string) or a result (pair of BorealisDiskInfos, The first references
  // the original state and the second references the updated state after the
  // resize).
  class ResizeDisk;

  // Transition class representing the process of assessing the state of the VM
  // disk and adjusting it to fit within the appropriate parameters if needed.
  // Specifically, when the available space on the disk does not match the
  // target buffer size +/-10%, we will resize the disk.
  class SyncDisk;

  // Handles the results of a GetDiskInfo request.
  void BuildGetDiskInfoResponse(
      base::OnceCallback<void(
          Expected<GetDiskInfoResponse, Described<BorealisGetDiskInfoResult>>)>
          callback,
      Expected<std::unique_ptr<BorealisDiskInfo>,
               Described<BorealisGetDiskInfoResult>> disk_info_or_error);

  // Handles the RequestSpace and ReleaseSpace requests. |bytes_requested| from
  // RequestSpace becomes a positive delta that expands the disk and the
  // |bytes_to_release| from ReleaseSpace becomes a negative delta that shrinks
  // the disk (an error is returned if the unit64_t can't be converted to a
  // negative int64_t).
  void RequestSpaceDelta(
      int64_t target_delta,
      base::OnceCallback<void(
          Expected<uint64_t, Described<BorealisResizeDiskResult>>)> callback);

  void OnRequestSpaceDelta(
      int64_t target_delta,
      base::OnceCallback<void(
          Expected<uint64_t, Described<BorealisResizeDiskResult>>)> callback,
      Expected<std::unique_ptr<std::pair<BorealisDiskInfo, BorealisDiskInfo>>,
               Described<BorealisResizeDiskResult>> success_or_error);

  void OnSyncDiskSize(
      base::OnceCallback<void(Expected<BorealisSyncDiskSizeResult,
                                       Described<BorealisSyncDiskSizeResult>>)>
          callback,
      Expected<std::unique_ptr<BorealisSyncDiskSizeResult>,
               Described<BorealisSyncDiskSizeResult>> success_or_error);

  const BorealisContext* const context_;
  BorealisService* const service_;
  int request_count_{0};
  std::unique_ptr<BuildDiskInfo> build_disk_info_transition_;
  std::unique_ptr<ResizeDisk> resize_disk_transition_;
  std::unique_ptr<SyncDisk> sync_disk_transition_;
  std::unique_ptr<FreeSpaceProvider> free_space_provider_;
  base::WeakPtrFactory<BorealisDiskManagerImpl> weak_factory_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_IMPL_H_
