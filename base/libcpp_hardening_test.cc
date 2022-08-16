// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if !_LIBCPP_ENABLE_ASSERTIONS
#error \
    "Define _LIBCPP_ENABLE_ASSERTIONS to 1 in \
buildtools/third_party/libc++/__config_site"

#endif

using ::testing::ContainsRegex;
using ::testing::Not;

TEST(LibcppHardeningTest, Assertions) {
  std::vector<int> vec = {0, 1, 2};
#ifdef NDEBUG
// We have to explicitly check for the GTEST_HAS_DEATH_TEST macro instead of
// using EXPECT_DEATH_IF_SUPPORTED(...) for the following reasons:
//
// 0. EXPECT_DEATH(...) does not support (non-escaped) parentheses in the regex,
//    so we can't use negative look arounds (https://stackoverflow.com/a/406408)
//    to check that the error message doesn't exist.
// 1. EXPECT_DEATH_IF_SUPPORTED(...) does not support having gmock matchers as
//    the second argument if GTEST_HAS_DEATH_TEST is false.
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(vec[3], Not(ContainsRegex(".*assertion.*failed:")));
#else
  GTEST_UNSUPPORTED_DEATH_TEST(vec[3], "", );
#endif  // GTEST_HAS_DEATH_TEST
#else
  EXPECT_DEATH_IF_SUPPORTED(vec[3], ".*assertion.*failed:");
#endif  // NDEBUG
}

}  // namespace
