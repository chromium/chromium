// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_LOGGING_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_LOGGING_H_

#include "partition_alloc/allocation_guard.h"
#include "partition_alloc/partition_alloc_base/logging.h"

namespace partition_alloc::internal {

// Logging requires allocations. This logger allows reentrant allocations to
// happen within the allocator context.
struct LoggerWithAllowedAllocations : ScopedAllowAllocations,
                                      logging::LogMessage {
  using logging::LogMessage::LogMessage;
};

#define PA_PCSCAN_VLOG_STREAM(verbose_level)                 \
  ::partition_alloc::internal::LoggerWithAllowedAllocations( \
      __FILE__, __LINE__, -(verbose_level))                  \
      .stream()

// Logging macro that is meant to be used inside *Scan. Generally, reentrancy
// may be an issue if the macro is called from malloc()/free(). Currently, it's
// only called at the end of *Scan and when scheduling a new *Scan task.
// Allocating from these paths should not be an issue, since we make sure that
// no infinite recursion can occur (e.g. we can't schedule two *Scan tasks and
// the inner free() call must be non-reentrant).  However, these sorts of things
// are tricky to enforce and easy to mess up with. Since verbose *Scan logging
// is essential for debugging, we choose to provide support for it inside *Scan.
#define PA_PCSCAN_VLOG(verbose_level)                  \
  PA_LAZY_STREAM(PA_PCSCAN_VLOG_STREAM(verbose_level), \
                 PA_VLOG_IS_ON(verbose_level))

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_LOGGING_H_
