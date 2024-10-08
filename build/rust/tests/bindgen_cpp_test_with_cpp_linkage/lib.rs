// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[allow(dead_code)]
#[allow(non_snake_case)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
mod ffi {
    include!(env!("BINDGEN_RS_FILE"));
    pub use root::*;
}

#[no_mangle]
pub fn rust_main() {
    let from_cpp = unsafe { ffi::functions::normal_fn(ffi::functions::kNumber) };
    println!("2 == {from_cpp}");
    assert_eq!(2, from_cpp);
}
