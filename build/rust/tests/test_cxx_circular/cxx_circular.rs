// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests that we can have cxx bindings which both provide types to
// a C++ shim, and import C++ functions from that shim.

// The purpose of this test is just to make sure everything builds, and
// `gn check` passes. We don't actually need to run any of this code
#![allow(unused)]

// Simple type to send to C++, and a function with which to use it there
pub struct RustI32(i32);

impl RustI32 {
    pub fn get(&self) -> i32 {
        self.0
    }
}

#[cxx::bridge(namespace = "cxx_circular_test")]
mod ffi {
    extern "Rust" {
        type RustI32;

        fn get(&self) -> i32;
    }

    unsafe extern "C++" {
        include!("build/rust/tests/test_cxx_circular/cxx_circular_shim.h");

        fn AddRustI32s(n1: &RustI32, n2: &RustI32) -> i32;
    }
}
