// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_

#include "components/tracing/common/background_tracing_metrics_provider.h"

#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace tracing {

// Only upload traces under 512 KB to decrease the risk of filling up
// the app's IPC binder buffer (1 MB) and causing crashes.
constexpr static int kCompressedUploadLimitBytes = 512 * 1024;

class AwBackgroundTracingMetricsProvider
    : public BackgroundTracingMetricsProvider {
 public:
  AwBackgroundTracingMetricsProvider();

  AwBackgroundTracingMetricsProvider(
      const AwBackgroundTracingMetricsProvider&) = delete;
  AwBackgroundTracingMetricsProvider& operator=(
      const AwBackgroundTracingMetricsProvider&) = delete;

  ~AwBackgroundTracingMetricsProvider() override;

  // metrics::MetricsProvider:
  void Init() override;

 private:
  // BackgroundTracingMetricsProvider:
  void ProvideEmbedderMetrics(
      metrics::ChromeUserMetricsExtension& uma_proto,
      std::string&& serialized_trace,
      metrics::TraceLog& log,
      base::HistogramSnapshotManager* snapshot_manager,
      base::OnceCallback<void(bool)> done_callback) override;

  void OnTraceCompressed(metrics::ChromeUserMetricsExtension& uma_proto,
                         metrics::TraceLog& log,
                         base::OnceCallback<void(bool)> done_callback,
                         absl::optional<std::string> serialized_trace);

  base::WeakPtrFactory<AwBackgroundTracingMetricsProvider> weak_factory_{this};
};

}  // namespace tracing

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_
