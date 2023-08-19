// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_METRICS_PROVIDER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class PersonalizationAppThemeMetricsProvider : public metrics::MetricsProvider {
 public:
  PersonalizationAppThemeMetricsProvider();

  PersonalizationAppThemeMetricsProvider(
      const PersonalizationAppThemeMetricsProvider&) = delete;
  PersonalizationAppThemeMetricsProvider& operator=(
      const PersonalizationAppThemeMetricsProvider&) = delete;

  ~PersonalizationAppThemeMetricsProvider() override;

  // metrics::MetricsProvider:
  bool ProvideHistograms() override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_THEME_METRICS_PROVIDER_H_
