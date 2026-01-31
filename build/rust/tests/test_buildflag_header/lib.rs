// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Test for `buildflag_header` template.
//!
//! `IS_FOO` and `IS_BAR` come from `BUILD.gn`.

use rust_gtest_interop::prelude::*;

#[cfg(IS_1_EQ_1_FOR_TESTING)]
fn test1() -> bool {
    true
}

#[cfg(not(IS_1_EQ_2_FOR_TESTING))]
fn test2() -> bool {
    true
}

#[cfg(IS_TRUE_FOR_TESTING)]
fn test3() -> bool {
    true
}

#[cfg(not(IS_FALSE_FOR_TESTING))]
fn test4() -> bool {
    true
}

#[cfg(not(STRING_VALUE_FOR_TESTING))]
fn test5() -> bool {
    true
}

#[gtest(TestBuildFlagHeaderInRust, SmokeTest)]
fn test_buildflag_header() {
    assert!(test1());
    assert!(test2());
    assert!(test3());
    assert!(test4());
    assert!(test5());
}
