// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp:assume_lifetimes_rs_api"
        as assume_lifetimes;
    "//build/rust/tests/test_rust_api_from_cpp:explicit_lifetimes_rs_api"
        as explicit_lifetimes;
    "//build/rust/tests/test_rust_api_from_cpp:lifetime_bound_rs_api"
        as lifetime_bound;
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

#[gtest(TestRustApiFromCpp, AssumeLifetimes)]
fn test_assume_lifetimes() {
    // By default, C++ methods take `&self` or `&mut self` in Rust (and
    // therefore can be called with the method-call syntax).
    let mut x = ::assume_lifetimes::AssumeLifetimesStruct { value: 123 };
    assert_eq!(x.GetValue(), 123);
    x.SetValue(456);
    assert_eq!(x.GetValue(), 456);
}

#[gtest(TestRustApiFromCpp, LifetimeBound)]
fn test_lifetime_bound() {
    // `LIFETIME_BOUND` annotations tie the result of `GetLargerValue` to both
    // `self` and `other`.  (Lifetime elision would only tie the result to
    // `self`, which would let the result outlive `other`.)
    let x = ::lifetime_bound::LifetimeBoundStruct { value: 123 };
    let other = 456;
    let larger = x.GetLargerValue(&other);
    assert!(std::ptr::eq(::cref::CRef::as_ptr(larger), &other));
}

#[gtest(TestRustApiFromCpp, ExplicitLifetimes)]
fn test_explicit_lifetimes() {
    // Explicit lifetime annotations tie the result of `GetLargerValue` to
    // both `self` and `other`.  (Lifetime elision would only tie the result to
    // `self`, which would let the result outlive `other`.)
    let x = ::explicit_lifetimes::ExplicitLifetimesStruct { value: 123 };
    let other = 456;
    let larger = x.GetLargerValue(&other);
    assert!(std::ptr::eq(::cref::CRef::as_ptr(larger), &other));
}
