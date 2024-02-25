// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_aliased_deps";
}

fn main() {
    test_aliased_deps::other_name::hello_world();
}
