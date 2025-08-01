// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This focuses on testing if `#[cfg(...)]` works inside `#[cxx::bridge]`.
//! For that to work, the appropriate `--cfg=foo=bar` configuration values
//! have to be passed into `cxxbridge-cmd` invocation.

// TODO(https://crbug.com/435437947): This should also cover additional configuration conditions:
//
// * Ones without `=` like `unix`
// * Ones associated with crate features (maybe;  we don't provide `cxx`
//   integration for `cargo_crate` targets...)
// * Ones associated with plain-vanilla `--cfg=...` set via `rustflags` of
//   `rust_static_library` or of `config`.

#[cxx::bridge(namespace = "rust_test")]
mod ffi {
    extern "Rust" {
        // TODO(https://crbug.com/435437947): Also handle + test the spelling of
        // `#[cfg(unix)]`.  This requires knowing somehow (?) that `unix=false`
        // for Windows target triples :-/
        #[cfg(target_family = "unix")]
        fn double_unix_value(x: u32) -> u32;

        #[cfg(not(target_family = "unix"))]
        fn double_non_unix_value(x: u32) -> u32;
    }
}

#[cfg(target_family = "unix")]
fn double_unix_value(x: u32) -> u32 {
    x * 2
}

#[cfg(not(target_family = "unix"))]
fn double_non_unix_value(x: u32) -> u32 {
    x + x
}
