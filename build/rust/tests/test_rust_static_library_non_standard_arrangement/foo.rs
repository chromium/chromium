// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub extern "C" fn do_subtract(a: u32, b: u32) -> u32 {
    a - b
}

#[test]
fn test_ok() {
    assert_eq!(do_subtract(12, 8), 4)
}
