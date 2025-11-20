// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/debug/stack_trace.h"

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/numerics/safe_conversions.h"

// Surprisingly, uClibc defines __GLIBC__ in some build configs, but
// execinfo.h and backtrace(3) are really only present in glibc and in macOS
// libc.
#if PA_BUILDFLAG(IS_APPLE) || \
    (defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__AIX))
#define HAVE_BACKTRACE
#include <execinfo.h>
#endif

namespace partition_alloc::internal::base::debug {

size_t CollectStackTrace(const void** trace, size_t count) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

#if PA_BUILDFLAG(IS_APPLE) && defined(HAVE_BACKTRACE)
  // Regarding Apple, no /proc is available. Try backtrace API.
  // Though the backtrace API man page does not list any possible negative
  // return values, we take no chance.
  return base::saturated_cast<size_t>(
      backtrace(const_cast<void**>(trace), base::saturated_cast<int>(count)));
#else
  // Not able to obtain stack traces.
  return 0;
#endif
}

}  // namespace partition_alloc::internal::base::debug
