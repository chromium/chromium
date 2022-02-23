// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_NOTREACHED_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_NOTREACHED_H_

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/logging_buildflags.h"

// When PartitionAlloc is used as the default allocator, we cannot use the
// regular (D)CHECK() macros, as they allocate internally. (c.f. //
// base/allocator/partition_allocator/partition_alloc_check.h)
// So PA_NOTREACHED() uses PA_DCHECK() instead of DCHECK().

#if BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
#define PA_NOTREACHED()                                                      \
  true ? logging::RawError(__FILE__                                          \
                           "(" PA_STRINGIFY(__LINE__) ") NOTREACHED() hit.") \
       : EAT_CHECK_STREAM_PARAMS()

#elif BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(OFFICIAL_BUILD) && \
    defined(NDEBUG) && DCHECK_IS_ON()

// PA_DCHECK(condition) is PA_CHECK(condition) if DCHECK_IS_ON().
// When BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC), OFFICIAL_BUILD, NDEBUG are
// defined, PA_CHECK(false) is IMMEDIATE_CRASH(). Since IMMEDIATE_CRASH()
// hints __builtin_unreachable() to the compiler, the following code causes
// compile failure:
//   switch(...) {
//     ...
//     case X:
//       PA_DCHECK(false);
//       [[fallthrough]]; // The compiler knows "not reached".
//     case Y:
//     ...
// So define PA_NOTREACHED() by using async-signal-safe RawCheck().
#define PA_NOTREACHED()                                                 \
  UNLIKELY(true)                                                        \
  ? logging::RawCheck(__FILE__                                          \
                      "(" PA_STRINGIFY(__LINE__) ") NOTREACHED() hit.") \
  : EAT_CHECK_STREAM_PARAMS()

#else

// PA_CHECK() uses RawCheck() for error reporting. So "PA_DCHECK(false);
// [[fallthrough]];" doesn't cause compile failure.
#define PA_NOTREACHED() PA_DCHECK(false)

#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_NOTREACHED_H_
