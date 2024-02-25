// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_multiple_dep_versions_exe:transitive_v2";
}

// To mimic third-party, the `test_lib` crate has a short name which does not
// need to be import!ed.

fn main() {
    test_lib::say_hello_from_v1();
    transitive_v2::transitively_say_hello();
}
