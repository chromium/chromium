// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:location";
}

use location::{from_here, Location};
use rust_gtest_interop::prelude::*;

const EXPECTED_FILE_NAME: &str =
    if cfg!(windows) { "base\\location_unittest.rs" } else { "base/location_unittest.rs" };

// TODO(crbug.com/575911997): disabled due to breaking on linux-win-cross-rel
#[gtest(RustLocationTest, DISABLED_FromHere)]
fn test_from_here() {
    let line = line!() + 1;
    let here = from_here!();
    expect_eq!(here.file_name().to_str(), Ok(EXPECTED_FILE_NAME));
    expect_eq!(here.line_number(), line);
    expect_ne!(here.program_counter(), 0);
    expect_eq!(here.to_string(), format!("{EXPECTED_FILE_NAME}:{line}"));
}

#[gtest(RustLocationTest, FromHereIdentifiesCallSite)]
fn test_from_here_identifies_call_site() {
    // The same call site always gives the same location...
    let locations: Vec<Location> = (0..3).map(|_| from_here!()).collect();
    expect_eq!(locations[0], locations[1]);
    expect_eq!(locations[0], locations[2]);

    // ...and different call sites give different locations, even on the same
    // line.
    let (a, b) = (from_here!(), from_here!());
    expect_ne!(a, b);
    expect_ne!(a, locations[0]);
}

#[track_caller]
fn caller_location() -> Location {
    Location::current()
}

// TODO(crbug.com/575911997): disabled due to breaking on linux-win-cross-rel
#[gtest(RustLocationTest, DISABLED_Current)]
fn test_current() {
    let line = line!() + 1;
    let caller = caller_location();
    expect_eq!(caller.file_name().to_str(), Ok(EXPECTED_FILE_NAME));
    expect_eq!(caller.line_number(), line);
    expect_ne!(caller.program_counter(), 0);
}

#[gtest(RustLocationTest, CurrentIdentifiesCallSite)]
fn test_current_identifies_call_site() {
    let locations: Vec<Location> = (0..3).map(|_| caller_location()).collect();
    expect_eq!(locations[0], locations[1]);
    expect_eq!(locations[0], locations[2]);

    let (a, b) = (caller_location(), caller_location());
    expect_ne!(a, b);
    expect_ne!(a, locations[0]);
}

#[gtest(RustLocationTest, IsSendAndSync)]
fn test_is_send_and_sync() {
    // A `Location` holds a raw pointer (for FFI), but should still be usable
    // from any thread (e.g. when posting tasks).
    fn assert_send_and_sync<T: Send + Sync>() {}
    assert_send_and_sync::<Location>();
}
