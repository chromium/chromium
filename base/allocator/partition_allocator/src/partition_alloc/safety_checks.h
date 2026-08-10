// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_SAFETY_CHECKS_H_
#define PARTITION_ALLOC_SAFETY_CHECKS_H_

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/memory/stack_allocated.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/scheduler_loop_quarantine_support.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace partition_alloc {

// Utility class to exclude deallocation from optional safety checks when an
// instance is on the stack. Can be applied to performance critical functions.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) ScopedSafetyChecksExclusion {
  PA_STACK_ALLOCATED();

 public:
  // Make this non-trivially-destructible to suppress unused variable warning.
  ~ScopedSafetyChecksExclusion() {}  // NOLINT(modernize-use-equals-default)

 private:
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  ScopedSchedulerLoopQuarantineExclusion opt_out_scheduler_loop_quarantine_;
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
};

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_SAFETY_CHECKS_H_
