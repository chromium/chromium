// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester_rust_shim.h"

#include <stdint.h>

#include <memory>
#include <string_view>

namespace base::test::rust {

HistogramTesterRs::HistogramTesterRs() = default;
HistogramTesterRs::~HistogramTesterRs() = default;

void HistogramTesterRs::ExpectUniqueSample(
    ::rust::Str name,
    int32_t sample,
    int32_t expected_bucket_count) const {
  histogram_tester_.ExpectUniqueSample(std::string_view(name), sample,
                                       expected_bucket_count);
}

void HistogramTesterRs::ExpectBucketCount(::rust::Str name,
                                          int32_t sample,
                                          int32_t expected_count) const {
  histogram_tester_.ExpectBucketCount(std::string_view(name), sample,
                                      expected_count);
}

void HistogramTesterRs::ExpectTotalCount(::rust::Str name,
                                         int32_t expected_count) const {
  histogram_tester_.ExpectTotalCount(std::string_view(name), expected_count);
}

void HistogramTesterRs::ExpectTimeBucketCount(::rust::Str name,
                                              int64_t sample_us,
                                              int32_t expected_count) const {
  histogram_tester_.ExpectTimeBucketCount(
      std::string_view(name), base::Microseconds(sample_us), expected_count);
}

void HistogramTesterRs::ExpectUniqueTimeSample(
    ::rust::Str name,
    int64_t sample_us,
    int32_t expected_bucket_count) const {
  histogram_tester_.ExpectUniqueTimeSample(std::string_view(name),
                                           base::Microseconds(sample_us),
                                           expected_bucket_count);
}

std::unique_ptr<HistogramTesterRs> CreateHistogramTesterRs() {
  return std::make_unique<HistogramTesterRs>();
}

}  // namespace base::test::rust
