// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_SAMPLE_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_SAMPLE_SYSTEM_WEB_APP_INFO_H_

#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_background_task_info.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

#if defined(OFFICIAL_BUILD)
#error Demo Mode should only be included in unofficial builds.
#endif

class SampleSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit SampleSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  ash::BrowserDelegate* GetWindowForLaunch(Profile* profile,
                                           const GURL& url) const override;
  bool ShouldShowNewWindowMenuOption() const override;
  std::optional<ash::SystemWebAppBackgroundTaskInfo> GetTimerInfo()
      const override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_SAMPLE_SYSTEM_WEB_APP_INFO_H_
