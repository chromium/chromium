// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("build/rust/tests/test_mixed_source_set/test_mixed_source_set.h");
        fn cpp_callback();
        fn cpp_addition(a: u32, b: u32) -> u32;
    }

    extern "Rust" {
        fn say_hello_from_a_cpp_callback_from_rust();
        fn add_two_ints_via_rust_then_cpp(a: u32, b: u32) -> u32;
    }
}

pub fn say_hello_from_a_cpp_callback_from_rust() {
    ffi::cpp_callback();
}

pub fn add_two_ints_via_rust_then_cpp(a: u32, b: u32) -> u32 {
    add_two_ints_using_cpp(a, b)
}

pub fn add_two_ints_using_cpp(a: u32, b: u32) -> u32 {
    ffi::cpp_addition(a, b)
}

#[test]
fn test_callback_to_cpp() {
    say_hello_from_a_cpp_callback_from_rust();
    assert_eq!(add_two_ints_via_rust_then_cpp(4u32, 4u32), 8u32);
}
