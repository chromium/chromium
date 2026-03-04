// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_MEMORY_BUDGET_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_MEMORY_BUDGET_H_

#include <cstddef>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace android_webview {

// A thread-safe object for tracking memory usage against a budget.
class JsSandboxMemoryBudget {
 public:
  // Value to supply as a budget to indicate an unlimited budget.
  static constexpr size_t kUnlimitedBudget = SIZE_MAX;

  // The memory budget specifies the maximum amount of memory which can be
  // allocated at any given time. Note that a value of 0 will disallow all
  // allocations. Use kUnlimitedBudget if you want no effective limit.
  //
  // The page size should describe the page size of the allocator used (this is
  // not necessarily the OS page size), and is used to (pessimistically) account
  // for possible internal memory fragmentation in the budget.
  explicit JsSandboxMemoryBudget(size_t memory_budget, size_t page_size);
  ~JsSandboxMemoryBudget();

  JsSandboxMemoryBudget(const JsSandboxMemoryBudget&) = delete;
  JsSandboxMemoryBudget& operator=(const JsSandboxMemoryBudget&) = delete;

  // Return the amount of allocated (used) memory.
  size_t GetUsage() const;
  // Try to allocate given bytes amount from the remaining budget. Returns true
  // iff success. The tracked memory usage may be rounded up to a multiple of
  // the page size.
  bool Allocate(size_t amount);
  // Deallocate the given bytes amount, and returns it to the budget.
  void Free(size_t amount);

 private:
  // The overall memory budget the allocators draw from.
  const size_t budget_;
  // Page size for rounding up allocation amounts.
  const size_t page_size_;
  // Protects access to `remaining_`.
  mutable base::Lock lock_;
  // The amount of unused budget.
  size_t remaining_ GUARDED_BY(lock_);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_MEMORY_BUDGET_H_
