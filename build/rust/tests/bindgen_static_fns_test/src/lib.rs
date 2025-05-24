// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/bindgen_static_fns_test:c_lib_bindgen";
}

pub fn mul_three_numbers_in_c(a: u32, b: u32, c: u32) -> u32 {
    unsafe { c_lib_bindgen::mul_three_numbers(a, b, c) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mul_three_numbers() {
        assert_eq!(mul_three_numbers_in_c(5, 10, 15), 750);
    }
}
