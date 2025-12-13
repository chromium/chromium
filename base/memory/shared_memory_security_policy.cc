// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_security_policy.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <optional>

#include "base/bits.h"
#include "base/numerics/checked_math.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"

namespace base {

namespace {

#if defined(ARCH_CPU_32_BITS)
// No effective limit on 32-bit, since there simply isn't enough address space
// for ASLR to be particularly effective.
constexpr size_t kTotalMappedSizeLimit = std::numeric_limits<size_t>::max();
#elif defined(ARCH_CPU_64_BITS)
// 32 GB of mappings ought to be enough for anybody.
constexpr size_t kTotalMappedSizeLimit = 32ULL * 1024 * 1024 * 1024;
#endif

static std::atomic_size_t total_mapped_size_;

std::optional<size_t> AlignWithPageSize(size_t size) {
  // Note that the function name says "page size", but on Windows, the
  // allocation granularity is distinct from the page size: on typical systems,
  // the allocation granularity is 64KB but the page size is 4KB.  In practice,
  // `MapViewOfFile()` returns addresses that are aligned to the allocation
  // granularity, so every mapping on Windows costs at least 64KB.
  //
  // TODO(crbug.com/440123090): Windows 10 version 1803 and later provide
  // `VirtualAlloc2()` and `MapViewOfFile3()` to relax the alignment to page
  // granularity. This will also require sorting out the alignment checks in V8,
  // which largely use allocation granularity instead of page size.
  const size_t page_size = SysInfo::VMAllocationGranularity();
  size_t rounded_size = bits::AlignUp(size, page_size);

  // Fail on overflow.
  if (rounded_size < size) {
    return std::nullopt;
  }

  return rounded_size;
}

}  // namespace

// static
bool SharedMemorySecurityPolicy::AcquireReservationForMapping(size_t size) {
  size_t previous_mapped_size =
      total_mapped_size_.load(std::memory_order_relaxed);
  size_t total_mapped_size;

  std::optional<size_t> page_aligned_size = AlignWithPageSize(size);

  if (!page_aligned_size) {
    return false;
  }

  // Relaxed memory ordering is all that's needed since all atomicity is all
  // that's required. If the value is stale, compare_exchange_weak() will fail
  // and the loop will retry the operation with an updated total mapped size.
  do {
    if (!CheckAdd(previous_mapped_size, *page_aligned_size)
             .AssignIfValid(&total_mapped_size)) {
      return false;
    }
    if (total_mapped_size >= kTotalMappedSizeLimit) {
      return false;
    }
  } while (!total_mapped_size_.compare_exchange_weak(
      previous_mapped_size, total_mapped_size, std::memory_order_relaxed,
      std::memory_order_relaxed));

  return true;
}

// static
void SharedMemorySecurityPolicy::ReleaseReservationForMapping(size_t size) {
  // Note #1: relaxed memory ordering is sufficient since atomicity is all
  // that's required.
  // Note #2: |size| should never overflow when aligned to page size, since
  // this should only be called if AcquireReservationForMapping() returned true.
  std::optional<size_t> page_aligned_size = AlignWithPageSize(size);
  total_mapped_size_.fetch_sub(*page_aligned_size, std::memory_order_relaxed);
}

}  // namespace base
