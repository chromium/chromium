// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/enum_set.h"

namespace base {
namespace {

#if defined(NCTEST_ALL_METHOD_DISALLOWED_ON_LARGE_SPARSE_ENUM) // [r"fatal error: static assertion failed due to requirement 'kValueCount <= 64': Max number of enum values is 64 for constexpr constructor"]

void WontCompile() {
  enum class TestEnumSparse {
    TEST_1 = 1,
    TEST_MIN = 1,
    TEST_50 = 50,
    TEST_100 = 100,
    TEST_MAX = TEST_100,
  };
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::TEST_MIN,
                                    TestEnumSparse::TEST_MAX>;

  // TestEnumSparseSet::All() does not compile because there are more than 64
  // possible values.
  TestEnumSparseSet::All();
}

#endif

}  // namespace
}  // namespace base
