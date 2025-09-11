// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/boca_ui/boca_app_page_handler.h"
#include "ash/webui/boca_ui/boca_ui.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/grit/ash_boca_ui_resources.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
namespace {

inline constexpr std::string_view kDisabled = "disabled";

bool IsConsumerProfile(Profile* profile) {
  return ash::boca_util::IsConsumer(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile));
}

// A clone of the method in boca_role_util.cc IsEnabled(). The difference is we
// read prefs from profile instead of from user. The previous function read from
// user to de-couple from browser profile. But user prefs are not guaranteed to
// be loaded before the check happens, so we check from profile prefs instead.
bool IsEnabled(Profile* profile) {
  // Uber switch for boca.
  if (!ash::features::IsBocaUberEnabled()) {
    return false;
  }

  if (ash::features::IsBocaEnabled()) {
    return true;
  }

  if (!profile) {
    return false;
  }

  if (!ash::InstallAttributes::IsInitialized() ||
      !enterprise_util::IsProfileAffiliated(profile)) {
    return false;
  }

  auto* prefs = profile->GetPrefs();
  if (!prefs) {
    return false;
  }

  auto setting =
      prefs->GetString(ash::prefs::kClassManagementToolsAvailabilitySetting);
  return !setting.empty() && setting != kDisabled;
}

}  // namespace

BocaSystemAppDelegate::BocaSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::BOCA,
                                "Boca",
                                GURL(ash::boca::kChromeBocaAppUntrustedURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
BocaSystemAppDelegate::GetWebAppInfo() const {
  GURL start_url = GURL(ash::boca::kChromeBocaAppUntrustedIndexURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::boca::kChromeBocaAppUntrustedURL);
  info->title = l10n_util::GetStringUTF16(IDS_CLASS_TOOLS_TITLE);
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

bool BocaSystemAppDelegate::ShouldHaveExtensionsContainerInToolbar() const {
  return IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::IsUrlInSystemAppScope(const GURL& url) const {
  // The SWA is configured to host both 1P and 3P content depending on the use
  // case. We relax URL scope checks for the following scenarios:
  // 1. Consumer using the SWA when Class Tools is enabled.
  // 2. Class Tools is disabled. This allows us to extend SWA usage beyond Class
  // Tools (for example, locked quizzes).
  return !IsEnabled(profile()) || IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::ShouldPinTab(GURL url) const {
  return ShouldHaveTabStrip() &&
         url == GURL(ash::boca::kChromeBocaAppUntrustedIndexURL);
}

bool BocaSystemAppDelegate::IsAppEnabled() const {
  return IsEnabled(profile());
}

bool BocaSystemAppDelegate::HasCustomTabMenuModel() const {
  return IsConsumerProfile(profile());
}

bool BocaSystemAppDelegate::ShouldShowInSearchAndShelf() const {
  return true;
}

bool BocaSystemAppDelegate::ShouldShowInLauncher() const {
  return true;
}

gfx::Size BocaSystemAppDelegate::GetMinimumWindowSize() const {
  if (!IsConsumerProfile(profile())) {
    return {400, 400};
  }
  return {500, 500};
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

ash::BrowserDelegate* BocaSystemAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    web_app::WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  ash::BrowserDelegate* const browser =
      ash::SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
          profile, provider, url, params);
  if (IsConsumerProfile(profile)) {
    // Notify downstream Boca components so they can prepare the app instance
    // for OnTask and restore contents from the previous session if needed.
    ash::boca::BocaAppClient::Get()->GetSessionManager()->NotifyAppReload();
  } else {
    // Always launch producer app into float mode.
    aura::Window* window = browser->GetNativeWindow();
    ash::boca::BocaAppHandler::SetFloatModeAndBoundsForWindow(
        /*is_float_mode=*/true, window, base::BindOnce([](bool result) {}));
  }
  return browser;
}
