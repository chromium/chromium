// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:command_line";
}

use command_line::{CurrentCommandLine, SubprocessCommandLine};
use rust_gtest_interop::prelude::*;

#[gtest(RustCommandLineTest, BasicOperations)]
fn test_basic_operations() {
    expect_true!(CurrentCommandLine::initialized_for_current_process());

    // Test read-only operations on the current process command line.
    let cl = CurrentCommandLine::get();
    let _program = cl.get_program();
    let _args = cl.get_args();
}

#[gtest(RustCommandLineTest, SubprocessCommandLineMutations)]
fn test_subprocess_command_line_mutations() {
    // Test mutations on an independent, owned SubprocessCommandLine.
    let mut cl = SubprocessCommandLine::new("test-program");

    cl.append_switch("test-rust-switch");
    expect_true!(cl.has_switch("test-rust-switch"));

    cl.append_switch_ascii("test-rust-switch-val", "hello-from-rust");
    expect_true!(cl.has_switch("test-rust-switch-val"));
    expect_eq!(cl.get_switch_value_ascii("test-rust-switch-val"), "hello-from-rust");

    cl.remove_switch("test-rust-switch");
    expect_false!(cl.has_switch("test-rust-switch"));

    cl.append_arg("test-rust-positional-arg");
    let args = cl.get_args();
    expect_true!(args.iter().any(|i| i == "test-rust-positional-arg"));
}

// Can't launch subprocesses on mobile.
#[cfg(not(any(target_os = "android", target_os = "ios")))]
#[gtest(RustCommandLineTest, SpawnsSubprocessWithNewCommandLine)]
fn test_spawns_subprocess_with_new_command_line() {
    let current_cmd = CurrentCommandLine::get();
    let program = current_cmd.get_program();

    let mut subprocess_cmd = SubprocessCommandLine::new(&program);
    // This is the flag we expect to see inherited.
    subprocess_cmd.append_switch("custom-inherited-rust-flag");
    // Tells the test runner to only run SubprocessTarget (and not an entire new
    // test suite).
    subprocess_cmd.append_switch_ascii("gtest_filter", "RustCommandLineTest.SubprocessTarget");
    // Stops gtest from spawning worker subprocesses.
    subprocess_cmd.append_switch("single-process-tests");

    let output = subprocess_cmd.get_app_output().expect("Failed to launch subprocess");

    expect_true!(output.contains("SUCCESS_FROM_SUBPROCESS"));
}

// Can't launch subprocesses on mobile.
#[cfg(not(any(target_os = "android", target_os = "ios")))]
#[gtest(RustCommandLineTest, SubprocessTarget)]
fn test_subprocess_target() {
    let cl = CurrentCommandLine::get();
    if cl.has_switch("custom-inherited-rust-flag") {
        println!("SUCCESS_FROM_SUBPROCESS");
    }
}
