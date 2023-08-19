// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_COUNTING_IMPL_WRAPPER_FOR_TEST_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_COUNTING_IMPL_WRAPPER_FOR_TEST_H_

#include <climits>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"

namespace base::test {

// Wraps a raw_ptr/raw_ref implementation with a class of the same interface
// that provides accounting for test purposes. A raw_ptr/raw_ref that uses it
// performs extra bookkeeping, e.g. to track the number of times the raw_ptr
// is wrapped, unrwapped, etc.
//
// Test only.
template <RawPtrTraits Traits>
struct RawPtrCountingImplWrapperForTest
    : public raw_ptr_traits::ImplForTraits<Traits> {
  static_assert(
      !raw_ptr_traits::Contains(Traits,
                                RawPtrTraits::kUseCountingWrapperForTest));

  using SuperImpl = typename raw_ptr_traits::ImplForTraits<Traits>;

  static constexpr bool kMustZeroOnInit = SuperImpl::kMustZeroOnInit;
  static constexpr bool kMustZeroOnMove = SuperImpl::kMustZeroOnMove;
  static constexpr bool kMustZeroOnDestruct = SuperImpl::kMustZeroOnDestruct;

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtr(T* ptr) {
    ++wrap_raw_ptr_cnt;
    return SuperImpl::WrapRawPtr(ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr void ReleaseWrappedPtr(T* ptr) {
    ++release_wrapped_ptr_cnt;
    SuperImpl::ReleaseWrappedPtr(ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForDereference(
      T* wrapped_ptr) {
    ++get_for_dereference_cnt;
    return SuperImpl::SafelyUnwrapPtrForDereference(wrapped_ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForExtraction(
      T* wrapped_ptr) {
    ++get_for_extraction_cnt;
    return SuperImpl::SafelyUnwrapPtrForExtraction(wrapped_ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForComparison(
      T* wrapped_ptr) {
    ++get_for_comparison_cnt;
    return SuperImpl::UnsafelyUnwrapPtrForComparison(wrapped_ptr);
  }

  PA_ALWAYS_INLINE static constexpr void IncrementSwapCountForTest() {
    ++wrapped_ptr_swap_cnt;
  }

  PA_ALWAYS_INLINE static constexpr void IncrementLessCountForTest() {
    ++wrapped_ptr_less_cnt;
  }

  PA_ALWAYS_INLINE static constexpr void
  IncrementPointerToMemberOperatorCountForTest() {
    ++pointer_to_member_operator_cnt;
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtrForDuplication(T* ptr) {
    ++wrap_raw_ptr_for_dup_cnt;
    return SuperImpl::WrapRawPtrForDuplication(ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForDuplication(
      T* wrapped_ptr) {
    ++get_for_duplication_cnt;
    return SuperImpl::UnsafelyUnwrapPtrForDuplication(wrapped_ptr);
  }

  static constexpr void ClearCounters() {
    wrap_raw_ptr_cnt = 0;
    release_wrapped_ptr_cnt = 0;
    get_for_dereference_cnt = 0;
    get_for_extraction_cnt = 0;
    get_for_comparison_cnt = 0;
    wrapped_ptr_swap_cnt = 0;
    wrapped_ptr_less_cnt = 0;
    pointer_to_member_operator_cnt = 0;
    wrap_raw_ptr_for_dup_cnt = 0;
    get_for_duplication_cnt = 0;
  }

  static inline int wrap_raw_ptr_cnt = INT_MIN;
  static inline int release_wrapped_ptr_cnt = INT_MIN;
  static inline int get_for_dereference_cnt = INT_MIN;
  static inline int get_for_extraction_cnt = INT_MIN;
  static inline int get_for_comparison_cnt = INT_MIN;
  static inline int wrapped_ptr_swap_cnt = INT_MIN;
  static inline int wrapped_ptr_less_cnt = INT_MIN;
  static inline int pointer_to_member_operator_cnt = INT_MIN;
  static inline int wrap_raw_ptr_for_dup_cnt = INT_MIN;
  static inline int get_for_duplication_cnt = INT_MIN;
};

}  // namespace base::test

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_COUNTING_IMPL_WRAPPER_FOR_TEST_H_
