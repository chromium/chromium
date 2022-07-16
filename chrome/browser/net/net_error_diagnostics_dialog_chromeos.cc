// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_diagnostics_dialog.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"

namespace {
void LaunchDiagnosticsAppAtConnectivityScreen(Profile* profile) {
  DCHECK(ash::features::IsNetworkingInDiagnosticsAppEnabled());
  std::string diagnostics_connectivity_url = {
      "chrome://diagnostics/?connectivity"};
  web_app::SystemAppLaunchParams params;
  params.url = GURL(diagnostics_connectivity_url);
  LaunchSystemWebAppAsync(profile, web_app::SystemAppType::DIAGNOSTICS, params);
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

  if (ash::features::IsNetworkingInDiagnosticsAppEnabled()) {
    LaunchDiagnosticsAppAtConnectivityScreen(std::move(profile));
  } else {
    LaunchSystemWebAppAsync(profile,
                            web_app::SystemAppType::CONNECTIVITY_DIAGNOSTICS);
  }
}
