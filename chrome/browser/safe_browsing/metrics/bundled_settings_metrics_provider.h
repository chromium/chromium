// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_METRICS_BUNDLED_SETTINGS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_METRICS_BUNDLED_SETTINGS_METRICS_PROVIDER_H_

#include "chrome/browser/metrics/cached_metrics_profile.h"
#include "components/metrics/metrics_provider.h"

namespace safe_browsing {

// A registerable metrics provider that will emit the bundled settings state of
// the main profile upon UMA upload.
class BundledSettingsMetricsProvider : public metrics::MetricsProvider {
 public:
  BundledSettingsMetricsProvider();
  ~BundledSettingsMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  metrics::CachedMetricsProfile cached_profile_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_METRICS_BUNDLED_SETTINGS_METRICS_PROVIDER_H_
