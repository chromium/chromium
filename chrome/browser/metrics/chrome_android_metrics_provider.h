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

class PrefService;
class PrefRegistrySimple;

// ChromeAndroidMetricsProvider provides Chrome-for-Android-specific stability
// metrics. Android-specific metrics which apply to lower layers should be
// implemented in metrics::AndroidMetricsProvider.
class ChromeAndroidMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit ChromeAndroidMetricsProvider(PrefService* local_state);
  ~ChromeAndroidMetricsProvider() override;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::MetricsProvider:
  void OnDidCreateMetricsLog() override;
  void ProvidePreviousSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAndroidMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_ANDROID_METRICS_PROVIDER_H_
