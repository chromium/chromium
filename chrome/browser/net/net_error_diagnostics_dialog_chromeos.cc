// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_diagnostics_dialog.h"

#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/chrome_pages.h"

namespace {
void LaunchDiagnosticsAppAtConnectivityScreen(Profile* profile) {
  std::string diagnostics_connectivity_url = {
      "chrome://diagnostics/?connectivity"};
  ash::SystemAppLaunchParams params;
  params.url = GURL(diagnostics_connectivity_url);
  LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::DIAGNOSTICS, params);
}
}  // namespace

bool CanShowNetworkDiagnosticsDialog(content::WebContents* web_contents) {
  // The ChromeOS network diagnostics dialog can be shown in incognito and guest
  // profiles since it does not log the referring URL.
  return true;
}

void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const std::string& failed_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  LaunchDiagnosticsAppAtConnectivityScreen(std::move(profile));
}
