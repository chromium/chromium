// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/check.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO(thakis): Remove _LIBCPP_ENABLE_ASSERTIONS here once
// pnacl-saigo's libc++ is new enough.
#if defined(__GLIBCXX__)
#if !defined(_GLIBCXX_ASSERTIONS)
#error "libstdc++ assertions should be enabled"
#endif
#elif !_LIBCPP_ENABLE_ASSERTIONS && \
    _LIBCPP_HARDENING_MODE != _LIBCPP_HARDENING_MODE_EXTENSIVE
#error "_LIBCPP_HARDENING_MODE not defined"
#endif

using ::testing::ContainsRegex;
using ::testing::Not;

// This test checks for two things:
//
// 0. Assertions are enabled for libc++ and cause the process to crash when
//    invoked (in this test's case, when an out of bounds access is made in
//    std::vector.
// 1. The correct assertion handler is linked in depending on whether or not
//    this test is built in debug mode. libc++ passes the string
//    {file}:{line} assertion {expression} failed: {message}. The default
//    libc++ handler, which we use in debug mode, prints this string to stderr,
//    while the nondebug assertion handler just crashes immediately. Therefore,
//    to check that we linked in the correct assertion handler, we check for the
//    presence or absence of the above string.
TEST(LibcppHardeningTest, Assertions) {
  std::vector<int> vec = {0, 1, 2};
#if CHECK_WILL_STREAM()
  EXPECT_DEATH_IF_SUPPORTED(vec[3], ".*assertion.*failed:");
#else
// We have to explicitly check for the GTEST_HAS_DEATH_TEST macro instead of
// using EXPECT_DEATH_IF_SUPPORTED(...) for the following reasons:
//
// 0. EXPECT_DEATH(...) does not support (non-escaped) parentheses in the regex,
//    so we can't use negative look arounds (https://stackoverflow.com/a/406408)
//    to check that the error message doesn't exist.
// 1. EXPECT_DEATH_IF_SUPPORTED(...) does not support having gmock matchers as
//    the second argument if GTEST_HAS_DEATH_TEST is false.
//
// We also have to prevent this test from running on Android because even though
// death tests are supported on Android, GTest death tests don't work with
// base::ImmediateCrash() (https://crbug.com/1353549#c2).
#if GTEST_HAS_DEATH_TEST && !GTEST_OS_LINUX_ANDROID
  EXPECT_DEATH(vec[3], Not(ContainsRegex(".*assertion.*failed:")));
#else
  GTEST_UNSUPPORTED_DEATH_TEST(vec[3], "", );
#endif  // GTEST_HAS_DEATH_TEST && !GTEST_OS_LINUX_ANDROID
#endif  // CHECK_WILL_STREAM()
}

}  // namespace
