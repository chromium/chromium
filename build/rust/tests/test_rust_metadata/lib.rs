// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The explicit `extern crate` will catch if a GN target specifies conflicting
// dependencies.
//
// When libraries are included implicitly from the command line, rustc seems to
// silently pick the first one that matches. On the other hand with an explicit
// "extern crate" directive, which tells rustc to link a dependency no matter
// what, rustc will see the conflict.
extern crate transitive_dep;

chromium::import! {
    "//build/rust/tests/test_rust_metadata:foo_dependency";
}

pub use foo_dependency::say_foo;
pub use foo_dependency::say_foo_directly;
pub use transitive_dep::say_something;

#[no_mangle]
pub extern "C" fn print_foo_bar() {
    println!("{}", foo_dependency::say_foo());
    println!("{}", transitive_dep::say_something());
}
