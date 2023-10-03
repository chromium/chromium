// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_CAST_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_CAST_H_

#include <memory>

#include <type_traits>
#if defined(__has_builtin)
#define PA_RAWPTR_CAST_USE_BUILTIN_BIT_CAST __has_builtin(__builtin_bit_cast)
#else
#define PA_RAWPTR_CAST_USE_BUILTIN_BIT_CAST 0
#endif

#if !PA_RAWPTR_CAST_USE_BUILTIN_BIT_CAST
#include <cstring>
#endif

// This header is explicitly allowlisted from a clang plugin rule at
// "tools/clang/plugins/FindBadRawPtrPatterns.cpp". You can bypass these checks
// by performing casts explicitly with functions here.
namespace base {

// Wrapper for |static_cast<T>(src)|.
template <typename Dest, typename Source>
inline constexpr Dest unsafe_raw_ptr_static_cast(Source&& source) noexcept {
  return static_cast<Dest>(source);
}

// Wrapper for |reinterpret_cast<T>(src)|.
template <typename Dest, typename Source>
inline constexpr Dest unsafe_raw_ptr_reinterpret_cast(
    Source&& source) noexcept {
  return reinterpret_cast<Dest>(source);
}

// Wrapper for |std::bit_cast<T>(src)|.
// Though we have similar implementations at |absl::bit_cast| and
// |base::bit_cast|, it is important to perform casting in this file to
// correctly exclude from the check.
template <typename Dest, typename Source>
#if PA_RAWPTR_CAST_USE_BUILTIN_BIT_CAST
inline constexpr std::enable_if_t<sizeof(Dest) == sizeof(Source) &&
                                      std::is_trivially_copyable_v<Dest> &&
                                      std::is_trivially_copyable_v<Source>,
                                  Dest>
#else
inline std::enable_if_t<sizeof(Dest) == sizeof(Source) &&
                            std::is_trivially_copyable_v<Dest> &&
                            std::is_trivially_copyable_v<Source> &&
                            std::is_default_constructible_v<Dest>,
                        Dest>
#endif  // PA_RAWPTR_CAST_USE_BUILTIN_BIT_CAST
unsafe_raw_ptr_bit_cast(const Source& source) noexcept {
  // TODO(mikt): Replace this with |std::bit_cast<T>| when C++20 arrives.
#if PA_RAWPTR_CAST_USE_BUILTIN_BIT_CAST
  return __builtin_bit_cast(Dest, source);
#else
  Dest dest;
  memcpy(std::addressof(dest), std::addressof(source), sizeof(dest));
  return dest;
#endif  // PA_RAWPTR_CAST_USE_BUILTIN_BIT_CAST
}

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_CAST_H_
