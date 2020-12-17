// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/resources/disk_resource_impl.h"

#include <atomic>
#include <cstdint>

#include "base/check_op.h"
#include "base/no_destructor.h"

namespace reporting {

// TODO(b/159361496): Set total disk allowance based on the platform
// (or policy?).
DiskResourceImpl::DiskResourceImpl()
    : total_(256u * 1024LLu * 1024LLu),  // 256 MiB
      used_(0) {}

DiskResourceImpl::~DiskResourceImpl() = default;

bool DiskResourceImpl::Reserve(int64_t size) {
  int64_t old_used = used_.fetch_add(size);
  if (old_used + size > total_) {
    used_.fetch_sub(size);
    return false;
  }
  return true;
}

void DiskResourceImpl::Discard(int64_t size) {
  DCHECK_LE(size, used_.load());
  used_.fetch_sub(size);
}

int64_t DiskResourceImpl::GetTotal() {
  return total_;
}

int64_t DiskResourceImpl::GetUsed() {
  return used_.load();
}

void DiskResourceImpl::Test_SetTotal(int64_t test_total) {
  total_ = test_total;
}

ResourceInterface* GetDiskResource() {
  static base::NoDestructor<DiskResourceImpl> disk;
  return disk.get();
}

}  // namespace reporting
