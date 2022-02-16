// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_

#include "components/tracing/common/background_tracing_metrics_provider.h"

namespace tracing {

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
      metrics::ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager) override;
};

}  // namespace tracing

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_
