// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_DEMO_MODE_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_DEMO_MODE_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

class DemoModeSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit DemoModeSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  gfx::Size GetMinimumWindowSize() const override;
  bool IsAppEnabled() const override;
};

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForDemoModeApp();

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_DEMO_MODE_WEB_APP_INFO_H_
