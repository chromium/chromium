// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/telemetry_extension_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/grit/ash_telemetry_extension_resources.h"
#include "ash/webui/telemetry_extension_ui/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForTelemetryExtension() {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUITelemetryExtensionURL);
  info->scope = GURL(ash::kChromeUITelemetryExtensionURL);
  info->title = u"Telemetry Extension";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_96.png", 96, IDR_TELEMETRY_EXTENSION_ICON_96}}, *info);
  info->theme_color = 0xFF4285F4;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

TelemetrySystemAppDelegate::TelemetrySystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::TELEMETRY,
                                    "Telemetry",
                                    GURL("chrome://telemetry-extension"),
                                    profile) {}

std::unique_ptr<WebApplicationInfo> TelemetrySystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForTelemetryExtension();
}

bool TelemetrySystemAppDelegate::IsAppEnabled() const {
  return base::FeatureList::IsEnabled(chromeos::features::kTelemetryExtension);
}
