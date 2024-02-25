// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/bindgen_test:bindgen_test_lib";
}

use bindgen_test_lib::add_two_numbers_in_c;

fn main() {
    println!("{} + {} = {}", 3, 7, add_two_numbers_in_c(3, 7));
}
