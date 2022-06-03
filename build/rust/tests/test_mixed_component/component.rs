// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn rust_math(a: u32, b: u32) -> u32;
    }
}

fn rust_math(a: u32, b: u32) -> u32 {
    a.checked_add(b).expect("Oh noes! Integer overflow")
}
