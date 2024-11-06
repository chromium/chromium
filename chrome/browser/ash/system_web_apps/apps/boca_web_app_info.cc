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
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForBocaApp() {
  GURL start_url = GURL(ash::boca::kChromeBocaAppUntrustedIndexURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::boca::kChromeBocaAppUntrustedURL);
  info->title = l10n_util::GetStringUTF16(IDS_SCHOOL_TOOLS_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(), {{"icon_256.png", 256, IDR_ASH_BOCA_UI_ICON_256_PNG}},
      *info);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

bool IsConsumerProfile(Profile* profile) {
  return ash::boca_util::IsConsumer(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile));
}

bool IsEnabled(Profile* profile) {
  return ash::boca_util::IsEnabled(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile));
}

}  // namespace

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
  return true;
}

bool BocaSystemAppDelegate::HasCustomTabMenuModel() const {
  return IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::ShouldShowInSearchAndShelf() const {
  return IsEnabled(profile());
}

bool BocaSystemAppDelegate::ShouldShowInLauncher() const {
  return IsEnabled(profile());
}

gfx::Size BocaSystemAppDelegate::GetMinimumWindowSize() const {
  if (!IsConsumerProfile(profile())) {
    return {400, 400};
  }
  return SystemWebAppDelegate::GetMinimumWindowSize();
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

Browser* BocaSystemAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    web_app::WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  Browser* const browser =
      ash::SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
          profile, provider, url, params);
  if (IsConsumerProfile(profile)) {
    // Notify downstream Boca components so they can prepare the app instance
    // for OnTask and restore contents from the previous session if needed.
    ash::boca::BocaAppClient::Get()->GetSessionManager()->NotifyAppReload();
  }
  return browser;
}
