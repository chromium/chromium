// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_is_test.h"

#include "base/check.h"
#include "base/logging.h"

namespace {
bool g_this_is_a_test = false;
}

namespace base::internal {
void check_is_test_impl(base::NotFatalUntil fatal_milestone) {
  CHECK(g_this_is_a_test, fatal_milestone);
}

// static
bool IsInTest::Get() {
  return g_this_is_a_test;
}
}  // namespace base::internal

namespace base::test {
// base/test/allow_check_is_test_for_testing.h declares
// `AllowCheckIsTestForTesting`, but is only allowed to be included in test
// code. We therefore have to also mark the symbol as exported here.
BASE_EXPORT void AllowCheckIsTestForTesting() {
  // This CHECK ensures that `AllowCheckIsTestForTesting` is called
  // just once. Since it is called in `base::TestSuite`, this should effectively
  // prevent calls to `AllowCheckIsTestForTesting` in production code
  // (assuming that code has unit test coverage).
  //
  // This is just in case someone ignores the fact that this function in the
  // `base::test` namespace and ends on "ForTesting".
  CHECK(!g_this_is_a_test)
      << "AllowCheckIsTestForTesting must not be called more than once";

  g_this_is_a_test = true;
}
}  // namespace base::test
