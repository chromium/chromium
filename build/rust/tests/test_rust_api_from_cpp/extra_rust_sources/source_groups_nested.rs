// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is placed into the `test_outer::test_inner` module of the
// generated bindings (see `extra_rust_source_groups` in `BUILD.gn`).

use super::TestMeters;

pub fn double_meters(m: TestMeters) -> TestMeters {
    // `scale_factor` is provided by `source_groups_root.rs`,
    // which is placed at the root of the crate.
    TestMeters { value: m.value * crate::scale_factor() }
}
