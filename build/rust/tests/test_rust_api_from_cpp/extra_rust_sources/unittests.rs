// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//build/rust/tests/test_rust_api_from_cpp/extra_rust_sources:source_groups_rs_api"
        as source_groups;
    "//build/rust/tests/test_rust_api_from_cpp/extra_rust_sources:time_delta_rs_api"
        as time_delta;
}

use rust_gtest_interop::prelude::*;

#[gtest(RustApiFromCppExtraRustSources, ExtraRustSources)]
fn test_extra_rust_sources() {
    use ::time_delta::test_base::TestTimeDelta;
    use std::time::Duration;

    // `TryFrom` impl comes from `extra_rust_sources`.
    let delta = TestTimeDelta::FromMicroseconds(1_234_567);
    assert_eq!(Duration::try_from(delta), Ok(Duration::from_micros(1_234_567)));

    let negative_delta = TestTimeDelta::FromMicroseconds(-1);
    assert!(Duration::try_from(negative_delta).is_err());
}

#[gtest(RustApiFromCppExtraRustSources, ExtraRustSourceGroups)]
fn test_extra_rust_source_groups() {
    use ::source_groups::test_outer;
    use ::source_groups::test_outer::test_inner;

    // `double_meters` comes from an `extra_rust_source_groups` entry placed
    // into the `test_outer::test_inner` module.  It calls `scale_factor`,
    // which comes from `extra_rust_sources` (placed at the root of the crate).
    let m = test_inner::double_meters(test_inner::TestMeters { value: 21 });
    assert_eq!(m.value, 42);
    assert_eq!(::source_groups::scale_factor(), 2);

    // `triple_meters` comes from another `extra_rust_source_groups` entry
    // (placed into the `test_outer` module).
    let m = test_outer::triple_meters(test_inner::TestMeters { value: 14 });
    assert_eq!(m.value, 42);
}
