// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_metadata:lib";
}

#[test]
fn test_expected_outputs() {
    assert_eq!(lib::say_foo(), "foo");
    assert_eq!(lib::say_foo_directly(), "foo");
    assert_eq!(lib::say_something(), "bar");
}
