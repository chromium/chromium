// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CHECK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CHECK_H_

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/check.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"

#define PA_STRINGIFY_IMPL(s) #s
#define PA_STRINGIFY(s) PA_STRINGIFY_IMPL(s)

// When PartitionAlloc is used as the default allocator, we cannot use the
// regular (D)CHECK() macros, as they allocate internally. When an assertion is
// triggered, they format strings, leading to reentrancy in the code, which none
// of PartitionAlloc is designed to support (and especially not for error
// paths).
//
// As a consequence:
// - When PartitionAlloc is not malloc(), use the regular macros
// - Otherwise, crash immediately. This provides worse error messages though.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// For official build discard log strings to reduce binary bloat.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
// See base/check.h for implementation details.
#define PA_CHECK(condition) \
  UNLIKELY(!(condition)) ? IMMEDIATE_CRASH() : EAT_CHECK_STREAM_PARAMS()
#else
// PartitionAlloc uses async-signal-safe RawCheck() for error reporting.
// Async-signal-safe functions are guaranteed to not allocate as otherwise they
// could operate with inconsistent allocator state.
#define PA_CHECK(condition)                                                \
  UNLIKELY(!(condition))                                                   \
  ? logging::RawCheck(                                                     \
        __FILE__ "(" PA_STRINGIFY(__LINE__) ") Check failed: " #condition) \
  : EAT_CHECK_STREAM_PARAMS()
#endif  // defined(OFFICIAL_BUILD) && defined(NDEBUG)

#if DCHECK_IS_ON()
#define PA_DCHECK(condition) PA_CHECK(condition)
#else
#define PA_DCHECK(condition) EAT_CHECK_STREAM_PARAMS(!(condition))
#endif  // DCHECK_IS_ON()

#define PA_PCHECK(condition)    \
  if (!(condition)) {           \
    int error = errno;          \
    base::debug::Alias(&error); \
    IMMEDIATE_CRASH();          \
  }

#else
#define PA_CHECK(condition) CHECK(condition)
#define PA_DCHECK(condition) DCHECK(condition)
#define PA_PCHECK(condition) PCHECK(condition)
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if defined(PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR)

// Use this macro to assert on things that are conditionally constexpr as
// determined by PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR or
// PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR. Where fixed at compile time, this
// is a static_assert. Where determined at run time, this is a PA_CHECK.
// Therefore, this macro must only be used where both a static_assert and a
// PA_CHECK would be viable, that is, within a function, and ideally a function
// that executes only once, early in the program, such as during initialization.
#define STATIC_ASSERT_OR_PA_CHECK(condition, message) \
  static_assert(condition, message)

#else

#define STATIC_ASSERT_OR_PA_CHECK(condition, message) \
  do {                                                \
    PA_CHECK(condition) << (message);                 \
  } while (false)

#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CHECK_H_
