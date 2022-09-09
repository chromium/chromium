// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_diagnostics_dialog.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "content/public/browser/web_contents.h"

bool CanShowNetworkDiagnosticsDialog(content::WebContents* web_contents) {
  // The ChromeOS network diagnostics dialog can be shown in incognito and guest
  // profiles since it does not log the referring URL.
  return true;
}

void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const std::string& failed_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  chrome::ShowConnectivityDiagnosticsApp(profile);
}
