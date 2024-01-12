// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MEDIA_APP_MEDIA_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MEDIA_APP_MEDIA_WEB_APP_INFO_H_

#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

class MediaSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit MediaSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldShowInSearchAndShelf() const override;
  bool ShouldShowNewWindowMenuOption() const override;
  Browser* GetWindowForLaunch(Profile* profile, const GURL& url) const override;
  bool ShouldHandleFileOpenIntents() const override;
  base::FilePath GetLaunchDirectory(
      const apps::AppLaunchParams& params) const override;
  Browser* LaunchAndNavigateSystemWebApp(
      Profile* profile,
      web_app::WebAppProvider* provider,
      const GURL& url,
      const apps::AppLaunchParams& params) const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForMediaWebApp();

// Returns a snapshot of the product-specific data that is attached to HaTS for
// the MediaApp.
base::flat_map<std::string, std::string> HatsProductSpecificDataForMediaApp();

void SetPhotosExperienceSurveyTriggerAppIdForTesting(const char* app_id);

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MEDIA_APP_MEDIA_WEB_APP_INFO_H_
