// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp/round_trip:cpp_lib_using_rust_type_rs_api"
        as cpp_lib_using_rust_type;
    "//build/rust/tests/test_rust_api_from_cpp/round_trip:rust_lib_with_rust_type";
}

use rust_gtest_interop::prelude::*;
use rust_lib_with_rust_type::RustStruct;

/// Tests round-tripping a Rust type through `cpp_api_from_rust` and then
/// `rust_api_from_cpp`.
#[gtest(RustApiFromCppRoundTrip, RustTypeUsedInCppApi)]
fn test_rust_type_used_in_cpp_api() {
    let result: RustStruct = ::cpp_lib_using_rust_type::DoubleRustStruct(RustStruct::new(21));
    assert_eq!(result.value(), 42);
}
