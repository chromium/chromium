// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_background_tracing_metrics_provider.h"

#include "base/strings/string_piece.h"
#include "base/time/time.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "components/metrics/content/gpu_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/background_tracing_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

namespace tracing {

AwBackgroundTracingMetricsProvider::AwBackgroundTracingMetricsProvider() =
    default;
AwBackgroundTracingMetricsProvider::~AwBackgroundTracingMetricsProvider() =
    default;

void AwBackgroundTracingMetricsProvider::Init() {
  // TODO(crbug.com/1290887): SetupBackgroundTracingFieldTrial() should be
  // called here.
  metrics::MetricsService* metrics =
      android_webview::AwMetricsServiceClient::GetInstance()
          ->GetMetricsService();
  DCHECK(metrics);

  system_profile_providers_.emplace_back(
      std::make_unique<variations::FieldTrialsProvider>(
          metrics->synthetic_trial_registry(), base::StringPiece()));
  system_profile_providers_.emplace_back(
      std::make_unique<metrics::CPUMetricsProvider>());
  system_profile_providers_.emplace_back(
      std::make_unique<metrics::GPUMetricsProvider>());
}

bool AwBackgroundTracingMetricsProvider::HasIndependentMetrics() {
  return content::BackgroundTracingManager::GetInstance()->HasTraceToUpload();
}

void AwBackgroundTracingMetricsProvider::ProvideIndependentMetrics(
    base::OnceCallback<void(bool)> done_callback,
    metrics::ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  auto* tracing_manager = content::BackgroundTracingManager::GetInstance();
  // TODO(crbug.com/1290887): remove this when
  // content::BackgroundTracingManager::GetInstance() is updated to return a
  // reference.
  DCHECK(tracing_manager);

  auto serialized_trace = tracing_manager->GetLatestTraceToUpload();
  if (serialized_trace.empty()) {
    std::move(done_callback).Run(false);
    return;
  }
  metrics::TraceLog* log = uma_proto->add_trace_log();
  log->set_raw_data(std::move(serialized_trace));

  auto* system_profile = uma_proto->mutable_system_profile();
  DCHECK(system_profile);

  for (auto& provider : system_profile_providers_) {
    provider->ProvideSystemProfileMetricsWithLogCreationTime(
        base::TimeTicks::Now(), system_profile);
  }

  // Remove the package name according to the privacy requirements.
  // See go/public-webview-trace-collection.
  system_profile->clear_app_package_name();
  std::move(done_callback).Run(true);
}

}  // namespace tracing
