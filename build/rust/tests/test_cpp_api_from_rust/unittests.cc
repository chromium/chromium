// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/std/rules/alloc.h"
#include "build/rust/tests/test_cpp_api_from_rust/rust_lib.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/rust-toolchain/lib/crubit/support/rs_std/char.h"

TEST(RustCcBindingsFromRs, TestI32) {
  EXPECT_EQ(12, rust_lib::mul_two_ints_via_rust(3, 4));
}

TEST(RustCcBindingsFromRs, TestChar) {
  char cpp_char = 'x';
  uint32_t cpp_char_as_int = static_cast<uint32_t>(cpp_char);
  ::rs_std::char_ rust_char =
      rust_lib::get_ascii_char_or_panic(cpp_char_as_int);
  uint32_t rust_char_as_int = static_cast<uint32_t>(rust_char);
  EXPECT_EQ(rust_char_as_int, cpp_char_as_int);
}

TEST(RustCcBindingsFromRs, TransitiveDep) {
  auto multiplier = rust_lib::create_multiplier(5);
  EXPECT_EQ(30, multiplier.mul(6));
}

TEST(RustCcBindingsFromRs, DirectOnAlloc) {
  // Silly test to just check if the type is present in the generated bindings.
  rs::alloc::alloc::Layout* layout_ptr = nullptr;
  std::ignore = layout_ptr;
}

TEST(RustCcBindingsFromRs, TransitiveOnStandardLibrary) {
  rs::core::time::Duration duration = rust_lib::create_duration_from_seconds(7);
  EXPECT_EQ(7u, duration.as_secs());
}
