// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_METRICS_HISTOGRAM_TESTER_RUST_SHIM_H_
#define BASE_TEST_METRICS_HISTOGRAM_TESTER_RUST_SHIM_H_

#include <stdint.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace base::test::rust {

class HistogramTesterRs {
 public:
  HistogramTesterRs();
  ~HistogramTesterRs();

  void ExpectUniqueSample(::rust::Str name,
                          int32_t sample,
                          int32_t expected_bucket_count) const;
  void ExpectBucketCount(::rust::Str name,
                         int32_t sample,
                         int32_t expected_count) const;
  void ExpectTotalCount(::rust::Str name, int32_t expected_count) const;
  void ExpectTimeBucketCount(::rust::Str name,
                             int64_t sample_us,
                             int32_t expected_count) const;
  void ExpectUniqueTimeSample(::rust::Str name,
                              int64_t sample_us,
                              int32_t expected_bucket_count) const;

 private:
  base::HistogramTester histogram_tester_;
};

std::unique_ptr<HistogramTesterRs> CreateHistogramTesterRs();

}  // namespace base::test::rust

#endif  // BASE_TEST_METRICS_HISTOGRAM_TESTER_RUST_SHIM_H_
