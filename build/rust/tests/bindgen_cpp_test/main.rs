// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod ffi {
    #![allow(dead_code)]
    #![allow(non_snake_case)]
    #![allow(non_camel_case_types)]
    #![allow(non_upper_case_globals)]
    include!(env!("BINDGEN_RS_FILE"));
    pub use root::*;
}

pub fn main() {
    let from_cpp = unsafe { ffi::functions::normal_fn(ffi::functions::kNumber) };
    println!("2 == {from_cpp}");
    assert_eq!(2, from_cpp);
}
