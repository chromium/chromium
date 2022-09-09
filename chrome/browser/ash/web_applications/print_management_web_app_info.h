// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PRINT_MANAGEMENT_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PRINT_MANAGEMENT_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "ui/gfx/geometry/size.h"

struct WebAppInstallInfo;

class PrintManagementSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit PrintManagementSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldShowInLauncher() const override;
  gfx::Size GetMinimumWindowSize() const override;
};

// Returns a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForPrintManagementApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PRINT_MANAGEMENT_WEB_APP_INFO_H_
