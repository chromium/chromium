// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp:self_contained_target_rs_api";
}

use rust_gtest_interop::prelude::*;

#[gtest(TestRustApiFromCpp, FunctionCalls)]
fn test_self_contained_target_function_call_basics() {
    assert_eq!(100 + 42, ::self_contained_target_rs_api::AddViaCc(100, 42));
    assert_eq!(100 * 42, ::self_contained_target_rs_api::MultiplyViaCc(100, 42));
}

#[gtest(TestRustApiFromCpp, PodStruct)]
fn test_self_contained_target_pod_struct_basics() {
    let x = ::self_contained_target_rs_api::CcPodStruct { value: 123 };
    assert_eq!(x.value, 123);
}
