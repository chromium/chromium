// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_SCHEDULER_LOOP_QUARANTINE_SUPPORT_H_
#define BASE_ALLOCATOR_SCHEDULER_LOOP_QUARANTINE_SUPPORT_H_

#include "partition_alloc/scheduler_loop_quarantine_support.h"

namespace base::allocator {

using partition_alloc::SchedulerLoopQuarantineScanPolicyUpdater;
using partition_alloc::ScopedSchedulerLoopQuarantineDisallowScanlessPurge;
using partition_alloc::ScopedSchedulerLoopQuarantineExclusion;

}  // namespace base::allocator

#endif  // BASE_ALLOCATOR_SCHEDULER_LOOP_QUARANTINE_SUPPORT_H_
