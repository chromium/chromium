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

  // A callback to confirm if GetTemplateIdFromFile for Sea Pen wallpaper
  // completed successfully.
  using GetTemplateIdCallback = base::RepeatingCallback<void(bool success)>;

  // Set a private callback to wait for the completion of Sea Pen template ID
  // extraction and its metrics is triggered.
  void SetGetTemplateIdCallbackForTesting(GetTemplateIdCallback callback);

 private:
  GetTemplateIdCallback g_get_template_id_callback_for_testing_;

  base::WeakPtrFactory<WallpaperMetricsProvider> weak_ptr_factory_{this};

  void OnTemplateIdFromFileExtracted(std::optional<int> template_id);
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_WALLPAPER_METRICS_PROVIDER_H_
