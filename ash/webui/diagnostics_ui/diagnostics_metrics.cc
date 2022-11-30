// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

namespace ash {
namespace diagnostics {
namespace metrics {
namespace {
const char kDiagnosticsUmaFeatureName[] = "DiagnosticsUi";
}

DiagnosticsMetrics::DiagnosticsMetrics()
    : feature_metrics_(kDiagnosticsUmaFeatureName, this),
      successful_usage_started_(false) {}

bool DiagnosticsMetrics::IsEligible() const {
  return true;
}

bool DiagnosticsMetrics::IsEnabled() const {
  return true;
}

// Helper function for feature_usage::FeatureUsageMetrics RecordUsage.
void DiagnosticsMetrics::RecordUsage(bool success) {
  feature_metrics_.RecordUsage(success);

  // Start successful usage recording only when usage was successful and not
  // currently recording a successful usage.
  // See {@link feature_usage::FeatureUsageMetrics} for rationale behind
  // starting and stopping usetime tracking.
  if (success) {
    DCHECK(!successful_usage_started_);
    feature_metrics_.StartSuccessfulUsage();
    successful_usage_started_ = true;
  }
}

void DiagnosticsMetrics::StopSuccessfulUsage() {
  // Exit early if usage was not started.
  if (!successful_usage_started_) {
    return;
  }

  feature_metrics_.StopSuccessfulUsage();
  successful_usage_started_ = false;
}

// Test helpers to check state of `successful_usage_started`.
bool DiagnosticsMetrics::GetSuccessfulUsageStartedForTesting() {
  return successful_usage_started_;
}
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
