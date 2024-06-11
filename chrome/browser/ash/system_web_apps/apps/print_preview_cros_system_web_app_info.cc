// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/print_preview_cros_system_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_print_preview_cros_app_resources.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "url/gurl.h"

namespace {

constexpr char kPrintPreviewCrosInternalName[] = "PrintPreviewCros";
constexpr char16_t kPrintPreviewCrosTitle[] = u"Print";

}  // namespace

PrintPreviewCrosDelegate::PrintPreviewCrosDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(
          ash::SystemWebAppType::PRINT_PREVIEW_CROS,
          /*internal_name=*/kPrintPreviewCrosInternalName,
          /*install_url=*/GURL(ash::kChromeUIPrintPreviewCrosURL),
          profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
PrintPreviewCrosDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForPrintPreviewCrosSystemWebApp();
}

bool PrintPreviewCrosDelegate::IsAppEnabled() const {
  return ash::features::IsPrinterPreviewCrosAppEnabled();
}

bool PrintPreviewCrosDelegate::ShouldShowInLauncher() const {
  return false;
}

bool PrintPreviewCrosDelegate::ShouldShowInSearchAndShelf() const {
  return false;
}

bool PrintPreviewCrosDelegate::ShouldCaptureNavigations() const {
  return true;
}

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForPrintPreviewCrosSystemWebApp() {
  GURL start_url = GURL(ash::kChromeUIPrintPreviewCrosURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = start_url;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  // TODO(b/323585997): Localize title.
  info->title = kPrintPreviewCrosTitle;

  // TODO(b/323421684): Replace with actual app icons when available.
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"app_icon_192.png", 192,
        IDR_ASH_PRINT_PREVIEW_CROS_APP_IMAGES_APP_ICON_192_PNG}},
      *info);

  return info;
}
