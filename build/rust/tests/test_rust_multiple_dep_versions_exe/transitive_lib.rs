// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// To mimic third-party, the `test_lib` crate has a short name which does not
// need to be import!ed.

pub fn transitively_say_hello() {
    test_lib::say_hello_from_v2();
}
