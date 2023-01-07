// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn print_message_from_rust();
    }
}

fn print_message_from_rust() {
    println!("Here is a message from Rust.")
}
