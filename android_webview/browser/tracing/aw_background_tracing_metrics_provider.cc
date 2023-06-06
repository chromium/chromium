// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_background_tracing_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/tracing/background_tracing_field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_service.h"
#include "third_party/metrics_proto/trace_log.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tracing {

namespace {

void OnProvideEmbedderMetrics(base::OnceCallback<void(bool)> done_callback,
                              bool success) {
  // TODO(crbug/1052796): Remove the UMA timer code, which is currently used to
  // determine if it is worth to finalize independent logs in the background
  // by measuring the time it takes to execute the callback
  // MetricsService::PrepareProviderMetricsLogDone().
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::move(done_callback).Run(success);
  if (success) {
    // We don't use the SCOPED_UMA_HISTOGRAM_TIMER macro because we want to
    // measure the time it takes to finalize an independent log, and that only
    // happens when |success| is true.
    base::UmaHistogramTimes(
        "UMA.IndependentLog.AwBackgroundTracingMetricsProvider.FinalizeTime",
        base::TimeTicks::Now() - start_time);
  }
}

}  // namespace

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
    std::string&& serialized_trace,
    metrics::TraceLog* log,
    base::HistogramSnapshotManager* snapshot_manager,
    base::OnceCallback<void(bool)> done_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&AwBackgroundTracingMetricsProvider::Compress,
                     std::move(serialized_trace), uma_proto, log),
      base::BindOnce(&OnProvideEmbedderMetrics, std::move(done_callback)));
}

// static
bool AwBackgroundTracingMetricsProvider::Compress(
    std::string&& serialized_trace,
    metrics::ChromeUserMetricsExtension* uma_proto,
    metrics::TraceLog* log) {
  std::string deflated;
  deflated.resize(kCompressedUploadLimitBytes);
  size_t compressed_size;

  if (!compression::GzipCompress(serialized_trace,
                                 reinterpret_cast<char*>(deflated.data()),
                                 kCompressedUploadLimitBytes, &compressed_size,
                                 /* malloc_fn= */ nullptr,
                                 /* free_fn= */ nullptr)) {
    return false;
  }

  deflated.resize(compressed_size);

  SetTrace(log, std::move(deflated));
  log->set_compression_type(metrics::TraceLog::COMPRESSION_TYPE_ZLIB);

  // Remove the package name according to the privacy requirements.
  // See go/public-webview-trace-collection.
  auto* system_profile = uma_proto->mutable_system_profile();
  system_profile->clear_app_package_name();

  return true;
}

}  // namespace tracing
