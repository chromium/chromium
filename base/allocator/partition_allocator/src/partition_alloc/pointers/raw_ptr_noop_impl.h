// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_NOOP_IMPL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_NOOP_IMPL_H_

#include <type_traits>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_forward.h"

namespace base::internal {

struct RawPtrNoOpImpl {
  static constexpr bool kMustZeroOnConstruct = false;
  static constexpr bool kMustZeroOnMove = false;
  static constexpr bool kMustZeroOnDestruct = false;

  // Wraps a pointer.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtr(T* ptr) {
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or
  // replaced.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForDereference(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForExtraction(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForComparison(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  PA_ALWAYS_INLINE static constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible_v<From*, To*>,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies
    // in the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <
      typename T,
      typename Z,
      typename =
          std::enable_if_t<partition_alloc::internal::is_offset_type<Z>, void>>
  PA_ALWAYS_INLINE static constexpr T* Advance(T* wrapped_ptr, Z delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  // Retreat the wrapped pointer by `delta_elems`.
  template <
      typename T,
      typename Z,
      typename =
          std::enable_if_t<partition_alloc::internal::is_offset_type<Z>, void>>
  PA_ALWAYS_INLINE static constexpr T* Retreat(T* wrapped_ptr, Z delta_elems) {
    return wrapped_ptr - delta_elems;
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                            T* wrapped_ptr2) {
    return wrapped_ptr1 - wrapped_ptr2;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on
  // whether memory was freed or not.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // `WrapRawPtrForDuplication` and `UnsafelyUnwrapPtrForDuplication` are used
  // to create a new raw_ptr<T> from another raw_ptr<T> of a different flavor.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtrForDuplication(T* ptr) {
    return ptr;
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForDuplication(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  PA_ALWAYS_INLINE constexpr static void IncrementSwapCountForTest() {}
  PA_ALWAYS_INLINE constexpr static void IncrementLessCountForTest() {}
};

}  // namespace base::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_NOOP_IMPL_H_
