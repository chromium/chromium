// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/url_pattern_set.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

namespace {

bool IsWebContentsSecure(content::WebContents* contents) {
  // TODO(b/290909386): Remove this line once we reach a conclusion on
  // how we should perform security check on IWA.
  if (chromeos::features::IsIWAForTelemetryExtensionAPIEnabled() &&
      contents->GetLastCommittedURL().SchemeIs(chrome::kIsolatedAppScheme)) {
    return true;
  }
  // Ensure the URL connection is secure (e.g. valid certificate).
  const auto visible_security_state =
      security_state::GetVisibleSecurityState(contents);
  return security_state::GetSecurityLevel(
             *visible_security_state,
             /*used_policy_installed_certificate=*/false) ==
         security_state::SecurityLevel::SECURE;
}

}  // namespace

content::WebContents* FindTelemetryExtensionOpenAndSecureAppUi(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  Profile* profile = Profile::FromBrowserContext(context);
  const auto& pattern_set =
      extensions::ExternallyConnectableInfo::Get(extension)->matches;

  for (auto* target_browser : *BrowserList::GetInstance()) {
    if (target_browser->profile() != profile) {
      continue;
    }

    TabStripModel* target_tab_strip = target_browser->tab_strip_model();
    for (int i = 0; i < target_tab_strip->count(); ++i) {
      content::WebContents* contents = target_tab_strip->GetWebContentsAt(i);
      if (pattern_set.MatchesURL(contents->GetLastCommittedURL()) &&
          IsWebContentsSecure(contents)) {
        return contents;
      }
    }
  }
  return nullptr;
}

bool IsTelemetryExtensionAppUiOpenAndSecure(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  return FindTelemetryExtensionOpenAndSecureAppUi(context, extension) !=
         nullptr;
}

}  // namespace chromeos
