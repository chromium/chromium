// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_CAST_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_CAST_H_

#include <bit>
#include <memory>
#include <type_traits>

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
template <typename Dest, typename Source>
inline constexpr Dest unsafe_raw_ptr_bit_cast(const Source& source) noexcept {
  return std::bit_cast<Dest>(source);
}

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_CAST_H_
