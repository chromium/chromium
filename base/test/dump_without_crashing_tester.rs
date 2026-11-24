// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Test helper for checking calls to `//base:dump_without_crashing`.

#[cxx::bridge(namespace = "base::test::rust")]
mod ffi {
    unsafe extern "C++" {
        include!("base/test/dump_without_crashing_tester_rust_shim.h");

        type DumpWithoutCrashingTesterRs;

        #[Self = "DumpWithoutCrashingTesterRs"]
        fn Create() -> UniquePtr<DumpWithoutCrashingTesterRs>;
        fn DumpCount(self: &DumpWithoutCrashingTesterRs) -> i32;
    }
}

/// Installs a fake dump function for the lifetime of this object, which counts
/// the number of dumps, and resets `DumpWithoutCrashing()` throttling.
///
/// Only one `DumpWithoutCrashingTester` should exist at a time.
pub struct DumpWithoutCrashingTester {
    inner: cxx::UniquePtr<ffi::DumpWithoutCrashingTesterRs>,
}

impl DumpWithoutCrashingTester {
    pub fn new() -> Self {
        Self { inner: ffi::DumpWithoutCrashingTesterRs::Create() }
    }

    /// Returns the number of dumps since this object was created.
    pub fn dump_count(&self) -> i32 {
        self.inner.DumpCount()
    }
}

impl Default for DumpWithoutCrashingTester {
    fn default() -> Self {
        Self::new()
    }
}
