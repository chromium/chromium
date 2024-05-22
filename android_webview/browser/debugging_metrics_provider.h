// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_DEBUGGING_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_DEBUGGING_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace android_webview {

// DebuggingMetricsProvider is responsible for logging whether a WebView
// instance is running in either a debug build or if a developer has called
// setRemoteDebuggingEnabled
class DebuggingMetricsProvider : public metrics::MetricsProvider {
 public:
  DebuggingMetricsProvider() = default;

  ~DebuggingMetricsProvider() override = default;

  // TODO(thomasbull): Investigate switching to ProvideHistograms;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

  DebuggingMetricsProvider(const DebuggingMetricsProvider&) = delete;

  DebuggingMetricsProvider& operator=(const DebuggingMetricsProvider&) = delete;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_DEBUGGING_METRICS_PROVIDER_H_
