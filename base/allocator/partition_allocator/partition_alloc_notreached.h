// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_NOTREACHED_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_NOTREACHED_H_

#include "base/allocator/partition_allocator/partition_alloc_check.h"
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
#else
#define PA_NOTREACHED() PA_DCHECK(false)
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_NOTREACHED_H_
