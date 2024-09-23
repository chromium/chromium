// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/boca_ui.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/grit/ash_boca_ui_resources.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "url/gurl.h"

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForBocaApp() {
  GURL start_url = GURL(ash::boca::kChromeBocaAppUntrustedIndexURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::boca::kChromeBocaAppUntrustedURL);
  // TODO(aprilzhou): Convert the title to a localized string
  info->title = u"BOCA";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"app_icon_120.png", 120, IDR_ASH_BOCA_UI_APP_ICON_120_PNG}}, *info);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

// Returns whether the user is able to consume Boca sessions. Primarily used by
// the delegate to tailor SWA UX.
// TODO(b/352675698): Identify Boca consumer profile without feature flags.
bool IsConsumerProfile(Profile* profile) {
  return ash::boca_util::IsConsumer();
}

BocaSystemAppDelegate::BocaSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::BOCA,
                                "Boca",
                                GURL(ash::boca::kChromeBocaAppUntrustedURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
BocaSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForBocaApp();
}

bool BocaSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool BocaSystemAppDelegate::ShouldAllowResize() const {
  return !IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::ShouldAllowMaximize() const {
  return !IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::ShouldHaveTabStrip() const {
  return IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::ShouldHideNewTabButton() const {
  return IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::IsUrlInSystemAppScope(const GURL& url) const {
  // Consumer SWA will also host 3P content, so we override app scope checks to
  // prevent navigation outside the app.
  return IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::ShouldPinTab(GURL url) const {
  return ShouldHaveTabStrip() &&
         url == GURL(ash::boca::kChromeBocaAppUntrustedIndexURL);
}

bool BocaSystemAppDelegate::IsAppEnabled() const {
  return ash::boca_util::IsEnabled();
}

bool BocaSystemAppDelegate::HasCustomTabMenuModel() const {
  return IsConsumerProfile(profile());
}

std::unique_ptr<ui::SimpleMenuModel> BocaSystemAppDelegate::GetTabMenuModel(
    ui::SimpleMenuModel::Delegate* delegate) const {
  std::unique_ptr<ui::SimpleMenuModel> tab_menu =
      std::make_unique<ui::SimpleMenuModel>(delegate);
  tab_menu->AddItemWithStringId(TabStripModel::CommandReload,
                                IDS_TAB_CXMENU_RELOAD);
  tab_menu->AddItemWithStringId(TabStripModel::CommandGoBack,
                                IDS_CONTENT_CONTEXT_BACK);
  return tab_menu;
}
