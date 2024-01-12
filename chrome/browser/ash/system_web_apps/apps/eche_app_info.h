// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_ECHE_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_ECHE_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "ui/gfx/geometry/rect.h"

class Browser;

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

class EcheSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit EcheSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearchAndShelf() const override;
  bool ShouldAllowResize() const override;
  bool ShouldAllowMaximize() const override;
  bool ShouldAllowFullscreen() const override;
  bool ShouldHaveReloadButtonInMinimalUi() const override;
  bool ShouldAllowScriptsToCloseWindows() const override;
  gfx::Rect GetDefaultBounds(Browser*) const override;
  bool IsAppEnabled() const override;

  gfx::Rect GetDefaultBoundsForEche() const;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForEcheApp();

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_ECHE_APP_INFO_H_
