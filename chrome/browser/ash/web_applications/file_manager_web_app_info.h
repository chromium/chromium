// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_

#include <vector>

#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_id.h"

struct WebAppInstallInfo;

class FileManagerSystemAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit FileManagerSystemAppDelegate(Profile* profile);

  // web_app::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldReuseExistingWindow() const override;
  bool ShouldShowNewWindowMenuOption() const override;
  bool IsAppEnabled() const override;
  std::vector<web_app::AppId> GetAppIdsToUninstallAndReplace() const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForFileManager();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_
