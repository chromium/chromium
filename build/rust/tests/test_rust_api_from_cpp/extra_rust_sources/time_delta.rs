// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Hand-written Rust code that `extra_rust_sources` merges into the
//! Crubit-generated bindings for `time_delta.h`.
//!
//! The code below can implement a foreign trait (`TryFrom`) for a type from
//! the generated bindings, because it is compiled as part of the same crate.

use crate::test_base::TestTimeDelta;
use core::num::TryFromIntError;
use core::time::Duration;

impl TryFrom<TestTimeDelta> for Duration {
    /// Negative `TestTimeDelta`s can't be represented as a `Duration`.
    type Error = TryFromIntError;

    fn try_from(delta: TestTimeDelta) -> Result<Self, Self::Error> {
        let us = u64::try_from(delta.InMicroseconds())?;
        Ok(Duration::from_micros(us))
    }
}
