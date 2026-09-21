// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_CHECK_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_CHECK_H_

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_base/immediate_crash.h"

// When PartitionAlloc is used as the default allocator, we cannot use the
// regular (D)CHECK() macros, as they allocate internally. When an assertion is
// triggered, they format strings, leading to reentrancy in the code, which none
// of PartitionAlloc is designed to support (and especially not for error
// paths).
//
// As a consequence:
// - When PartitionAlloc is not malloc(), use the regular macros
// - Otherwise, crash immediately. This provides worse error messages though.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && !PA_BASE_CHECK_WILL_STREAM()

// For official build discard log strings to reduce binary bloat.
// See base/check.h for implementation details.
#define PA_CHECK(condition) PA_BASE_CHECK(condition)

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
#define PA_DCHECK(condition) PA_CHECK(condition)
#else
#define PA_DCHECK(condition) PA_EAT_CHECK_STREAM_PARAMS(!(condition))
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

#define PA_PCHECK(condition)                                 \
  if (!(condition)) {                                        \
    int error = errno;                                       \
    ::partition_alloc::internal::base::debug::Alias(&error); \
    PA_IMMEDIATE_CRASH();                                    \
  }                                                          \
  static_assert(true)

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
#define PA_DPCHECK(condition) PA_PCHECK(condition)
#else
#define PA_DPCHECK(condition) PA_EAT_CHECK_STREAM_PARAMS(!(condition))
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

#else  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
       // !PA_BASE_CHECK_WILL_STREAM()
#define PA_CHECK(condition) PA_BASE_CHECK(condition)
#define PA_DCHECK(condition) PA_BASE_DCHECK(condition)
#define PA_PCHECK(condition) PA_BASE_PCHECK(condition)
#define PA_DPCHECK(condition) PA_BASE_DPCHECK(condition)
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // !PA_BASE_CHECK_WILL_STREAM()

// Expensive dchecks that run within *Scan. These checks are only enabled in
// debug builds with dchecks enabled.
#if PA_BUILDFLAG(IS_DEBUG)
#define PA_SCAN_DCHECK_IS_ON() PA_BUILDFLAG(DCHECKS_ARE_ON)
#else
#define PA_SCAN_DCHECK_IS_ON() 0
#endif  // PA_BUILDFLAG(IS_DEBUG)

#if PA_SCAN_DCHECK_IS_ON()
#define PA_SCAN_DCHECK(expr) PA_DCHECK(expr)
#else
#define PA_SCAN_DCHECK(expr) PA_EAT_CHECK_STREAM_PARAMS(!(expr))
#endif

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

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_CHECK_H_
