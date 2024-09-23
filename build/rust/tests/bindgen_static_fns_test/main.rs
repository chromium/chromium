// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/bindgen_static_fns_test:bindgen_static_fns_test_lib";
}

use bindgen_static_fns_test_lib::mul_three_numbers_in_c;

fn main() {
    println!("{} * {} * {} = {}", 3, 7, 11, mul_three_numbers_in_c(3, 7, 11));
}
