// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_

#include "base/allocator/partition_allocator/oom_callback.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {
// The crash is generated in a NOINLINE function so that we can classify the
// crash as an OOM solely by analyzing the stack trace.
NOINLINE void OnNoMemory() {
  base::internal::RunPartitionAllocOomCallback();
#if defined(OS_WIN)
  ::RaiseException(base::win::kOomExceptionCode, EXCEPTION_NONCONTINUABLE, 0,
                   nullptr);
#endif
  IMMEDIATE_CRASH();
}
}  // namespace

// OOM_CRASH() - Specialization of IMMEDIATE_CRASH which will raise a custom
// exception on Windows to signal this is OOM and not a normal assert.
// OOM_CRASH() is called by users of PageAllocator (including PartitionAlloc) to
// signify an allocation failure from the platform.
#define OOM_CRASH() \
  do {              \
    OnNoMemory();   \
  } while (0)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_
