// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp:self_contained_target_rs_api"
        as self_contained_target;
    "//build/rust/tests/test_rust_api_from_cpp:target_depending_on_another_rs_api"
        as target_depending_on_another;
}

use rust_gtest_interop::prelude::*;

#[gtest(TestRustApiFromCpp, FunctionCalls)]
fn test_self_contained_target_function_call_basics() {
    assert_eq!(100 + 42, ::self_contained_target::AddViaCc(100, 42));
    assert_eq!(100 * 42, ::self_contained_target::MultiplyViaCc(100, 42));
}

#[gtest(TestRustApiFromCpp, PodStruct)]
fn test_self_contained_target_pod_struct_basics() {
    let x = ::self_contained_target::CcPodStruct { value: 123 };
    assert_eq!(x.value, 123);
}

#[gtest(TestRustApiFromCpp, TargetDependingOnAnother)]
fn test_target_depending_on_another() {
    let x = ::target_depending_on_another::CreateCcPodStructFromValue(456);
    assert_eq!(x.value, 456);
}
