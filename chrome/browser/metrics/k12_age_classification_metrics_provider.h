// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_K12_AGE_CLASSIFICATION_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_K12_AGE_CLASSIFICATION_METRICS_PROVIDER_H_

#include <optional>

#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager_observer.h"

class K12AgeClassificationMetricsProvider
    : public metrics::MetricsProvider,
      public session_manager::SessionManagerObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(K12AgeClassificationSegment)
  enum class K12AgeClassificationSegment {
    // K12 user age classification is unspecified.
    kAgeUnspecified = 0,
    // K12 user age is under 18.
    kAgeUnder18 = 1,
    // K12 user age is equal or over 18.
    kAgeEqualOrOver18 = 2,
    kMaxValue = kAgeEqualOrOver18,
  };

  static constexpr char kHistogramName[] =
      "ChromeOS.K12UserAgeClassification.LogSegment";

  // LINT.ThenChange(//tools/metrics/histograms/metadata/chromeos/enums.xml:K12AgeClassificationSegment,//components/policy/proto/device_management_backend.proto:K12AgeClassificationSegment)
  K12AgeClassificationMetricsProvider();
  K12AgeClassificationMetricsProvider(
      const K12AgeClassificationMetricsProvider&) = delete;
  K12AgeClassificationMetricsProvider& operator=(
      const K12AgeClassificationMetricsProvider&) = delete;
  ~K12AgeClassificationMetricsProvider() override;

  // MetricsProvider:
  bool ProvideHistograms() override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  std::optional<K12AgeClassificationSegment> segment_;
};

#endif  // CHROME_BROWSER_METRICS_K12_AGE_CLASSIFICATION_METRICS_PROVIDER_H_
