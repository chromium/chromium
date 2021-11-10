// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/system_web_app_delegate_base.h"

#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

url::Origin GetOrigin(const char* url) {
  GURL gurl = GURL(url);
  DCHECK(gurl.is_valid());

  url::Origin origin = url::Origin::Create(gurl);
  DCHECK(!origin.opaque());

  return origin;
}

SystemWebAppDelegateBase::SystemWebAppDelegateBase(
    const SystemAppType type,
    const std::string& internal_name,
    const GURL& install_url,
    Profile* profile,
    const OriginTrialsMap& origin_trials_map)
    : SystemWebAppDelegate(type,
                           internal_name,
                           install_url,
                           profile,
                           origin_trials_map) {}

SystemWebAppDelegateBase::~SystemWebAppDelegateBase() = default;

std::vector<AppId> SystemWebAppDelegateBase::GetAppIdsToUninstallAndReplace()
    const {
  return {};
}

gfx::Size SystemWebAppDelegateBase::GetMinimumWindowSize() const {
  return gfx::Size();
}

bool SystemWebAppDelegateBase::ShouldReuseExistingWindow() const {
  return true;
}

bool SystemWebAppDelegateBase::ShouldShowNewWindowMenuOption() const {
  return false;
}

bool SystemWebAppDelegateBase::ShouldIncludeLaunchDirectory() const {
  return false;
}

std::vector<int> SystemWebAppDelegateBase::GetAdditionalSearchTerms() const {
  return {};
}

bool SystemWebAppDelegateBase::ShouldShowInLauncher() const {
  return true;
}

bool SystemWebAppDelegateBase::ShouldShowInSearch() const {
  return true;
}

bool SystemWebAppDelegateBase::ShouldCaptureNavigations() const {
  return false;
}

bool SystemWebAppDelegateBase::ShouldAllowResize() const {
  return true;
}

bool SystemWebAppDelegateBase::ShouldAllowMaximize() const {
  return true;
}

bool SystemWebAppDelegateBase::ShouldHaveTabStrip() const {
  return false;
}

bool SystemWebAppDelegateBase::ShouldHaveReloadButtonInMinimalUi() const {
  return true;
}

bool SystemWebAppDelegateBase::ShouldAllowScriptsToCloseWindows() const {
  return false;
}

absl::optional<SystemAppBackgroundTaskInfo>
SystemWebAppDelegateBase::GetTimerInfo() const {
  return absl::nullopt;
}

bool SystemWebAppDelegateBase::IsAppEnabled() const {
  return true;
}

gfx::Rect SystemWebAppDelegateBase::GetDefaultBounds(Browser* browser) const {
  return {};
}

bool SystemWebAppDelegateBase::HasCustomTabMenuModel() const {
  return false;
}

std::unique_ptr<ui::SimpleMenuModel> SystemWebAppDelegateBase::GetTabMenuModel(
    ui::SimpleMenuModel::Delegate* delegate) const {
  return nullptr;
}

bool SystemWebAppDelegateBase::ShouldShowTabContextMenuShortcut(
    Profile* profile,
    int command_id) const {
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool SystemWebAppDelegateBase::HasTitlebarTerminalSelectNewTabButton() const {
  return false;
}
#endif

Browser* SystemWebAppDelegateBase::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  Browser::Type browser_type =
      (params.disposition == WindowOpenDisposition::NEW_POPUP)
          ? Browser::TYPE_APP_POPUP
          : Browser::TYPE_APP;

  // Always find an existing window, so that we can offset the screen
  // coordinates from a previously opened one.
  Browser* browser = FindSystemWebAppBrowser(profile, GetType(), browser_type);

  // System Web App windows can't be properly restored without storing the app
  // type. Until that is implemented, skip them for session restore.
  // TODO(crbug.com/1003170): Enable session restore for System Web Apps by
  // passing through the underlying value of params.omit_from_session_restore.
  constexpr bool kOmitFromSessionRestore = true;

  // Always reuse an existing browser for popups, otherwise check app type
  // whether we should use a single window.
  // TODO(crbug.com/1060423): Allow apps to control whether popups are single.
  const bool reuse_existing_window =
      browser_type == Browser::TYPE_APP_POPUP || ShouldReuseExistingWindow();

  if (!browser) {
    browser = CreateWebApplicationWindow(
        profile, params.app_id, params.disposition, params.restore_id,
        kOmitFromSessionRestore, ShouldAllowResize(), ShouldAllowMaximize());
  } else if (!reuse_existing_window) {
    gfx::Rect initial_bounds = browser->window()->GetRestoredBounds();
    initial_bounds.Offset(20, 20);
    browser = CreateWebApplicationWindow(
        profile, params.app_id, params.disposition, params.restore_id,
        kOmitFromSessionRestore, ShouldAllowResize(), ShouldAllowMaximize(),
        initial_bounds);
  }

  // Navigate application window to application's |url| if necessary.
  // Help app always navigates because its url might not match the url inside
  // the iframe, and the iframe's url is the one that matters.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  if (!web_contents || web_contents->GetURL() != url ||
      GetType() == SystemAppType::HELP) {
    web_contents = NavigateWebApplicationWindow(
        browser, params.app_id, url, WindowOpenDisposition::CURRENT_TAB);
  }

  SetLaunchFiles(ShouldIncludeLaunchDirectory(), params, web_contents,
                 provider);

  return browser;
}

}  // namespace web_app
