// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_

#include <vector>

#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

struct WebAppInstallInfo;

class FileManagerSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit FileManagerSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  Browser* GetWindowForLaunch(Profile* profile, const GURL& url) const override;
  bool ShouldShowNewWindowMenuOption() const override;
  bool IsAppEnabled() const override;
  std::vector<std::string> GetAppIdsToUninstallAndReplace() const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForFileManager();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_
