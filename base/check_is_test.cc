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
void check_is_test_impl() {
  CHECK(g_this_is_a_test);
}
}  // namespace base::internal

namespace base::test {
// base/test/allow_check_is_test_to_be_called.h declares
// `AllowCheckIsTestToBeCalled`, but is only allowed to be included in test
// code.
// We therefore have to also mark the symbol as exported here.
BASE_EXPORT void AllowCheckIsTestToBeCalled() {
  LOG(WARNING) << "Allowing special test code paths";
  // This CHECK ensures that `AllowCheckIsTestToBeCalled` is called just once.
  // Since it is called in `base::TestSuite`, this should effectivly prevent
  // calls to AllowCheckIsTestToBeCalled in production code (assuming that code
  // has unit test coverage).
  //
  // This is just in case someone ignores the fact that this function in the
  // `base::test` namespace and ends on "ForTesting".
  CHECK(!g_this_is_a_test)
      << "AllowCheckIsTestToBeCalled must not be called more than once";

  g_this_is_a_test = true;
}

}  // namespace base::test
