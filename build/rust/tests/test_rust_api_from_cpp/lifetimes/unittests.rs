// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp/lifetimes:assume_lifetimes_rs_api"
        as assume_lifetimes;
    "//build/rust/tests/test_rust_api_from_cpp/lifetimes:explicit_lifetimes_rs_api"
        as explicit_lifetimes;
    "//build/rust/tests/test_rust_api_from_cpp/lifetimes:lifetime_bound_rs_api"
        as lifetime_bound;
}

use rust_gtest_interop::prelude::*;

#[gtest(RustApiFromCppLifetimes, AssumeLifetimes)]
fn test_assume_lifetimes() {
    // By default, C++ methods take `&self` or `&mut self` in Rust (and
    // therefore can be called with the method-call syntax).
    let mut x = ::assume_lifetimes::AssumeLifetimesStruct { value: 123 };
    assert_eq!(x.GetValue(), 123);
    x.SetValue(456);
    assert_eq!(x.GetValue(), 456);
}

#[gtest(RustApiFromCppLifetimes, LifetimeBound)]
fn test_lifetime_bound() {
    // `LIFETIME_BOUND` annotations tie the result of `GetLargerValue` to both
    // `self` and `other`.  (Lifetime elision would only tie the result to
    // `self`, which would let the result outlive `other`.)
    let x = ::lifetime_bound::LifetimeBoundStruct { value: 123 };
    let other = 456;
    let larger = x.GetLargerValue(&other);
    assert!(std::ptr::eq(::cref::CRef::as_ptr(larger), &other));
}

#[gtest(RustApiFromCppLifetimes, ExplicitLifetimes)]
fn test_explicit_lifetimes() {
    // Explicit lifetime annotations tie the result of `GetLargerValue` to
    // both `self` and `other`.  (Lifetime elision would only tie the result to
    // `self`, which would let the result outlive `other`.)
    let x = ::explicit_lifetimes::ExplicitLifetimesStruct { value: 123 };
    let other = 456;
    let larger = x.GetLargerValue(&other);
    assert!(std::ptr::eq(::cref::CRef::as_ptr(larger), &other));
}
