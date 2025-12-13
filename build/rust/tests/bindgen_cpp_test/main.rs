// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/bindgen_cpp_test:cpp_lib_bindgen";
}

use cpp_lib_bindgen::root::functions;

pub fn main() {
    let from_cpp = unsafe { functions::normal_fn(functions::kNumber) };
    println!("2 == {from_cpp}");
    assert_eq!(2, from_cpp);
}
