// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::env;

// This is a regression test against https://crbug.com/401427004 which
// reported that Chromium's build infrastructure (`run_build_script.py`)
// sets `CARGO_CFG_TARGET_ARCH` environment variable incorrectly.
fn main() {
    println!("cargo:rustc-cfg=build_script_ran");

    // https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-build-scripts
    // documents how `CARGO_CFG_TARGET_ARCH` should be set.
    let target = env::var("CARGO_CFG_TARGET_ARCH").unwrap();

    // https://doc.rust-lang.org/reference/conditional-compilation.html#target_arch lists the
    // following example values: "x86", "x86_64", "mips", "powerpc", "powerpc64",
    // "arm", "aarch64".
    //
    // Before https://crbug.com/401427004 was fixed `run_build_script.py` would incorrectly
    // take the first component of a target triple (e.g. `i686` from
    // `i686-pc-windows-msvc`) and set that as `CARGO_CFG_TARGET_ARCH`.
    assert_eq!(target, "x86");
}
