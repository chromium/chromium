// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_SETTINGS_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_SETTINGS_WEB_APP_INFO_H_

#include <memory>
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/rect.h"

struct WebAppInstallInfo;

class OSSettingsSystemAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit OSSettingsSystemAppDelegate(Profile* profile);

  // web_app::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  gfx::Size GetMinimumWindowSize() const override;
  std::vector<web_app::AppId> GetAppIdsToUninstallAndReplace() const override;
  bool PreferManifestBackgroundColor() const override;
  bool ShouldAnimateThemeChanges() const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForOSSettingsSystemWebApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_SETTINGS_WEB_APP_INFO_H_
