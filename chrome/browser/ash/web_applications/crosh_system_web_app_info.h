// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_SYSTEM_WEB_APP_INFO_H_

#include <memory>
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"

#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/rect.h"

struct WebAppInstallInfo;

class CroshSystemAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit CroshSystemAppDelegate(Profile* profile);

  // web_app::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldReuseExistingWindow() const override;
  bool ShouldShowInSearch() const override;
  bool ShouldHaveTabStrip() const override;
};

// Returns a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForCroshSystemWebApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_SYSTEM_WEB_APP_INFO_H_
