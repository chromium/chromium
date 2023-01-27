// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_TEST_SUPPORT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_TEST_SUPPORT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Struct intended to be used with designated initializers and passed
// to the `CountersMatch()` matcher.
//
// `CountingImplType` isn't used directly; it tells the `CountersMatch`
// matcher which impl's static members should be checked.
template <typename CountingImplType>
struct CountingRawPtrExpectations {
  absl::optional<int> wrap_raw_ptr_cnt;
  absl::optional<int> release_wrapped_ptr_cnt;
  absl::optional<int> get_for_dereference_cnt;
  absl::optional<int> get_for_extraction_cnt;
  absl::optional<int> get_for_comparison_cnt;
  absl::optional<int> wrapped_ptr_swap_cnt;
  absl::optional<int> wrapped_ptr_less_cnt;
  absl::optional<int> pointer_to_member_operator_cnt;
  absl::optional<int> wrap_raw_ptr_for_dup_cnt;
  absl::optional<int> get_for_duplication_cnt;
};

#define REPORT_UNEQUAL_RAW_PTR_COUNTER(member_name, CounterClassImpl) \
  {                                                                   \
    if (arg.member_name.has_value() &&                                \
        arg.member_name.value() != CounterClassImpl::member_name) {   \
      *result_listener << "Expected `" #member_name "` to be "        \
                       << arg.member_name.value() << " but got "      \
                       << CounterClassImpl::member_name << "; ";      \
      result = false;                                                 \
    }                                                                 \
  }
#define REPORT_UNEQUAL_RAW_PTR_COUNTERS(result, CounterClassImpl)              \
  {                                                                            \
    result = true;                                                             \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(wrap_raw_ptr_cnt, CounterClassImpl)         \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(release_wrapped_ptr_cnt, CounterClassImpl)  \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(get_for_dereference_cnt, CounterClassImpl)  \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(get_for_extraction_cnt, CounterClassImpl)   \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(get_for_comparison_cnt, CounterClassImpl)   \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(wrapped_ptr_swap_cnt, CounterClassImpl)     \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(wrapped_ptr_less_cnt, CounterClassImpl)     \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(pointer_to_member_operator_cnt,             \
                                   CounterClassImpl)                           \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(wrap_raw_ptr_for_dup_cnt, CounterClassImpl) \
    REPORT_UNEQUAL_RAW_PTR_COUNTER(get_for_duplication_cnt, CounterClassImpl)  \
  }

// Matcher used with `CountingRawPtr`. Provides slightly shorter
// boilerplate for verifying counts. This inner function is detached
// from the `MATCHER` to isolate the templating.
template <typename CountingImplType>
bool CountersMatchImpl(const CountingRawPtrExpectations<CountingImplType>& arg,
                       testing::MatchResultListener* result_listener) {
  bool result = true;
  REPORT_UNEQUAL_RAW_PTR_COUNTERS(result, CountingImplType);
  return result;
}

// Implicit `arg` has type `CountingRawPtrExpectations`, specialized for
// the specific counting impl.
MATCHER(CountersMatch, "counting impl has specified counters") {
  return CountersMatchImpl(arg, result_listener);
}

#undef REPORT_UNEQUAL_RAW_PTR_COUNTERS
#undef REPORT_UNEQUAL_RAW_PTR_COUNTER

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_TEST_SUPPORT_H_
