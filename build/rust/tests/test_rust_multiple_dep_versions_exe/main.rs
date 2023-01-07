// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

fn main() {
    test_lib::say_hello_from_v1();
    transitive_v2::transitively_say_hello();
}
