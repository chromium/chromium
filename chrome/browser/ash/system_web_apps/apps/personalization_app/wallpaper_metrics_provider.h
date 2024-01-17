// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_WALLPAPER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_WALLPAPER_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class WallpaperMetricsProvider : public metrics::MetricsProvider {
 public:
  WallpaperMetricsProvider();

  WallpaperMetricsProvider(const WallpaperMetricsProvider&) = delete;
  WallpaperMetricsProvider& operator=(const WallpaperMetricsProvider&) = delete;

  ~WallpaperMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_WALLPAPER_METRICS_PROVIDER_H_
