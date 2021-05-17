// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/os_feedback_system_web_app_info.h"

#include <memory>

#include "ash/components/os_feedback_ui/url_constants.h"
#include "ash/grit/ash_os_feedback_resources.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
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

std::unique_ptr<WebApplicationInfo>
CreateWebAppInfoForOSFeedbackSystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
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
  info->open_as_window = true;

  return info;
}

gfx::Rect GetDefaultBoundsForOSFeedbackApp(Browser*) {
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  bounds.ClampToCenteredSize(
      {kFeedbackAppDefaultWidth, kFeedbackAppDefaultHeight});
  return bounds;
}
