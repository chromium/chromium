// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_INFO_H_

#include <memory>

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

// Forward declare browser and profile.
class Browser;
class Profile;

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

// For Boca SWA.
class BocaSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit BocaSystemAppDelegate(Profile* profile);
  BocaSystemAppDelegate(const BocaSystemAppDelegate&) = delete;
  BocaSystemAppDelegate& operator=(const BocaSystemAppDelegate&) = delete;
  ~BocaSystemAppDelegate() override = default;

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldAllowResize() const override;
  bool ShouldAllowMaximize() const override;
  bool ShouldHaveTabStrip() const override;
  bool ShouldHideNewTabButton() const override;
  bool IsUrlInSystemAppScope(const GURL& url) const override;
  bool ShouldPinTab(GURL url) const override;
  bool IsAppEnabled() const override;
  bool HasCustomTabMenuModel() const override;
  bool ShouldShowInSearchAndShelf() const override;
  bool ShouldShowInLauncher() const override;

  gfx::Size GetMinimumWindowSize() const override;
  std::unique_ptr<ui::SimpleMenuModel> GetTabMenuModel(
      ui::SimpleMenuModel::Delegate* delegate) const override;
  Browser* LaunchAndNavigateSystemWebApp(
      Profile* profile,
      web_app::WebAppProvider* provider,
      const GURL& url,
      const apps::AppLaunchParams& params) const override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_INFO_H_
