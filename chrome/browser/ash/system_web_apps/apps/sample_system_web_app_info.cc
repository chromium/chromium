// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/sample_system_web_app_info.h"

#include <memory>

#include "ash/webui/grit/ash_sample_system_web_app_resources.h"
#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForSampleSystemWebApp() {
  GURL start_url = GURL(ash::kChromeUISampleSystemWebAppURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUISampleSystemWebAppURL);
  // |title| should come from a resource string, but this is the sample app, and
  // doesn't have one.
  info->title = u"Sample System Web App";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"app_icon_192.png", 192,
        IDR_ASH_SAMPLE_SYSTEM_WEB_APP_APP_ICON_192_PNG}},
      *info);
  info->theme_color = 0xFF4285F4;
  info->background_color = 0xFFFFFFFF;
  // Bright green in dark mode to be able to see it flicker.
  // This should match up with the dark theme background color to prevent
  // flickering. But for the sample app, we use different, garish colors
  // to make sure we can see it working.
  info->dark_mode_theme_color = 0xFF11ff00;
  info->dark_mode_background_color = 0xFFff8888;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  info->share_target = apps::ShareTarget();
  info->share_target->action =
      GURL("chrome://sample-system-web-app/share.html");
  {
    apps::ShareTarget::Files icon_files;
    icon_files.name = "icons";
    icon_files.accept.push_back("image/x-xbitmap");
    info->share_target->params.files.push_back(std::move(icon_files));
  }

  web_app::CreateShortcutsMenuItemForSystemWebApp(
      u"Inter Frame Communication Demo",
      GURL("chrome://sample-system-web-app/inter_frame_communication.html"),
      {{"test.png", 192, IDR_ASH_SAMPLE_SYSTEM_WEB_APP_APP_ICON_192_PNG}},
      *info);

  web_app::CreateShortcutsMenuItemForSystemWebApp(
      u"Component Playground",
      GURL("chrome://sample-system-web-app/component_playground.html"),
      {{"test.png", 192, IDR_ASH_SAMPLE_SYSTEM_WEB_APP_APP_ICON_192_PNG}},
      *info);

  return info;
}

SampleSystemAppDelegate::SampleSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(
          ash::SystemWebAppType::SAMPLE,
          "Sample",
          GURL("chrome://sample-system-web-app/pwa.html"),
          profile,
          ash::OriginTrialsMap(
              {{ash::GetOrigin("chrome://sample-system-web-app"),
                {"Frobulate"}},
               {ash::GetOrigin("chrome-untrusted://sample-system-web-app"),
                {"Frobulate"}}})) {}

std::unique_ptr<web_app::WebAppInstallInfo>
SampleSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForSampleSystemWebApp();
}

bool SampleSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool SampleSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}

Browser* SampleSystemAppDelegate::GetWindowForLaunch(Profile* profile,
                                                     const GURL& url) const {
  return nullptr;
}

std::optional<ash::SystemWebAppBackgroundTaskInfo>
SampleSystemAppDelegate::GetTimerInfo() const {
  return ash::SystemWebAppBackgroundTaskInfo(
      base::Seconds(30), GURL("chrome://sample-system-web-app/timer.html"));
}
