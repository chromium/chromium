// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_LOGGING_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_LOGGING_H_

#include "base/allocator/partition_allocator/allocation_guard.h"
#include "base/logging.h"

namespace base {
namespace internal {

// Logging requires allocations. This logger allows reentrant allocations to
// happen within the allocator context.
struct LoggerWithAllowedAllocations : ScopedAllowAllocations,
                                      logging::LogMessage {
  using logging::LogMessage::LogMessage;
};

#define PA_PCSCAN_VLOG_STREAM(verbose_level)                         \
  ::base::internal::LoggerWithAllowedAllocations(__FILE__, __LINE__, \
                                                 -(verbose_level))   \
      .stream()

// Logging macro that is meant to be used inside *Scan. Generally, reentrancy
// may be an issue if the macro is called from malloc()/free(). Currently, it's
// only called at the end of *Scan and when scheduling a new *Scan task.
// Allocating from these paths should not be an issue, since we make sure that
// no infinite recursion can occur (e.g. we can't schedule two *Scan tasks and
// the inner free() call must be non-reentrant).  However, these sorts of things
// are tricky to enforce and easy to mess up with. Since verbose *Scan logging
// is essential for debugging, we choose to provide support for it inside *Scan.
#define PA_PCSCAN_VLOG(verbose_level) \
  LAZY_STREAM(PA_PCSCAN_VLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_LOGGING_H_
