// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp/mutual_recursion:is_odd";
    "//build/rust/tests/test_rust_api_from_cpp/mutual_recursion:parity_headers_rs_api"
        as parity;
}

use ::parity::{IsEven, IsOdd};
use rust_gtest_interop::prelude::*;

#[gtest(RustApiFromCppMutualRecursion, IsEven)]
fn test_is_even() {
    expect_false!(IsEven(1));
    expect_true!(IsEven(2));
}

#[gtest(RustApiFromCppMutualRecursion, IsOddViaCpp)]
fn test_is_odd_via_cpp() {
    expect_true!(IsOdd(1));
    expect_false!(IsOdd(2));
}

#[gtest(RustApiFromCppMutualRecursion, IsOddViaRust)]
fn test_is_odd_via_rust() {
    expect_true!(::is_odd::is_odd(1));
    expect_false!(::is_odd::is_odd(2));
}
