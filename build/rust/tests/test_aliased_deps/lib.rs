// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub use other_name;

#[cfg(test)]
#[test]
fn test_add_from_renamed_dep() {
    assert_eq!(other_name::add(2, 3), 5);
}
