// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace = "base::test::rust")]
mod ffi {
    unsafe extern "C++" {
        include!("base/test/metrics/histogram_tester_rust_shim.h");

        type HistogramTesterRs;

        fn CreateHistogramTesterRs() -> UniquePtr<HistogramTesterRs>;
        fn ExpectUniqueSample(
            self: &HistogramTesterRs,
            name: &str,
            sample: i32,
            expected_bucket_count: i32,
        );
        fn ExpectBucketCount(
            self: &HistogramTesterRs,
            name: &str,
            sample: i32,
            expected_count: i32,
        );
        fn ExpectTotalCount(self: &HistogramTesterRs, name: &str, expected_count: i32);
        fn ExpectTimeBucketCount(
            self: &HistogramTesterRs,
            name: &str,
            sample_us: i64,
            expected_count: i32,
        );
        fn ExpectUniqueTimeSample(
            self: &HistogramTesterRs,
            name: &str,
            sample_us: i64,
            expected_bucket_count: i32,
        );
    }
}

pub struct HistogramTester {
    inner: cxx::UniquePtr<ffi::HistogramTesterRs>,
}

impl HistogramTester {
    pub fn new() -> Self {
        Self { inner: ffi::CreateHistogramTesterRs() }
    }

    pub fn expect_unique_sample(&self, name: &str, sample: i32, expected_bucket_count: i32) {
        self.inner.ExpectUniqueSample(name, sample, expected_bucket_count);
    }

    pub fn expect_bucket_count(&self, name: &str, sample: i32, expected_count: i32) {
        self.inner.ExpectBucketCount(name, sample, expected_count);
    }

    pub fn expect_total_count(&self, name: &str, expected_count: i32) {
        self.inner.ExpectTotalCount(name, expected_count);
    }

    pub fn expect_time_bucket_count(
        &self,
        name: &str,
        sample: core::time::Duration,
        expected_count: i32,
    ) {
        let sample_us = i64::try_from(sample.as_micros()).unwrap_or(i64::MAX);
        self.inner.ExpectTimeBucketCount(name, sample_us, expected_count);
    }

    pub fn expect_unique_time_sample(
        &self,
        name: &str,
        sample: core::time::Duration,
        expected_bucket_count: i32,
    ) {
        let sample_us = i64::try_from(sample.as_micros()).unwrap_or(i64::MAX);
        self.inner.ExpectUniqueTimeSample(name, sample_us, expected_bucket_count);
    }
}

impl Default for HistogramTester {
    fn default() -> Self {
        Self::new()
    }
}
