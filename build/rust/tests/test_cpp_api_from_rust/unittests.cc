// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_cpp_api_from_rust/rust_lib.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(RustCcBindingsFromRs, BasicTest) {
  // TODO(crbug.com/470466915): Stop leaking mangled crate name via the
  // namespace of the generated bindings.
  EXPECT_EQ(12, rust_lib_1dc874e1::mul_two_ints_via_rust(3, 4));
}
