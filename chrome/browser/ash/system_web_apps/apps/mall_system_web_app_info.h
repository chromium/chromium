// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MALL_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MALL_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

class MallSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit MallSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool IsAppEnabled() const override;
  std::vector<std::string> GetAppIdsToUninstallAndReplace() const override;
  bool ShouldCaptureNavigations() const override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MALL_SYSTEM_WEB_APP_INFO_H_
