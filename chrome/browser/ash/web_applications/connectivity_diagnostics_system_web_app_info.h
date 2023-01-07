// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CONNECTIVITY_DIAGNOSTICS_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CONNECTIVITY_DIAGNOSTICS_SYSTEM_WEB_APP_INFO_H_

#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"

struct WebAppInstallInfo;

class ConnectivityDiagnosticsSystemAppDelegate
    : public ash::SystemWebAppDelegate {
 public:
  explicit ConnectivityDiagnosticsSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearch() const override;
};

// Returns a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo>
CreateWebAppInfoForConnectivityDiagnosticsSystemWebApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CONNECTIVITY_DIAGNOSTICS_SYSTEM_WEB_APP_INFO_H_
