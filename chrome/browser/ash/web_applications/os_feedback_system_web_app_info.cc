// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/os_feedback_system_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_os_feedback_resources.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"

namespace {
// All Feedback Tool window will be a fixed 600px*640px portal per
// specification.
constexpr int kFeedbackAppDefaultWidth = 600;
constexpr int kFeedbackAppDefaultHeight = 640;
}  // namespace

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForOSFeedbackSystemWebApp() {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUIOSFeedbackUrl);
  info->scope = GURL(ash::kChromeUIOSFeedbackUrl);
  info->title = l10n_util::GetStringUTF16(IDS_FEEDBACK_REPORT_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {// use the new icons per the request in http://b/186638497
       {"app_icon_48.png", 48, IDR_ASH_OS_FEEDBACK_APP_ICON_48_PNG},
       {"app_icon_192.png", 192, IDR_ASH_OS_FEEDBACK_APP_ICON_192_PNG},
       {"app_icon_256.png", 256, IDR_ASH_OS_FEEDBACK_APP_ICON_256_PNG}},
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

gfx::Rect GetDefaultBoundsForOSFeedbackApp(Browser*) {
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  bounds.ClampToCenteredSize(
      {kFeedbackAppDefaultWidth, kFeedbackAppDefaultHeight});
  return bounds;
}

OSFeedbackAppDelegate::OSFeedbackAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::OS_FEEDBACK,
                                    "OSFeedback",
                                    GURL(ash::kChromeUIOSFeedbackUrl),
                                    profile) {}

std::unique_ptr<WebAppInstallInfo> OSFeedbackAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForOSFeedbackSystemWebApp();
}

bool OSFeedbackAppDelegate::IsAppEnabled() const {
  return base::FeatureList::IsEnabled(ash::features::kOsFeedback);
}
bool OSFeedbackAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  return true;
}
bool OSFeedbackAppDelegate::ShouldCaptureNavigations() const {
  return true;
}
bool OSFeedbackAppDelegate::ShouldAllowMaximize() const {
  return false;
}
bool OSFeedbackAppDelegate::ShouldAllowResize() const {
  return false;
}
gfx::Rect OSFeedbackAppDelegate::GetDefaultBounds(Browser* browser) const {
  return GetDefaultBoundsForOSFeedbackApp(browser);
}
