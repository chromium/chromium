// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Helpers that tests authored in C++ can use for initiating `log::info!`,
//! `panic!`, etc.  ! on the Rust side.

#[cxx::bridge(namespace = "base::test")]
mod ffi {
    extern "Rust" {
        fn log_info_from_rust();
        fn log_warning_from_rust();
        fn log_error_from_rust();
        fn log_debug_from_rust();
        fn log_trace_from_rust();
        fn log_error_with_placeholder_from_rust(i: i32);
        fn panic_with_placeholder_from_rust(i: i32);
    }
}

pub fn log_info_from_rust() {
    log::info!("test info log");
}

pub fn log_warning_from_rust() {
    log::warn!("test warning log");
}

pub fn log_error_from_rust() {
    log::error!("test error log");
}

pub fn log_debug_from_rust() {
    log::debug!("test debug log");
}

pub fn log_trace_from_rust() {
    log::trace!("test trace log");
}

fn log_error_with_placeholder_from_rust(i: i32) {
    log::error!("test log with placeholder {i}");
}

fn panic_with_placeholder_from_rust(i: i32) {
    panic!("panic with placeholder {i}");
}
