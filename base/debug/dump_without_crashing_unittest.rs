// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:dump_without_crashing";
    "//base/test:dump_without_crashing_tester";
}

use dump_without_crashing::dump_without_crashing;
use dump_without_crashing_tester::DumpWithoutCrashingTester;
use rust_gtest_interop::prelude::*;

#[gtest(RustDumpWithoutCrashingTest, NoDumpFunction)]
fn test_no_dump_function() {
    // Without a dump function (as in most tests), nothing should happen.
    expect_false!(dump_without_crashing!());
}

#[gtest(RustDumpWithoutCrashingTest, Dumps)]
fn test_dumps() {
    let tester = DumpWithoutCrashingTester::new();
    expect_true!(dump_without_crashing!());
    expect_eq!(tester.dump_count(), 1);
}

#[gtest(RustDumpWithoutCrashingTest, ThrottledPerCallSite)]
fn test_throttled_per_call_site() {
    let tester = DumpWithoutCrashingTester::new();

    // Repeated dumps from the same call site are throttled...
    for _ in 0..3 {
        dump_without_crashing!();
    }
    expect_eq!(tester.dump_count(), 1);

    // ...but dumps from a different call site are not.
    expect_true!(dump_without_crashing!());
    expect_eq!(tester.dump_count(), 2);
}
