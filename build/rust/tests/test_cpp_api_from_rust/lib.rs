// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Public function which multiplies two integers.
///
/// Used to test the infrastructure for generating C++ bindings to public Rust
/// APIs.
pub fn mul_two_ints_via_rust(x: i32, y: i32) -> i32 {
    x * y
}

/// Public function that returns a Rust `char`.
///
/// Used to test `crubit/support/...` libraries.  In particular, support for
/// `char` depends on `crubit/support/rs_std/char.h`.
pub fn get_ascii_char_or_panic(code: u8) -> char {
    let c = char::from_u32(code as u32).unwrap();
    assert!(c.is_ascii());
    c
}
