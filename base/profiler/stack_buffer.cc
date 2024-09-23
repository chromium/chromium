// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/stack_buffer.h"

#include <bit>

#if BUILDFLAG(IS_CHROMEOS)
#include <sys/mman.h>

#include <ostream>

#include "base/bits.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/page_size.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS)

namespace base {

constexpr size_t StackBuffer::kPlatformStackAlignment;

#if BUILDFLAG(IS_CHROMEOS)

void StackBuffer::MarkUpperBufferContentsAsUnneeded(size_t retained_bytes) {
  // Round up to the next multiple of the page size. madvise needs the
  // starting address to be page aligned. Since buffer_.get() is
  // already page aligned, we just need to round up the retained bytes.
  size_t actual_retained_bytes = bits::AlignUp(retained_bytes, GetPageSize());

  // Avoid passing a negative discard_size to madvise(). Doing so would randomly
  // discard large amounts of memory causing weird crashes.
  CHECK_LE(actual_retained_bytes, size_);

  uint8_t* start_of_discard =
      reinterpret_cast<uint8_t*>(buffer_.get()) + actual_retained_bytes;
  size_t discard_size = size_ - actual_retained_bytes;
  int result = madvise(start_of_discard, discard_size, MADV_DONTNEED);

  DPCHECK(result == 0) << "madvise failed: ";
}

#endif  // #if BUILDFLAG(IS_CHROMEOS)

StackBuffer::StackBuffer(size_t buffer_size)
#if BUILDFLAG(IS_CHROMEOS)
    // On ChromeOS, we have 8MB of stack space per thread; however, we normally
    // only use a small fraction of that. To avoid blowing our memory budget,
    // we use madvise(MADV_DONTNEED) to let the kernel discard the memory in the
    // 8MB buffer except when we are actively using it. For madvise() to work,
    // we need |buffer_| to be aligned to a page boundary.
    //
    // We also need the |size_| to be a multiple of the page size so that we
    // don't pass partial pages to madvise(). This isn't documented but the
    // program will consistently crash otherwise.
    : size_(bits::AlignUp(buffer_size, GetPageSize())),
      buffer_(static_cast<uintptr_t*>(AlignedAlloc(size_, GetPageSize()))) {
  // Our (very large) buffer may already have data written to it & thus have
  // backing pages. Tell the kernel we don't need the current contents.
  MarkUpperBufferContentsAsUnneeded(0);
}
#else   // #if BUILDFLAG(IS_CHROMEOS)
    : size_(buffer_size),
      buffer_(static_cast<uintptr_t*>(
          AlignedAlloc(size_, kPlatformStackAlignment))) {
  static_assert(std::has_single_bit(kPlatformStackAlignment));
}
#endif  // !#if BUILDFLAG(IS_CHROMEOS)

StackBuffer::~StackBuffer() = default;

}  // namespace base
