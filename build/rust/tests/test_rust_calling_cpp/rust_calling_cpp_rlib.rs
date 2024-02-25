// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn rust_calling_cpp();
    }

    unsafe extern "C++" {
        include!("build/rust/tests/test_rust_calling_cpp/cpp_library.h");

        fn mul_by_2_in_cpp_library(a: i32) -> i32;
    }
}

#[no_mangle]
pub fn rust_calling_cpp() {
    assert_eq!(ffi::mul_by_2_in_cpp_library(3), 6);
}
