// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use test_mixed_source_set_rs::say_hello_from_a_cpp_callback_from_rust;
use test_rust_source_set::say_hello;

fn main() {
    say_hello();
    say_hello_from_a_cpp_callback_from_rust();
}
