// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ref.h"

namespace {

#if defined(NCTEST_CROSS_KIND_CONVERSION_FROM_MAY_DANGLE) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)4U == \(\(partition_alloc::internal::RawPtrTraits\)5U | RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  int x = 123;
  raw_ref<int, base::RawPtrTraits::kMayDangle> ref(x);
  [[maybe_unused]] raw_ref<int> ref2(ref);
}

#elif defined(NCTEST_CROSS_KIND_CONVERSION_FROM_DUMMY) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)5U == \(\(partition_alloc::internal::RawPtrTraits\)2052U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  int x = 123;
  raw_ref<int, base::RawPtrTraits::kDummyForTest> ref(x);
  [[maybe_unused]] raw_ref<int, base::RawPtrTraits::kMayDangle> ref2(ref);
}

#elif defined(NCTEST_CROSS_KIND_CONVERSION_MOVE_FROM_MAY_DANGLE) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)4U == \(\(partition_alloc::internal::RawPtrTraits\)5U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  int x = 123;
  raw_ref<int, base::RawPtrTraits::kMayDangle> ref(x);
  [[maybe_unused]] raw_ref<int> ref2(std::move(ref));
}

#elif defined(NCTEST_CROSS_KIND_CONVERSION_MOVE_FROM_DUMMY) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)5U == \(\(partition_alloc::internal::RawPtrTraits\)2052U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  int x = 123;
  raw_ref<int, base::RawPtrTraits::kDummyForTest> ref(x);
  [[maybe_unused]] raw_ref<int, base::RawPtrTraits::kMayDangle> ref2(std::move(ref));
}

#elif defined(NCTEST_CROSS_KIND_ASSIGNMENT_FROM_MAY_DANGLE) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)4U == \(\(partition_alloc::internal::RawPtrTraits\)5U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  int x = 123;
  raw_ref<int, base::RawPtrTraits::kMayDangle> ref(x);
  raw_ref<int> ref2(x);
  ref2 = ref;
}

#elif defined(NCTEST_CROSS_KIND_ASSIGNMENT_FROM_DUMMY) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)5U == \(\(partition_alloc::internal::RawPtrTraits\)2052U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  int x = 123;
  raw_ref<int, base::RawPtrTraits::kDummyForTest> ref(x);
  raw_ref<int, base::RawPtrTraits::kMayDangle> ref2(x);
  ref2 = ref;
}

#elif defined(NCTEST_CROSS_KIND_ASSIGNMENT_MOVE_FROM_MAY_DANGLE) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)4U == \(\(partition_alloc::internal::RawPtrTraits\)5U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  int x = 123;
  raw_ref<int, base::RawPtrTraits::kMayDangle> ref(x);
  raw_ref<int> ref2(x);
  ref2 = std::move(ref);
}

#elif defined(NCTEST_CROSS_KIND_ASSIGNMENT_MOVE_FROM_DUMMY) // [r"static assertion failed due to requirement '\(partition_alloc::internal::RawPtrTraits\)5U == \(\(partition_alloc::internal::RawPtrTraits\)2052U \| RawPtrTraits::kMayDangle\)'"]

void WontCompile() {
  int x = 123;
  raw_ref<int, base::RawPtrTraits::kDummyForTest> ref(x);
  raw_ref<int, base::RawPtrTraits::kMayDangle> ref2(x);
  ref2 = std::move(ref);
}

#endif

}  // namespace
