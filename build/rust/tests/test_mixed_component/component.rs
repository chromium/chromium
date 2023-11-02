// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("build/rust/tests/test_mixed_component/component.h");
        fn get_a_string_from_cpp() -> String;
    }
    extern "Rust" {
        fn rust_math(a: u32, b: u32) -> u32;
        fn rust_get_an_uppercase_string() -> String;
    }
}

fn rust_math(a: u32, b: u32) -> u32 {
    a.checked_add(b).expect("Oh noes! Integer overflow")
}

fn rust_get_an_uppercase_string() -> String {
    ffi::get_a_string_from_cpp().to_uppercase()
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_math() {
        assert_eq!(crate::rust_math(33, 3), 36)
    }

    #[test]
    fn test_string() {
        assert_eq!(crate::ffi::get_a_string_from_cpp(), "Mixed Case String")
    }
}
