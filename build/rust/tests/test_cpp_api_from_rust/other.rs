// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub struct Multiplier(i32);

impl Multiplier {
    pub fn new(i: i32) -> Self {
        Self(i)
    }

    pub fn mul(&self, other: i32) -> i32 {
        self.0 * other
    }
}
