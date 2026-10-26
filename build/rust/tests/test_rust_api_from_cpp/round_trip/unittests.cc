// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "build/rust/tests/test_rust_api_from_cpp/round_trip/rust_lib_using_cpp_type.h"
#include "build/rust/tests/test_rust_api_from_cpp/self_contained_target_header2.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests round-tripping a C++ type through `rust_api_from_cpp` and then
// `cpp_api_from_rust`.
TEST(RustApiFromCppRoundTrip, CppTypeUsedInRustApi) {
  auto result =
      rust_lib_using_cpp_type::double_cc_pod_struct(CcPodStruct{.value = 21});
  static_assert(std::is_same_v<decltype(result), CcPodStruct>);
  EXPECT_EQ(42, result.value);
}
