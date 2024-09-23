// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod c_ffi {
    #![allow(dead_code)]
    #![allow(non_snake_case)]
    #![allow(non_camel_case_types)]
    #![allow(non_upper_case_globals)]
    include!(env!("BINDGEN_RS_FILE"));
}

pub fn mul_three_numbers_in_c(a: u32, b: u32, c: u32) -> u32 {
    unsafe { c_ffi::mul_three_numbers(a, b, c) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mul_three_numbers() {
        assert_eq!(mul_three_numbers_in_c(5, 10, 15), 750);
    }
}
