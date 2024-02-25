// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_

#include "build/build_config.h"
#include "partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc::internal::base::internal {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Current thread id is cached in thread local storage for performance reasons.
// In some rare cases it's important to invalidate that cache explicitly (e.g.
// after going through clone() syscall which does not call pthread_atfork()
// handlers).
// This can only be called when the process is single-threaded.
PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE) void InvalidateTidCache();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace partition_alloc::internal::base::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_
