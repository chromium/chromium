// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_SYSTEM_WEB_APP_INFO_H_

#include <memory>

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/rect.h"

struct WebAppInstallInfo;

class CroshSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit CroshSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldShowInLauncher() const override;
  Browser* GetWindowForLaunch(Profile* profile, const GURL& url) const override;
  bool ShouldShowInSearch() const override;
  bool ShouldHaveTabStrip() const override;
};

// Returns a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForCroshSystemWebApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_SYSTEM_WEB_APP_INFO_H_
