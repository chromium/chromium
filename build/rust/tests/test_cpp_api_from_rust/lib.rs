// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Public function which multiplies two integers.
/// Used to test the infrastructure for generating C++ bindings to public Rust
/// APIs.
pub fn mul_two_ints_via_rust(x: i32, y: i32) -> i32 {
    x * y
}
