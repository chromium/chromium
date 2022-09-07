// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_H_

#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

namespace ash {
namespace diagnostics {
namespace metrics {
class DiagnosticsMetrics final
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  DiagnosticsMetrics();
  DiagnosticsMetrics(DiagnosticsMetrics&) = delete;
  DiagnosticsMetrics& operator=(DiagnosticsMetrics&) = delete;
  ~DiagnosticsMetrics() override = default;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  bool IsEnabled() const override;

  // feature_usage::FeatureUsageMetrics helpers:
  void RecordUsage(bool success);
  void StopSuccessfulUsage();

  // Test helpers:
  bool GetSuccessfulUsageStartedForTesting();

 private:
  feature_usage::FeatureUsageMetrics feature_metrics_;
  bool successful_usage_started_;
};
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_METRICS_H_
