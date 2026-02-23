// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_HISTOGRAM_TESTER_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_HISTOGRAM_TESTER_H_

#include <string_view>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace glic {

// A wrapper around base::HistogramTester that automatically calls
// content::FetchHistogramsFromChildProcesses() and
// metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting()
// before every assertion. This is required for testing WebUI metrics which
// are recorded in the renderer process via chrome.histograms.
class GlicHistogramTester {
 public:
  GlicHistogramTester() = default;
  ~GlicHistogramTester() = default;

  template <typename T>
  void ExpectUniqueSample(
      std::string_view name,
      T sample,
      base::HistogramBase::Count32 expected_bucket_count) const {
    CollectHistograms();
    tester_.ExpectUniqueSample(name, sample, expected_bucket_count);
  }

  template <typename T>
  void ExpectBucketCount(
      std::string_view name,
      T sample,
      base::HistogramBase::Count32 expected_bucket_count) const {
    CollectHistograms();
    tester_.ExpectBucketCount(name, sample, expected_bucket_count);
  }

  void ExpectTotalCount(std::string_view name,
                        base::HistogramBase::Count32 count) const {
    CollectHistograms();
    tester_.ExpectTotalCount(name, count);
  }

  template <typename T>
  base::HistogramBase::Count32 GetBucketCount(std::string_view name,
                                              T sample) const {
    CollectHistograms();
    return tester_.GetBucketCount(name, sample);
  }

  std::vector<base::Bucket> GetAllSamples(std::string_view name) const {
    CollectHistograms();
    return tester_.GetAllSamples(name);
  }

  absl::flat_hash_map<std::string, std::vector<base::Bucket>>
  GetAllSamplesForPrefix(std::string_view prefix) const {
    CollectHistograms();
    return tester_.GetAllSamplesForPrefix(prefix);
  }

 private:
  void CollectHistograms() const {
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  }

  base::HistogramTester tester_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_HISTOGRAM_TESTER_H_
