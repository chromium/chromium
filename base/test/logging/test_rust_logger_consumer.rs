// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use log::{debug, error, info, trace, warn};

#[cxx::bridge(namespace = "base::test")]
mod ffi {
    extern "Rust" {
        fn print_test_info_log();
        fn print_test_warning_log();
        fn print_test_error_log();
        fn print_test_debug_log();
        fn print_test_trace_log();
        fn print_test_error_log_with_placeholder(i: i32);
    }
}

pub fn print_test_info_log() {
    info!("test info log");
}

pub fn print_test_warning_log() {
    warn!("test warning log");
}

pub fn print_test_error_log() {
    error!("test error log");
}

pub fn print_test_debug_log() {
    debug!("test debug log");
}

pub fn print_test_trace_log() {
    trace!("test trace log");
}

fn print_test_error_log_with_placeholder(i: i32) {
    error!("test log with placeholder {}", i);
}
