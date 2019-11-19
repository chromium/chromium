// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_PLATFORM_FEATURES_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_PLATFORM_FEATURES_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;
}

// Provides platform-specific metrics for desktop browsers.
class DesktopPlatformFeaturesMetricsProvider : public metrics::MetricsProvider {
 public:
  DesktopPlatformFeaturesMetricsProvider();
  ~DesktopPlatformFeaturesMetricsProvider() override;

  // metrics::MetricsProvider
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopPlatformFeaturesMetricsProvider);
};

#endif  //  CHROME_BROWSER_METRICS_DESKTOP_PLATFORM_FEATURES_METRICS_PROVIDER_H_
