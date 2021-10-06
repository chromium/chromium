// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[no_mangle]
pub extern "C" fn say_hello_from_cpp() {
    say_hello();
}

pub fn say_hello() {
    println!(
        "Hello, world - from a Rust library. Calculations suggest that 3+4={}",
        add_two_ints_via_rust(3, 4)
    );
}

#[test]
fn test_hello() {
    assert_eq!(7, add_two_ints_via_rust(3, 4));
}

// The extern "C" functions below are used by GTest tests from the same C++ test
// executable which is why for now they're all in this same file.

// The next 2 functions are used from the
// AllocatorTest.RustComponentUsesPartitionAlloc unit test.
#[no_mangle]
pub extern "C" fn allocate_via_rust() -> *mut i32 {
    Box::into_raw(Box::new(123_i32))
}
#[no_mangle]
pub unsafe extern "C" fn deallocate_via_rust(ptr: *mut i32) {
    Box::from_raw(ptr);
}

// The next function is used from FfiTest.CppCallingIntoRust unit test.
#[no_mangle]
pub extern "C" fn add_two_ints_via_rust(x: i32, y: i32) -> i32 {
    x + y
}
