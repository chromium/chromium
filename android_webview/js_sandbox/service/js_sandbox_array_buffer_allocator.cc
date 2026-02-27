// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_array_buffer_allocator.h"

#include <cstddef>

#include "android_webview/js_sandbox/service/js_sandbox_memory_budget.h"
#include "v8/include/v8-array-buffer.h"

namespace android_webview {

JsSandboxArrayBufferAllocator::JsSandboxArrayBufferAllocator(
    v8::ArrayBuffer::Allocator& inner,
    JsSandboxMemoryBudget& memory_budget)
    : inner_allocator_(inner), memory_budget_(memory_budget) {}

JsSandboxArrayBufferAllocator::~JsSandboxArrayBufferAllocator() = default;

void* JsSandboxArrayBufferAllocator::Allocate(const size_t length) {
  if (!memory_budget_->Allocate(length)) {
    return nullptr;
  }
  void* const buffer = inner_allocator_->Allocate(length);
  if (!buffer) {
    memory_budget_->Free(length);
    return nullptr;
  }
  return buffer;
}

void* JsSandboxArrayBufferAllocator::AllocateUninitialized(
    const size_t length) {
  if (!memory_budget_->Allocate(length)) {
    return nullptr;
  }
  void* const buffer = inner_allocator_->AllocateUninitialized(length);
  if (!buffer) {
    memory_budget_->Free(length);
    return nullptr;
  }
  return buffer;
}

void JsSandboxArrayBufferAllocator::Free(void* const data,
                                         const size_t length) {
  inner_allocator_->Free(data, length);
  memory_budget_->Free(length);
}

}  // namespace android_webview
