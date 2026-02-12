// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_cpp_api_from_rust/rust_lib.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/rust-toolchain/lib/crubit/support/rs_std/char.h"

TEST(RustCcBindingsFromRs, TestI32) {
  // TODO(crbug.com/470466915): Stop leaking mangled crate name via the
  // namespace of the generated bindings.
  EXPECT_EQ(12, rust_lib_1dc874e1::mul_two_ints_via_rust(3, 4));
}

TEST(RustCcBindingsFromRs, TestChar) {
  // TODO(crbug.com/470466915): Stop leaking mangled crate name via the
  // namespace of the generated bindings.

  char cpp_char = 'x';
  uint32_t cpp_char_as_int = static_cast<uint32_t>(cpp_char);
  ::rs_std::char_ rust_char =
      rust_lib_1dc874e1::get_ascii_char_or_panic(cpp_char_as_int);
  uint32_t rust_char_as_int = static_cast<uint32_t>(rust_char);
  EXPECT_EQ(rust_char_as_int, cpp_char_as_int);
}
