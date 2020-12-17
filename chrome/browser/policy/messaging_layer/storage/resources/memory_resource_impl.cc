// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/resources/memory_resource_impl.h"

#include <atomic>
#include <cstdint>

#include "base/check_op.h"
#include "base/no_destructor.h"

namespace reporting {

// TODO(b/159361496): Set total memory allowance based on the platform
// (or policy?).
MemoryResourceImpl::MemoryResourceImpl()
    : total_(16u * 1024LLu * 1024LLu),  // 16 MiB
      used_(0) {}

MemoryResourceImpl::~MemoryResourceImpl() = default;

bool MemoryResourceImpl::Reserve(int64_t size) {
  DCHECK_GE(size, 0);
  int64_t old_used = used_.fetch_add(size);
  if (old_used + size > total_) {
    used_.fetch_sub(size);
    return false;
  }
  return true;
}

void MemoryResourceImpl::Discard(int64_t size) {
  DCHECK_LE(size, used_.load());
  used_.fetch_sub(size);
}

int64_t MemoryResourceImpl::GetTotal() {
  return total_;
}

int64_t MemoryResourceImpl::GetUsed() {
  return used_.load();
}

void MemoryResourceImpl::Test_SetTotal(int64_t test_total) {
  total_ = test_total;
}

ResourceInterface* GetMemoryResource() {
  static base::NoDestructor<MemoryResourceImpl> memory;
  return memory.get();
}

}  // namespace reporting
