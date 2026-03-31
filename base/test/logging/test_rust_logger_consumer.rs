// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace = "base::test")]
mod ffi {
    extern "Rust" {
        fn print_test_info_log();
        fn print_test_warning_log();
        fn print_test_error_log();
        fn print_test_debug_log();
        fn print_test_trace_log();
        fn print_test_error_log_with_placeholder(i: i32);
        fn panic_from_rust_with_placeholder(i: i32);
    }
}

fn print_test_info_log() {
    log::info!("test info log");
}

fn print_test_warning_log() {
    log::warn!("test warning log");
}

fn print_test_error_log() {
    log::error!("test error log");
}

fn print_test_debug_log() {
    log::debug!("test debug log");
}

fn print_test_trace_log() {
    log::trace!("test trace log");
}

fn print_test_error_log_with_placeholder(i: i32) {
    log::error!("test log with placeholder {i}");
}

fn panic_from_rust_with_placeholder(i: i32) {
    panic!("panic with placeholder {i}");
}
