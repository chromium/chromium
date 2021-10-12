// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use test_mixed_source_set_rs::add_two_ints_using_cpp;

#[test]
fn test_call_into_mixed_source_set() {
    assert_eq!(add_two_ints_using_cpp(5, 7), 12)
}
