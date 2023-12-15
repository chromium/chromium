// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![feature(test)]
extern crate test;

chromium::import! {
    "//build/rust/tests/test_rust_static_library";
}

use test::Bencher;
use test_rust_static_library::add_two_ints_via_rust;

#[test]
fn test_call_into_mixed_static_library() {
    assert_eq!(add_two_ints_via_rust(5, 7), 12)
}

#[allow(soft_unstable)]
#[bench]
fn test_benchmark(b: &mut Bencher) {
    b.iter(|| 2 + 2);
}
