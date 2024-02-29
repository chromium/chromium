// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/tracing/common/background_tracing_metrics_provider.h"

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
  void DoInit() override;

  void RecordCoreSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  // BackgroundTracingMetricsProvider:
  base::OnceCallback<bool(metrics::ChromeUserMetricsExtension*, std::string&&)>
  GetEmbedderMetricsProvider() override;

  base::WeakPtrFactory<AwBackgroundTracingMetricsProvider> weak_factory_{this};
};

}  // namespace tracing

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_BACKGROUND_TRACING_METRICS_PROVIDER_H_
