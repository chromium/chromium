// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_

#include "base/allocator/partition_allocator/oom_callback.h"
#include "base/logging.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

// Do not want trivial entry points just calling OOM_CRASH() to be
// commoned up by linker icf/comdat folding.
#define OOM_CRASH_PREVENT_ICF()                  \
  volatile int oom_crash_inhibit_icf = __LINE__; \
  ALLOW_UNUSED_LOCAL(oom_crash_inhibit_icf)

// OOM_CRASH() - Specialization of IMMEDIATE_CRASH which will raise a custom
// exception on Windows to signal this is OOM and not a normal assert.
#if defined(OS_WIN)
#define OOM_CRASH()                                                     \
  do {                                                                  \
    OOM_CRASH_PREVENT_ICF();                                            \
    base::internal::RunPartitionAllocOomCallback();                     \
    ::RaiseException(0xE0000008, EXCEPTION_NONCONTINUABLE, 0, nullptr); \
    IMMEDIATE_CRASH();                                                  \
  } while (0)
#else
#define OOM_CRASH()                                 \
  do {                                              \
    base::internal::RunPartitionAllocOomCallback(); \
    OOM_CRASH_PREVENT_ICF();                        \
    IMMEDIATE_CRASH();                              \
  } while (0)
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_
