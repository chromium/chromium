// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

// For Boca SWA.
class BocaSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit BocaSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  bool IsAppEnabled() const override;
};

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForBocaApp();

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_INFO_H_
