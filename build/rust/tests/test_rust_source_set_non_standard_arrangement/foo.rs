// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub extern "C" fn say_foo() {
    println!("Foo");
}

#[test]
fn test_ok() {
    assert_eq!(42, 42);
}
