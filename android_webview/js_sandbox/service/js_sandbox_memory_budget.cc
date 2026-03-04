// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_memory_budget.h"

#include <cstddef>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace {

size_t RoundUpToPage(const size_t amount, const size_t page_size) {
  CHECK_LE(amount, SIZE_MAX / page_size * page_size);
  return ((amount + (page_size - 1)) / page_size) * page_size;
}

}  // namespace

namespace android_webview {

JsSandboxMemoryBudget::JsSandboxMemoryBudget(size_t budget, size_t page_size)
    : budget_(budget), page_size_(page_size), remaining_(budget) {
  CHECK_GT(page_size, 0u);
}

JsSandboxMemoryBudget::~JsSandboxMemoryBudget() {
  base::AutoLock lock(lock_);
  CHECK_EQ(remaining_, budget_)
      << "Memory leaked: " << (budget_ - remaining_)
      << " bytes not freed before memory budget destruction";
}

size_t JsSandboxMemoryBudget::GetUsage() const {
  base::AutoLock lock(lock_);
  return budget_ - remaining_;
}

bool JsSandboxMemoryBudget::Allocate(size_t amount) {
  const size_t rounded_amount = RoundUpToPage(amount, page_size_);
  base::AutoLock lock(lock_);
  if (remaining_ < rounded_amount) {
    return false;
  }
  remaining_ -= rounded_amount;
  return true;
}

void JsSandboxMemoryBudget::Free(size_t amount) {
  const size_t rounded_amount = RoundUpToPage(amount, page_size_);
  base::AutoLock lock(lock_);
  CHECK_LE(rounded_amount, budget_ - remaining_)
      << "Attempted to free more memory than is allocated";
  remaining_ += rounded_amount;
}

}  // namespace android_webview
