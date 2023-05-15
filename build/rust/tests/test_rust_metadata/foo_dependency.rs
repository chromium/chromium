// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reexporting the function should also work fine.
pub use transitive_dep::say_something as say_foo_directly;

pub fn say_foo() -> String {
    transitive_dep::say_something()
}
