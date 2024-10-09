// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/terminal_system_web_app_info.h"

#include <memory>

#include "base/strings/strcat.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider_registry.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/screen.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
constexpr gfx::Rect TERMINAL_DEFAULT_BOUNDS(gfx::Point(64, 64),
                                            gfx::Size(652, 484));
constexpr gfx::Size TERMINAL_SETTINGS_DEFAULT_SIZE(768, 512);
}  // namespace

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForTerminalSystemWebApp() {
  // URL used for crostini::kCrostiniTerminalSystemAppId.
  GURL start_url("chrome-untrusted://terminal/html/terminal.html");
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(chrome::kChromeUIUntrustedTerminalURL);
  info->title = l10n_util::GetStringUTF16(IDS_CROSTINI_TERMINAL_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"app_icon_256.png", 256, IDR_LOGO_CROSTINI_TERMINAL}}, *info);
  info->background_color = 0xFF202124;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->additional_search_terms = {
      "linux", "terminal", "crostini", "ssh",
      l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_SEARCH_TERMS)};
  return info;
}

gfx::Rect GetDefaultBoundsForTerminal(Browser* browser) {
  if (browser->is_type_app_popup()) {
    gfx::Rect bounds =
        display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
    bounds.ClampToCenteredSize(TERMINAL_SETTINGS_DEFAULT_SIZE);
    return bounds;
  }
  return TERMINAL_DEFAULT_BOUNDS;
}

TerminalSystemAppDelegate::TerminalSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::TERMINAL,
                                "Terminal",
                                GURL(chrome::kChromeUIUntrustedTerminalURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
TerminalSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForTerminalSystemWebApp();
}

Browser* TerminalSystemAppDelegate::GetWindowForLaunch(Profile* profile,
                                                       const GURL& url) const {
  return nullptr;
}

bool TerminalSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}

bool TerminalSystemAppDelegate::ShouldShowInLauncher() const {
  // Show if SSH is enabled, or crostini enabled, or any other terminal exists.
  std::string reason;
  return profile()->GetPrefs()->GetBoolean(
             crostini::prefs::kTerminalSshAllowedByPolicy) ||
         crostini::CrostiniFeatures::Get()->IsAllowedNow(profile(), &reason) ||
         !guest_os::GuestOsServiceFactory::GetForProfile(profile())
              ->TerminalProviderRegistry()
              ->List()
              .empty();
}

bool TerminalSystemAppDelegate::IsAppEnabled() const {
  // Do not install for child accounts.
  return !profile()->IsChild();
}
bool TerminalSystemAppDelegate::ShouldHaveTabStrip() const {
  return true;
}

gfx::Rect TerminalSystemAppDelegate::GetDefaultBounds(Browser* browser) const {
  return GetDefaultBoundsForTerminal(browser);
}

bool TerminalSystemAppDelegate::HasCustomTabMenuModel() const {
  return true;
}

std::unique_ptr<ui::SimpleMenuModel> TerminalSystemAppDelegate::GetTabMenuModel(
    ui::SimpleMenuModel::Delegate* delegate) const {
  auto result = std::make_unique<ui::SimpleMenuModel>(delegate);
  result->AddItemWithStringId(TabStripModel::CommandNewTabToRight,
                              IDS_TAB_CXMENU_NEWTABTORIGHT);
  result->AddSeparator(ui::NORMAL_SEPARATOR);
  result->AddItemWithStringId(TabStripModel::CommandCloseTab,
                              IDS_TAB_CXMENU_CLOSETAB);
  result->AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                              IDS_TAB_CXMENU_CLOSEOTHERTABS);
  result->AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                              IDS_TAB_CXMENU_CLOSETABSTORIGHT);
  return result;
}

bool TerminalSystemAppDelegate::ShouldShowTabContextMenuShortcut(
    Profile* profile,
    int command_id) const {
  if (command_id == TabStripModel::CommandCloseTab) {
    return guest_os::GetTerminalSettingPassCtrlW(profile);
  }
  return true;
}

bool TerminalSystemAppDelegate::ShouldPinTab(GURL url) const {
  return url == GURL(base::StrCat({chrome::kChromeUIUntrustedTerminalURL,
                                   guest_os::kTerminalHomePath}));
}

bool TerminalSystemAppDelegate::UseSystemThemeColor() const {
  return false;
}
