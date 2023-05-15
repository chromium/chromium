// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"

namespace partition_alloc {

namespace internal {

// Alignment has two constraints:
// - Alignment requirement for scalar types: alignof(std::max_align_t)
// - Alignment requirement for operator new().
//
// The two are separate on Windows 64 bits, where the first one is 8 bytes, and
// the second one 16. We could technically return something different for
// malloc() and operator new(), but this would complicate things, and most of
// our allocations are presumably coming from operator new() anyway.
constexpr size_t kAlignment =
    std::max(alignof(max_align_t),
             static_cast<size_t>(__STDCPP_DEFAULT_NEW_ALIGNMENT__));
static_assert(kAlignment <= 16,
              "PartitionAlloc doesn't support a fundamental alignment larger "
              "than 16 bytes.");

constexpr bool ThreadSafe = true;

template <bool thread_safe>
struct SlotSpanMetadata;

// This type trait verifies a type can be used as a pointer offset.
//
// We support pointer offsets in signed (ptrdiff_t) or unsigned (size_t) values.
// Smaller types are also allowed.
template <typename Z>
static constexpr bool is_offset_type =
    std::is_integral_v<Z> && sizeof(Z) <= sizeof(ptrdiff_t);

}  // namespace internal

class PartitionStatsDumper;

template <bool thread_safe = internal::ThreadSafe>
struct PartitionRoot;

using ThreadSafePartitionRoot = PartitionRoot<internal::ThreadSafe>;

}  // namespace partition_alloc

// From https://clang.llvm.org/docs/AttributeReference.html#malloc:
//
// The malloc attribute indicates that the function acts like a system memory
// allocation function, returning a pointer to allocated storage disjoint from
// the storage for any other object accessible to the caller.
//
// Note that it doesn't apply to realloc()-type functions, as they can return
// the same pointer as the one passed as a parameter, as noted in e.g. stdlib.h
// on Linux systems.
#if PA_HAS_ATTRIBUTE(malloc)
#define PA_MALLOC_FN __attribute__((malloc))
#endif

// Allows the compiler to assume that the return value is aligned on a
// kAlignment boundary. This is useful for e.g. using aligned vector
// instructions in the constructor for zeroing.
#if PA_HAS_ATTRIBUTE(assume_aligned)
#define PA_MALLOC_ALIGNED \
  __attribute__((assume_aligned(::partition_alloc::internal::kAlignment)))
#endif

#if !defined(PA_MALLOC_FN)
#define PA_MALLOC_FN
#endif

#if !defined(PA_MALLOC_ALIGNED)
#define PA_MALLOC_ALIGNED
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_
