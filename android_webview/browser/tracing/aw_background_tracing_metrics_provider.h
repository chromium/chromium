// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_

#include <vector>

#include "components/metrics/metrics_provider.h"

namespace tracing {

// Provides trace log metrics collected using BackgroundTracingManager to UMA
// proto. Background tracing uploads metrics of larger size compared to UMA
// histograms and it is better to upload them as independent metrics rather
// than part of UMA histograms log. The background tracing manager will make
// sure the traces are small when uploading over data.
class AwBackgroundTracingMetricsProvider : public metrics::MetricsProvider {
 public:
  AwBackgroundTracingMetricsProvider();

  AwBackgroundTracingMetricsProvider(
      const AwBackgroundTracingMetricsProvider&) = delete;
  AwBackgroundTracingMetricsProvider& operator=(
      const AwBackgroundTracingMetricsProvider&) = delete;

  ~AwBackgroundTracingMetricsProvider() override;

  // metrics::MetricsProvider:
  void Init() override;
  bool HasIndependentMetrics() override;
  void ProvideIndependentMetrics(
      base::OnceCallback<void(bool)> done_callback,
      metrics::ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager) override;

 private:
  std::vector<std::unique_ptr<metrics::MetricsProvider>>
      system_profile_providers_;
};

}  // namespace tracing

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_
