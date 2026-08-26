// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CPU_PERFORMANCE_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CPU_PERFORMANCE_METRICS_PROVIDER_H_

#include "chrome/browser/metrics/cached_metrics_profile.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/browser/cpu_performance.h"

namespace metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CpuPerformanceTierOverridePair)
enum class CpuPerformanceTierOverridePair {
  kMinValue = 0,
  kUnknownToUnknown = kMinValue,
  kUnknownToLow = 1,
  kUnknownToMid = 2,
  kUnknownToHigh = 3,
  kUnknownToUltra = 4,
  kLowToUnknown = 5,
  kLowToLow = 6,
  kLowToMid = 7,
  kLowToHigh = 8,
  kLowToUltra = 9,
  kMidToUnknown = 10,
  kMidToLow = 11,
  kMidToMid = 12,
  kMidToHigh = 13,
  kMidToUltra = 14,
  kHighToUnknown = 15,
  kHighToLow = 16,
  kHighToMid = 17,
  kHighToHigh = 18,
  kHighToUltra = 19,
  kUltraToUnknown = 20,
  kUltraToLow = 21,
  kUltraToMid = 22,
  kUltraToHigh = 23,
  kUltraToUltra = 24,
  kMaxValue = kUltraToUltra
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/performance_controls/enums.xml:CpuPerformanceTierOverridePair)

// CpuPerformanceMetricsProvider provides metrics about CPU performance tier
// overrides (both user-selected and enterprise policy overrides) at UMA upload.
class CpuPerformanceMetricsProvider : public MetricsProvider {
 public:
  CpuPerformanceMetricsProvider() = default;
  CpuPerformanceMetricsProvider(const CpuPerformanceMetricsProvider&) = delete;
  CpuPerformanceMetricsProvider& operator=(
      const CpuPerformanceMetricsProvider&) = delete;
  ~CpuPerformanceMetricsProvider() override = default;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;

  static CpuPerformanceTierOverridePair EncodePair(
      content::cpu_performance::Tier nominal_tier,
      content::cpu_performance::Tier override_tier);

 private:
  CachedMetricsProfile cached_profile_;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CPU_PERFORMANCE_METRICS_PROVIDER_H_
