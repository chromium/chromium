// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Hand-written Rust code that `extra_rust_sources_mod` places into the
//! `test_outer::test_inner` module (next to the Crubit-generated bindings for
//! the `test_outer::test_inner` C++ namespace).

use super::TestMeters;

pub fn double_meters(m: TestMeters) -> TestMeters {
    TestMeters { value: m.value * 2 }
}
