// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is placed into the `test_outer` module of the generated bindings
// (see `extra_rust_source_groups` in `BUILD.gn`).

use super::test_inner::TestMeters;

pub fn triple_meters(m: TestMeters) -> TestMeters {
    TestMeters { value: m.value * 3 }
}
