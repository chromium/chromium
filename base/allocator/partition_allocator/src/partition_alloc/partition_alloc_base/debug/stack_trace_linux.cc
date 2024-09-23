// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/debug/stack_trace.h"

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"

namespace partition_alloc::internal::base::debug {

size_t CollectStackTrace(const void** trace, size_t count) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

#if PA_BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
  // Regarding Linux and Android, backtrace API internally invokes malloc().
  // So the API is not available inside memory allocation. Instead try tracing
  // using frame pointers.
  return base::debug::TraceStackFramePointers(trace, count, 0);
#else
  // Not able to obtain stack traces.
  return 0;
#endif
}

}  // namespace partition_alloc::internal::base::debug
