// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_diagnostics_dialog.h"

#include "apps/launcher.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chromeos/components/connectivity_diagnostics/url_constants.h"
#include "extensions/browser/extension_registry.h"

bool CanShowNetworkDiagnosticsDialog(content::WebContents* web_contents) {
  // The ChromeOS network diagnostics dialog can be shown in incognito and guest
  // profiles since it does not log the referring URL.
  return true;
}

void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const std::string& failed_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (base::FeatureList::IsEnabled(
          chromeos::features::kConnectivityDiagnosticsWebUi)) {
    LaunchSystemWebAppAsync(profile,
                            web_app::SystemAppType::CONNECTIVITY_DIAGNOSTICS);
  } else {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
            "kodldpbjkkmmnilagfdheibampofhaom");
    apps::LaunchPlatformAppWithUrl(web_contents->GetBrowserContext(), extension,
                                   "", GURL::EmptyGURL(), GURL(failed_url));
  }
}
