// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_metrics_provider.h"

#include <memory>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/metrics/content/gpu_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/field_trials_provider.h"
#include "content/public/browser/background_tracing_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

#if defined(OS_WIN)
#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"
#endif  // defined(OS_WIN)

namespace tracing {

BackgroundTracingMetricsProvider::BackgroundTracingMetricsProvider() {
#if defined(OS_WIN)
  // AV metrics provider is initialized asynchronously. It might not be
  // initialized when reporting metrics, in which case it'll just not add any AV
  // metrics to the proto.
  system_profile_providers_.emplace_back(
      std::make_unique<AntiVirusMetricsProvider>());
  av_metrics_provider_ = system_profile_providers_.back().get();
#endif  // defined(OS_WIN)
  system_profile_providers_.emplace_back(
      std::make_unique<variations::FieldTrialsProvider>(nullptr,
                                                        base::StringPiece()));
  system_profile_providers_.emplace_back(
      std::make_unique<metrics::CPUMetricsProvider>());
  system_profile_providers_.emplace_back(
      std::make_unique<metrics::GPUMetricsProvider>());
}
BackgroundTracingMetricsProvider::~BackgroundTracingMetricsProvider() {}

void BackgroundTracingMetricsProvider::Init() {
  // TODO(ssid): SetupBackgroundTracingFieldTrial() should be called here.
}

#if defined(OS_WIN)
void BackgroundTracingMetricsProvider::AsyncInit(
    base::OnceClosure done_callback) {
  av_metrics_provider_->AsyncInit(std::move(done_callback));
}
#endif  // defined(OS_WIN)

bool BackgroundTracingMetricsProvider::HasIndependentMetrics() {
  return content::BackgroundTracingManager::GetInstance()->HasTraceToUpload();
}

void BackgroundTracingMetricsProvider::ProvideIndependentMetrics(
    base::OnceCallback<void(bool)> done_callback,
    metrics::ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  auto* tracing_manager = content::BackgroundTracingManager::GetInstance();
  auto serialized_trace = tracing_manager->GetLatestTraceToUpload();
  if (serialized_trace.empty()) {
    std::move(done_callback).Run(false);
    return;
  }
  metrics::TraceLog* log = uma_proto->add_trace_log();
  log->set_raw_data(std::move(serialized_trace));

  auto* system_profile = uma_proto->mutable_system_profile();
  for (auto& provider : system_profile_providers_) {
    provider->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks(),
                                                             system_profile);
  }

  std::move(done_callback).Run(true);
}

}  // namespace tracing
