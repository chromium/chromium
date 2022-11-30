// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge]
mod ffi {
    struct FooBars {
        foos: usize,
        bars: usize,
    }
    extern "Rust" {
        fn do_something_in_memory_safe_language(input: &CxxString) -> FooBars;
    }
}

pub fn do_something_in_memory_safe_language(input: &cxx::CxxString) -> ffi::FooBars {
    println!(
        "Memory safe language enabled: doing this operation without spinning up an extra process."
    );
    let s = input.to_string_lossy(); // discards any non-UTF8
    ffi::FooBars { foos: s.matches("foo").count(), bars: s.matches("bar").count() }
}
