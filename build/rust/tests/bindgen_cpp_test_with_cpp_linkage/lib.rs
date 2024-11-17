// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/bindgen_cpp_test_with_cpp_linkage:cpp_lib_bindgen";
}

#[no_mangle]
pub fn rust_main() {
    let from_cpp =
        unsafe { cpp_lib_bindgen::functions::normal_fn(cpp_lib_bindgen::functions::kNumber) };
    println!("2 == {from_cpp}");
    assert_eq!(2, from_cpp);
}
