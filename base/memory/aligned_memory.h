// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_ALIGNED_MEMORY_H_
#define BASE_MEMORY_ALIGNED_MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#include <bit>
#include <ostream>

#include "base/base_export.h"
#include "base/check.h"
#include "build/build_config.h"

#if defined(COMPILER_MSVC)
#include <malloc.h>
#else
#include <stdlib.h>
#endif

// A runtime sized aligned allocation can be created:
//
//   float* my_array = static_cast<float*>(AlignedAlloc(size, alignment));
//   CHECK(reinterpret_cast<uintptr_t>(my_array) % alignment == 0);
//   memset(my_array, 0, size);  // fills entire object.
//
//   // ... later, to release the memory:
//   AlignedFree(my_array);
//
// Or using unique_ptr:
//
//   std::unique_ptr<float, AlignedFreeDeleter> my_array(
//       static_cast<float*>(AlignedAlloc(size, alignment)));

namespace base {

// Allocate memory of size `size` aligned to `alignment`.
//
// TODO(https://crbug.com/40255447): Convert usage to / convert to use
// `std::aligned_alloc` to the extent that it can be done (since
// `std::aligned_alloc` can't be used on Windows). When that happens, note that
// `std::aligned_alloc` requires the `size` parameter be an integral multiple of
// `alignment` while this implementation does not.
BASE_EXPORT void* AlignedAlloc(size_t size, size_t alignment);

// Deallocate memory allocated by `AlignedAlloc`.
inline void AlignedFree(void* ptr) {
#if defined(COMPILER_MSVC)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

// Deleter for use with unique_ptr. E.g., use as
//   std::unique_ptr<Foo, base::AlignedFreeDeleter> foo;
struct AlignedFreeDeleter {
  inline void operator()(void* ptr) const {
    AlignedFree(ptr);
  }
};

#ifdef __has_builtin
#define SUPPORTS_BUILTIN_IS_ALIGNED (__has_builtin(__builtin_is_aligned))
#else
#define SUPPORTS_BUILTIN_IS_ALIGNED 0
#endif

inline bool IsAligned(uintptr_t val, size_t alignment) {
  // If the compiler supports builtin alignment checks prefer them.
#if SUPPORTS_BUILTIN_IS_ALIGNED
  return __builtin_is_aligned(val, alignment);
#else
  DCHECK(std::has_single_bit(alignment)) << alignment << " is not a power of 2";
  return (val & (alignment - 1)) == 0;
#endif
}

#undef SUPPORTS_BUILTIN_IS_ALIGNED

inline bool IsAligned(const void* val, size_t alignment) {
  return IsAligned(reinterpret_cast<uintptr_t>(val), alignment);
}

}  // namespace base

#endif  // BASE_MEMORY_ALIGNED_MEMORY_H_
