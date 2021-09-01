// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[no_mangle]
pub extern "C" fn say_hello_from_cpp() {
    say_hello();
}

pub fn say_hello() {
    println!("Hello, world - from a Rust library. Calculations suggest that 3+4={}", do_maths());
}

fn do_maths() -> i32 {
    3 + 4
}

#[test]
fn test_hello() {
    assert_eq!(7, do_maths());
}
