// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_SYSTEM_APP_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_SYSTEM_APP_DELEGATE_H_

#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class Browser;

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

class PersonalizationSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit PersonalizationSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  gfx::Size GetMinimumWindowSize() const override;
  gfx::Rect GetDefaultBounds(Browser* browser) const override;
  bool ShouldCaptureNavigations() const override;
  bool IsAppEnabled() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearchAndShelf() const override;
  bool ShouldAnimateThemeChanges() const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForPersonalizationApp();

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_SYSTEM_APP_DELEGATE_H_
