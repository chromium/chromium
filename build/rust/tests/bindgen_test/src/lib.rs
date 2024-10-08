// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[allow(dead_code)]
#[allow(non_snake_case)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
mod c_ffi {
    include!(env!("BINDGEN_RS_FILE"));
}

pub fn add_two_numbers_in_c(a: u32, b: u32) -> u32 {
    unsafe { c_ffi::add_two_numbers(a, b) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add_two_numbers() {
        assert_eq!(add_two_numbers_in_c(5, 10), 15);
    }
}
