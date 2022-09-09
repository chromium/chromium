// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_PLATFORM_FEATURES_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_PLATFORM_FEATURES_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;
}

// Provides platform-specific metrics for desktop browsers.
class DesktopPlatformFeaturesMetricsProvider : public metrics::MetricsProvider {
 public:
  DesktopPlatformFeaturesMetricsProvider();

  DesktopPlatformFeaturesMetricsProvider(
      const DesktopPlatformFeaturesMetricsProvider&) = delete;
  DesktopPlatformFeaturesMetricsProvider& operator=(
      const DesktopPlatformFeaturesMetricsProvider&) = delete;

  ~DesktopPlatformFeaturesMetricsProvider() override;

  // metrics::MetricsProvider
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

#endif  //  CHROME_BROWSER_METRICS_DESKTOP_PLATFORM_FEATURES_METRICS_PROVIDER_H_
