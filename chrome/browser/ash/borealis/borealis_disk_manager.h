// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ash/borealis/infra/described.h"
#include "chrome/browser/ash/borealis/infra/expected.h"

namespace borealis {

class BorealisContext;
enum class BorealisGetDiskInfoResult;
enum class BorealisResizeDiskResult;
enum class BorealisSyncDiskSizeResult;

// Service responsible for managing borealis' disk space.
class BorealisDiskManager {
 public:
  // Struct for storing the response of a GetDiskInfoRequest.
  struct GetDiskInfoResponse {
    // The number of bytes available for the client to use on the VM's disk.
    // (This is the true amount of available space on the disk minus any space
    // that is reserved for the buffer)
    uint64_t available_bytes = 0;
    // The number of bytes that the VM disk can be expanded by.
    uint64_t expandable_bytes = 0;
    // The current size of the disk in bytes.
    uint64_t disk_size = 0;
  };

  BorealisDiskManager() = default;
  virtual ~BorealisDiskManager() = default;

  // Gets information about the borealis disk and the host device, returns
  // information about how the disk could be resized or an error.
  virtual void GetDiskInfo(
      base::OnceCallback<void(
          Expected<GetDiskInfoResponse, Described<BorealisGetDiskInfoResult>>)>
          callback) = 0;

  // Attempt to expand the VM disk by the number of bytes specified. Returns the
  // actual size increase in bytes, or an error.
  virtual void RequestSpace(
      uint64_t bytes_requested,
      base::OnceCallback<
          void(Expected<uint64_t, Described<BorealisResizeDiskResult>>)>
          callback) = 0;

  // Attempt to shrink the VM disk by the number of bytes specified. Returns the
  // actual size decrease in bytes, or an error.
  virtual void ReleaseSpace(
      uint64_t bytes_to_release,
      base::OnceCallback<
          void(Expected<uint64_t, Described<BorealisResizeDiskResult>>)>
          callback) = 0;

  // Assesses the disk and resizes it so that it fits within the desired
  // constraints. This only fails if the process fails to get information
  // about the disk, the resize transition fails or if another SyncDiskSize
  // is already in progress. This means that we consider partial resizes or
  // cases where the disk size is inadequate, but we don't have room to expand
  // it, as successes. It will return an enum on success or an enum, with an
  // error string, on failure.
  virtual void SyncDiskSize(
      base::OnceCallback<void(Expected<BorealisSyncDiskSizeResult,
                                       Described<BorealisSyncDiskSizeResult>>)>
          callback) = 0;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_DISK_MANAGER_H_
