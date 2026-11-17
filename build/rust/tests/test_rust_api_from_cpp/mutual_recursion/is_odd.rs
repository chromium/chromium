// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! The Rust implementation of `parity::IsOdd` from `parity.h`.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp/mutual_recursion:parity_headers_rs_api"
        as parity;
}

/// Returns true if `n` is odd.  Requires `n >= 0`.
///
/// `parity::IsOdd` (in `parity.cc`) calls this through its C++ bindings, and
/// this calls back into the C++ `parity::IsEven`.
pub fn is_odd(n: i32) -> bool {
    if n == 0 {
        return false;
    }
    ::parity::IsEven(n - 1)
}
