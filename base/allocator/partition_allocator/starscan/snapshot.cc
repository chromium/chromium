// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/snapshot.h"

#include <memory>
#include <mutex>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/starscan/pcscan_internal.h"

namespace base {
namespace internal {

std::unique_ptr<StarScanSnapshot> StarScanSnapshot::Create(
    const PCScanInternal& pcscan) {
  // Create unique_ptr object to avoid presubmit error.
  std::unique_ptr<StarScanSnapshot> snapshot(new StarScanSnapshot(pcscan));
  return snapshot;
}

StarScanSnapshot::StarScanSnapshot(const PCScanInternal& pcscan) {
  PA_DCHECK(pcscan.is_initialized());
  std::lock_guard<std::mutex> lock(pcscan.roots_mutex_);

  for (const auto& root : pcscan.scannable_roots()) {
    const auto& super_pages = root.second;
    clear_worklist_.Push(super_pages.begin(), super_pages.end());
    scan_worklist_.Push(super_pages.begin(), super_pages.end());
    sweep_worklist_.Push(super_pages.begin(), super_pages.end());
    if (pcscan.WriteProtectionEnabled())
      unprotect_worklist_.Push(super_pages.begin(), super_pages.end());
  }

  for (const auto& root : pcscan.nonscannable_roots()) {
    const auto& super_pages = root.second;
    clear_worklist_.Push(super_pages.begin(), super_pages.end());
    sweep_worklist_.Push(super_pages.begin(), super_pages.end());
    if (pcscan.WriteProtectionEnabled())
      unprotect_worklist_.Push(super_pages.begin(), super_pages.end());
  }
}

StarScanSnapshot::~StarScanSnapshot() = default;

}  // namespace internal
}  // namespace base
