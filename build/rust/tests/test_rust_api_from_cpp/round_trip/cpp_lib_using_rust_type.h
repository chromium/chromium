// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_ROUND_TRIP_CPP_LIB_USING_RUST_TYPE_H_
#define BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_ROUND_TRIP_CPP_LIB_USING_RUST_TYPE_H_

#include "build/rust/tests/test_rust_api_from_cpp/round_trip/rust_lib_with_rust_type.h"

inline rust_lib_with_rust_type::RustStruct DoubleRustStruct(
    rust_lib_with_rust_type::RustStruct s) {
  return rust_lib_with_rust_type::RustStruct::new_(s.value() * 2);
}

#endif  // BUILD_RUST_TESTS_TEST_RUST_API_FROM_CPP_ROUND_TRIP_CPP_LIB_USING_RUST_TYPE_H_
