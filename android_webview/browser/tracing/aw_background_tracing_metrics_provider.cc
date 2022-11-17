// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_background_tracing_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/tracing/background_tracing_field_trial.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_service.h"
#include "third_party/metrics_proto/trace_log.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tracing {

namespace {

// Compresses |serialized_trace| and returns the result. If the compressed trace
// does not fit into the upload limit or if there are zlib errors, returns
// nullopt.
// TODO(b/247824653): consider truncating the trace so that we have at least
// some data.
absl::optional<std::string> Compress(std::string&& serialized_trace) {
  std::string deflated;
  deflated.resize(kCompressedUploadLimitBytes);
  size_t compressed_size;

  if (compression::GzipCompress(serialized_trace,
                                reinterpret_cast<char*>(deflated.data()),
                                kCompressedUploadLimitBytes, &compressed_size,
                                /* malloc_fn= */ nullptr,
                                /* free_fn= */ nullptr)) {
    deflated.resize(compressed_size);
    return deflated;
  } else {
    return absl::nullopt;
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
    metrics::ChromeUserMetricsExtension& uma_proto,
    std::string&& serialized_trace,
    metrics::TraceLog& log,
    base::HistogramSnapshotManager* snapshot_manager,
    base::OnceCallback<void(bool)> done_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&Compress, std::move(serialized_trace)),
      base::BindOnce(&AwBackgroundTracingMetricsProvider::OnTraceCompressed,
                     weak_factory_.GetWeakPtr(), std::ref(uma_proto),
                     std::ref(log), std::move(done_callback)));
}

void AwBackgroundTracingMetricsProvider::OnTraceCompressed(
    metrics::ChromeUserMetricsExtension& uma_proto,
    metrics::TraceLog& log,
    base::OnceCallback<void(bool)> done_callback,
    absl::optional<std::string> compressed_trace) {
  if (!compressed_trace) {
    std::move(done_callback).Run(false);
    return;
  }

  SetTrace(log, std::move(*compressed_trace));
  log.set_compression_type(metrics::TraceLog::COMPRESSION_TYPE_ZLIB);

  // Remove the package name according to the privacy requirements.
  // See go/public-webview-trace-collection.
  auto* system_profile = uma_proto.mutable_system_profile();
  system_profile->clear_app_package_name();
  std::move(done_callback).Run(true);
}

}  // namespace tracing
