// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_array_buffer_allocator.h"

#include <cstddef>
#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "v8/include/v8-array-buffer.h"

namespace {

size_t RoundUpToPage(const size_t amount, const size_t page_size) {
  CHECK_LE(amount, SIZE_MAX / page_size * page_size);
  return ((amount + page_size - 1) / page_size) * page_size;
}

}  // namespace

namespace android_webview {

JsSandboxArrayBufferAllocator::JsSandboxArrayBufferAllocator(
    v8::ArrayBuffer::Allocator& inner,
    const size_t budget,
    const size_t page_size)
    : inner_allocator_(inner),
      remaining_(budget),
      budget_(budget),
      page_size_(page_size) {
  DCHECK_GT(page_size, size_t{0});
}

JsSandboxArrayBufferAllocator::~JsSandboxArrayBufferAllocator() {
  // Note, remaining_ <= budget is an invariant maintained by CHECKs, so this
  // should only ever fail if remaining_ < budget_.
  DCHECK_EQ(remaining_, budget_) << "Memory leaked: " << (budget_ - remaining_)
                                 << " bytes of array buffers not freed before "
                                    "array buffer allocator destruction";
}

void* JsSandboxArrayBufferAllocator::Allocate(const size_t length) {
  if (!AllocateBudget(length)) {
    return nullptr;
  }
  void* const buffer = inner_allocator_->Allocate(length);
  if (!buffer) {
    FreeBudget(length);
    return nullptr;
  }
  return buffer;
}

void* JsSandboxArrayBufferAllocator::AllocateUninitialized(
    const size_t length) {
  if (!AllocateBudget(length)) {
    return nullptr;
  }
  void* const buffer = inner_allocator_->AllocateUninitialized(length);
  if (!buffer) {
    FreeBudget(length);
    return nullptr;
  }
  return buffer;
}

void JsSandboxArrayBufferAllocator::Free(void* const data,
                                         const size_t length) {
  inner_allocator_->Free(data, length);
  FreeBudget(length);
}

void* JsSandboxArrayBufferAllocator::Reallocate(void* const data,
                                                const size_t old_length,
                                                const size_t new_length) {
  // Don't assume we will only need new_length minus old_length extra bytes
  // during the operation, or that if the new_length is less than old_length
  // that everything will just happen in place.  In fact, the V8 docs stipulate
  // that the default implementation will always allocate a new block and copy
  // the old data over.
  if (!AllocateBudget(new_length)) {
    return nullptr;
  }
  void* const new_data =
      inner_allocator_->Reallocate(data, old_length, new_length);
  if (!new_data) {
    FreeBudget(new_length);
    return nullptr;
  }
  FreeBudget(old_length);
  return new_data;
}

bool JsSandboxArrayBufferAllocator::AllocateBudget(const size_t amount) {
  const size_t rounded_amount = RoundUpToPage(amount, page_size_);
  if (remaining_ < rounded_amount) {
    return false;
  }
  remaining_ -= rounded_amount;
  return true;
}

void JsSandboxArrayBufferAllocator::FreeBudget(const size_t amount) {
  const size_t rounded_amount = RoundUpToPage(amount, page_size_);
  CHECK_LE(amount, GetUsage())
      << "attempted to free more array buffer memory than is allocated";
  remaining_ += rounded_amount;
}

size_t JsSandboxArrayBufferAllocator::GetUsage() const {
  return budget_ - remaining_;
}

}  // namespace android_webview
