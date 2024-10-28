// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/bindgen_test:c_lib_bindgen";
}

pub fn add_two_numbers_in_c(a: u32, b: u32) -> u32 {
    unsafe { c_lib_bindgen::add_two_numbers(a, b) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add_two_numbers() {
        assert_eq!(add_two_numbers_in_c(5, 10), 15);
    }
}
