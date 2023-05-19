// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"

namespace android_webview {

class VisibilityMetricsLogger;

// Records the metrics collected by VisibilityMetricsLogger.
//
// This class is owned by the metrics::MetricsService.
//
// Lifetime: Singleton
class VisibilityMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit VisibilityMetricsProvider(VisibilityMetricsLogger* logger);
  ~VisibilityMetricsProvider() override;

  VisibilityMetricsProvider() = delete;
  VisibilityMetricsProvider(const VisibilityMetricsProvider&) = delete;

  // metrics::MetricsProvider
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  raw_ptr<VisibilityMetricsLogger> logger_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_PROVIDER_H_
