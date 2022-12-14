// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_DIAGNOSTICS_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_DIAGNOSTICS_SYSTEM_WEB_APP_INFO_H_

#include <memory>
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"

#include "ui/gfx/geometry/rect.h"

struct WebAppInstallInfo;

class DiagnosticsSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit DiagnosticsSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldShowInLauncher() const override;
  gfx::Size GetMinimumWindowSize() const override;
  bool ShouldCaptureNavigations() const override;
  Browser* LaunchAndNavigateSystemWebApp(
      Profile* profile,
      web_app::WebAppProvider* provider,
      const GURL& url,
      const apps::AppLaunchParams& params) const override;
};

// Returns a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForDiagnosticsSystemWebApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_DIAGNOSTICS_SYSTEM_WEB_APP_INFO_H_
