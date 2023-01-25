// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_HOOKABLE_IMPL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_HOOKABLE_IMPL_H_

#include <stddef.h>

#include <type_traits>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"

#if !BUILDFLAG(USE_HOOKABLE_RAW_PTR)
#error "Included under wrong build option"
#endif

namespace base::internal {

struct RawPtrHooks {
  using WrapPtr = void(uintptr_t address);
  using ReleaseWrappedPtr = void(uintptr_t address);
  using SafelyUnwrapForDereference = void(uintptr_t address);
  using SafelyUnwrapForExtraction = void(uintptr_t address);
  using UnsafelyUnwrapForComparison = void(uintptr_t address);
  using Advance = void(uintptr_t old_address, uintptr_t new_address);
  using Duplicate = void(uintptr_t address);

  WrapPtr* wrap_ptr;
  ReleaseWrappedPtr* release_wrapped_ptr;
  SafelyUnwrapForDereference* safely_unwrap_for_dereference;
  SafelyUnwrapForExtraction* safely_unwrap_for_extraction;
  UnsafelyUnwrapForComparison* unsafely_unwrap_for_comparison;
  Advance* advance;
  Duplicate* duplicate;
};

PA_COMPONENT_EXPORT(RAW_PTR) const RawPtrHooks* GetRawPtrHooks();
PA_COMPONENT_EXPORT(RAW_PTR) void InstallRawPtrHooks(const RawPtrHooks*);
PA_COMPONENT_EXPORT(RAW_PTR) void ResetRawPtrHooks();

struct RawPtrHookableImpl {
  // Wraps a pointer.
  template <typename T>
  static PA_ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    GetRawPtrHooks()->wrap_ptr(reinterpret_cast<uintptr_t>(ptr));
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static PA_ALWAYS_INLINE void ReleaseWrappedPtr(T* ptr) {
    GetRawPtrHooks()->release_wrapped_ptr(reinterpret_cast<uintptr_t>(ptr));
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    GetRawPtrHooks()->safely_unwrap_for_dereference(
        reinterpret_cast<uintptr_t>(wrapped_ptr));
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    GetRawPtrHooks()->safely_unwrap_for_extraction(
        reinterpret_cast<uintptr_t>(wrapped_ptr));
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    GetRawPtrHooks()->unsafely_unwrap_for_comparison(
        reinterpret_cast<uintptr_t>(wrapped_ptr));
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static PA_ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <
      typename T,
      typename Z,
      typename =
          std::enable_if_t<partition_alloc::internal::offset_type<Z>, void>>
  static PA_ALWAYS_INLINE T* Advance(T* wrapped_ptr, Z delta_elems) {
    GetRawPtrHooks()->advance(
        reinterpret_cast<uintptr_t>(wrapped_ptr),
        reinterpret_cast<uintptr_t>(wrapped_ptr + delta_elems));
    return wrapped_ptr + delta_elems;
  }

  template <typename T>
  static PA_ALWAYS_INLINE ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                  T* wrapped_ptr2) {
    return wrapped_ptr1 - wrapped_ptr2;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    GetRawPtrHooks()->duplicate(reinterpret_cast<uintptr_t>(wrapped_ptr));
    return wrapped_ptr;
  }

  // `WrapRawPtrForDuplication` and `UnsafelyUnwrapPtrForDuplication` are used
  // to create a new raw_ptr<T> from another raw_ptr<T> of a different flavor.
  template <typename T>
  static PA_ALWAYS_INLINE T* WrapRawPtrForDuplication(T* ptr) {
    return ptr;
  }

  template <typename T>
  static PA_ALWAYS_INLINE T* UnsafelyUnwrapPtrForDuplication(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static PA_ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementLessCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}
};

}  // namespace base::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_HOOKABLE_IMPL_H_
