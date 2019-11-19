// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_ANDROID_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CHROME_ANDROID_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;
}

// ChromeAndroidMetricsProvider provides Chrome-for-Android-specific stability
// metrics. Android-specific metrics which apply to lower layers should be
// implemented in metrics::AndroidMetricsProvider.
class ChromeAndroidMetricsProvider : public metrics::MetricsProvider {
 public:
  ChromeAndroidMetricsProvider();
  ~ChromeAndroidMetricsProvider() override;

  // metrics::MetricsProvider:
  void OnDidCreateMetricsLog() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeAndroidMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_ANDROID_METRICS_PROVIDER_H_
