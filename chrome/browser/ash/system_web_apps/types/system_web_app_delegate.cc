// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

#include "ui/gfx/geometry/rect.h"

namespace ash {

url::Origin GetOrigin(const char* url) {
  GURL gurl = GURL(url);
  DCHECK(gurl.is_valid());

  url::Origin origin = url::Origin::Create(gurl);
  DCHECK(!origin.opaque());

  return origin;
}

SystemWebAppDelegate::SystemWebAppDelegate(
    const SystemWebAppType type,
    const std::string& internal_name,
    const GURL& install_url,
    Profile* profile,
    const OriginTrialsMap& origin_trials_map)
    : type_(type),
      internal_name_(internal_name),
      install_url_(install_url),
      profile_(profile),
      origin_trials_map_(origin_trials_map) {}

SystemWebAppDelegate::~SystemWebAppDelegate() = default;

std::vector<std::string> SystemWebAppDelegate::GetAppIdsToUninstallAndReplace()
    const {
  return {};
}

gfx::Size SystemWebAppDelegate::GetMinimumWindowSize() const {
  return gfx::Size();
}

bool SystemWebAppDelegate::ShouldShowNewWindowMenuOption() const {
  return false;
}

base::FilePath SystemWebAppDelegate::GetLaunchDirectory(
    const apps::AppLaunchParams& params) const {
  return base::FilePath();
}

std::vector<int> SystemWebAppDelegate::GetAdditionalSearchTerms() const {
  return {};
}

bool SystemWebAppDelegate::ShouldShowInLauncher() const {
  return true;
}

bool SystemWebAppDelegate::ShouldShowInSearchAndShelf() const {
  return true;
}

bool SystemWebAppDelegate::ShouldCaptureNavigations() const {
  return false;
}

bool SystemWebAppDelegate::ShouldAllowResize() const {
  return true;
}

bool SystemWebAppDelegate::ShouldAllowMaximize() const {
  return true;
}

bool SystemWebAppDelegate::ShouldAllowFullscreen() const {
  return true;
}

bool SystemWebAppDelegate::ShouldHaveTabStrip() const {
  return false;
}

bool SystemWebAppDelegate::ShouldHideNewTabButton() const {
  return false;
}

bool SystemWebAppDelegate::ShouldHaveReloadButtonInMinimalUi() const {
  return true;
}

bool SystemWebAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  return false;
}

bool SystemWebAppDelegate::ShouldHandleFileOpenIntents() const {
  return ShouldShowInLauncher();
}

std::optional<SystemWebAppBackgroundTaskInfo>
SystemWebAppDelegate::GetTimerInfo() const {
  return std::nullopt;
}

bool SystemWebAppDelegate::IsAppEnabled() const {
  return true;
}

gfx::Rect SystemWebAppDelegate::GetDefaultBounds(Browser* browser) const {
  return {};
}

bool SystemWebAppDelegate::HasCustomTabMenuModel() const {
  return false;
}

std::unique_ptr<ui::SimpleMenuModel> SystemWebAppDelegate::GetTabMenuModel(
    ui::SimpleMenuModel::Delegate* delegate) const {
  return nullptr;
}

bool SystemWebAppDelegate::ShouldShowTabContextMenuShortcut(
    Profile* profile,
    int command_id) const {
  return true;
}

bool SystemWebAppDelegate::ShouldRestoreOverrideUrl() const {
  return false;
}

bool SystemWebAppDelegate::IsUrlInSystemAppScope(const GURL& url) const {
  return false;
}

bool SystemWebAppDelegate::UseSystemThemeColor() const {
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
bool SystemWebAppDelegate::ShouldAnimateThemeChanges() const {
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool SystemWebAppDelegate::ShouldPinTab(GURL url) const {
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace ash
