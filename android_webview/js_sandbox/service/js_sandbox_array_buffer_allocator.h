// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ARRAY_BUFFER_ALLOCATOR_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ARRAY_BUFFER_ALLOCATOR_H_

#include <cstddef>

#include "base/memory/raw_ref.h"
#include "v8/include/v8-array-buffer.h"

namespace android_webview {

class JsSandboxMemoryBudget;

// A v8::ArrayBuffer::Allocator which uses a JsSandboxMemoryBudget to track
// allocations against a shared budget. See also the V8 documentation for
// v8::ArrayBuffer::Allocator.
//
// This allocator must only be used from one thread at a time. Allocation and
// deallocation methods should only happen on the isolate thread, but
// construction and destruction will typically happen outside of the isolate
// thread.
class JsSandboxArrayBufferAllocator final : public v8::ArrayBuffer::Allocator {
 public:
  // Wrap the given inner allocator and use memory budget for accounting.
  //
  // The JsSandboxArrayBufferAllocator does NOT assume ownership of or copy the
  // inner allocator or memory budget, so the caller must ensure that they
  // outlive this object.
  explicit JsSandboxArrayBufferAllocator(v8::ArrayBuffer::Allocator& inner,
                                         JsSandboxMemoryBudget& memory_budget);
  JsSandboxArrayBufferAllocator(const JsSandboxArrayBufferAllocator&) = delete;
  JsSandboxArrayBufferAllocator& operator=(
      const JsSandboxArrayBufferAllocator&) = delete;
  // All prior allocations must be freed before the allocator is destructed.
  ~JsSandboxArrayBufferAllocator() override;

  // Attempt to allocate length bytes (zero-initialized).
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

 private:
  // The underlying allocator which does the actual memory management.
  const raw_ref<v8::ArrayBuffer::Allocator> inner_allocator_;
  // The budget used for accounting.
  const raw_ref<JsSandboxMemoryBudget> memory_budget_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ARRAY_BUFFER_ALLOCATOR_H_
