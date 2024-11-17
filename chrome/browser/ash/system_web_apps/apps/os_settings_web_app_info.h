// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_SETTINGS_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_SETTINGS_WEB_APP_INFO_H_

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

class OSSettingsSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit OSSettingsSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  gfx::Size GetMinimumWindowSize() const override;
  std::vector<std::string> GetAppIdsToUninstallAndReplace() const override;
  bool ShouldAnimateThemeChanges() const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForOSSettingsSystemWebApp();

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_SETTINGS_WEB_APP_INFO_H_
