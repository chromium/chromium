// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace android_webview {

// AndroidMetricsProvider is responsible for logging information related to
// system-level information about the Android device as well as the process.
class AndroidMetricsProvider : public metrics::MetricsProvider {
 public:
  AndroidMetricsProvider() = default;

  ~AndroidMetricsProvider() override = default;

  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  void ProvidePreviousSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

  AndroidMetricsProvider(const AndroidMetricsProvider&) = delete;

  AndroidMetricsProvider& operator=(const AndroidMetricsProvider&) = delete;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_
