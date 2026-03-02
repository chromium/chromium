// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ARRAY_BUFFER_ALLOCATOR_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ARRAY_BUFFER_ALLOCATOR_H_

#include <cstddef>

#include "base/memory/raw_ref.h"
#include "v8/include/v8-array-buffer.h"

namespace android_webview {

// A v8::ArrayBuffer::Allocator which imposes a limit on the amount of
// simultaneously allocated memory. See also the V8 documentation for
// v8::ArrayBuffer::Allocator.
//
// This allocator must only be used from one thread at a time. Allocation and
// deallocation methods should only happen on the isolate thread, but
// construction and destruction will typically happen outside of the isolate
// thread.
class JsSandboxArrayBufferAllocator final : public v8::ArrayBuffer::Allocator {
 public:
  // Value to supply as a budget to indicate an unlimited budget.
  static constexpr size_t kUnlimitedBudget = SIZE_MAX;

  // Wrap the given inner allocator and impose a maximum allocation budget.
  //
  // The JsSandboxArrayBufferAllocator allocator does NOT assume ownership of or
  // copy the inner allocator, so the caller must ensure that it outlives the
  // JsSandboxArrayBufferAllocator.
  //
  // The memory budget specifies the maximum amount of memory which can be
  // allocated at any given time. Note that a value of 0 will disallow all
  // allocations. Use kUnlimitedBudget if you want no effective limit.
  //
  // The page size should desribe the page size of the allocator used (this is
  // not necessarily the OS page size), and is used to (pessimistically) account
  // for possible internal memory fragmentation in the budget.
  explicit JsSandboxArrayBufferAllocator(v8::ArrayBuffer::Allocator& inner,
                                         size_t memory_budget,
                                         size_t page_size);
  JsSandboxArrayBufferAllocator(const JsSandboxArrayBufferAllocator&) = delete;
  JsSandboxArrayBufferAllocator& operator=(
      const JsSandboxArrayBufferAllocator&) = delete;
  // All prior allocations must be freed before the allocator is destructed.
  ~JsSandboxArrayBufferAllocator() override;
  // Attempt to allocate length bytes (zero-initialized).
  //
  // The tracked memory usage may be rounded up to a multiple of the page size.
  //
  // nullptr will be returned if the request exceeds the memory budget or the
  // inner allocator returns nullptr.
  void* Allocate(size_t length) override;
  // Similar to Allocate(), but does not explicitly zero-(re)initialize memory
  // in userspace.
  void* AllocateUninitialized(size_t length) override;
  // Deallocate a currently allocated memory region that was previously returned
  // by Allocate.
  //
  // The length must match the current length associated with the region.
  void Free(void* data, size_t length) override;

  // Return the amount of allocated (used) memory budget.
  size_t GetUsage() const;

 private:
  // Try to allocate some budget and return true iff successful
  bool AllocateBudget(size_t amount);
  // Deallocate some budget
  void FreeBudget(size_t amount);

  // The underlying allocator which does the actual memory management.
  const raw_ref<v8::ArrayBuffer::Allocator> inner_allocator_;
  // The amount of unused budget
  size_t remaining_;
  // The overall memory budget this allocator has to stay within
  const size_t budget_;
  // Page size for rounding up allocation amounts.
  const size_t page_size_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ARRAY_BUFFER_ALLOCATOR_H_
