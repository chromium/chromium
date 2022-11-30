// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_background_tracing_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/tracing/background_tracing_field_trial.h"
#include "base/strings/string_piece.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_service.h"

namespace tracing {

AwBackgroundTracingMetricsProvider::AwBackgroundTracingMetricsProvider() =
    default;
AwBackgroundTracingMetricsProvider::~AwBackgroundTracingMetricsProvider() =
    default;

void AwBackgroundTracingMetricsProvider::Init() {
  android_webview::MaybeSetupWebViewOnlyTracing();

  metrics::MetricsService* metrics =
      android_webview::AwMetricsServiceClient::GetInstance()
          ->GetMetricsService();
  DCHECK(metrics);

  system_profile_providers_.emplace_back(
      std::make_unique<variations::FieldTrialsProvider>(
          metrics->GetSyntheticTrialRegistry(), base::StringPiece()));
}

void AwBackgroundTracingMetricsProvider::ProvideEmbedderMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  // Remove the package name according to the privacy requirements.
  // See go/public-webview-trace-collection.
  auto* system_profile = uma_proto->mutable_system_profile();
  system_profile->clear_app_package_name();
}

}  // namespace tracing
