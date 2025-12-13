// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/util.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/shimless_rma/backend/external_app_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/webapps/isolated_web_apps/scheme.h"
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
  if (contents->GetLastCommittedURL().SchemeIs(webapps::kIsolatedAppScheme)) {
    return true;
  }
  // Ensure the URL connection is secure (e.g. valid certificate).
  const auto visible_security_state =
      security_state::GetVisibleSecurityState(contents);
  return security_state::GetSecurityLevel(*visible_security_state) ==
         security_state::SecurityLevel::SECURE;
}

bool IsWebContentsSecureAppUi(const extensions::URLPatternSet& pattern_set,
                              content::WebContents* contents) {
  return pattern_set.MatchesURL(contents->GetLastCommittedURL()) &&
         IsWebContentsSecure(contents);
}

}  // namespace

content::WebContents* FindTelemetryExtensionOpenAndSecureAppUi(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    bool focused_ui_required) {
  Profile* profile = Profile::FromBrowserContext(context);
  const auto& pattern_set =
      extensions::ExternallyConnectableInfo::Get(extension)->matches;

  if (ash::features::IsShimlessRMA3pDiagnosticsEnabled()) {
    content::WebContents* contents =
        ash::shimless_rma::ExternalAppDialog::GetWebContents();
    if (contents && contents->GetBrowserContext() == context &&
        IsWebContentsSecureAppUi(pattern_set, contents)) {
      // In shimless, ExternalAppDialog is always on the top so we can assume it
      // is always focused.
      return contents;
    }
  }

  // A focused UI must be:
  // 1. In a browser that is front-most;
  // 2. In a tab that is active.
  BrowserWindowInterface* const last_active_bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (last_active_bwi && last_active_bwi->GetProfile() == profile) {
    content::WebContents* const contents =
        last_active_bwi->GetTabStripModel()->GetActiveWebContents();
    if (contents && IsWebContentsSecureAppUi(pattern_set, contents)) {
      return contents;
    }
  }
  if (focused_ui_required) {
    return nullptr;
  }

  content::WebContents* found_contents = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* target_bwi) {
        if (target_bwi->GetProfile() != profile) {
          return true;  // Continue iteration
        }

        TabStripModel* const target_tab_strip = target_bwi->GetTabStripModel();
        for (int i = 0; i < target_tab_strip->count(); ++i) {
          content::WebContents* const contents =
              target_tab_strip->GetWebContentsAt(i);
          if (IsWebContentsSecureAppUi(pattern_set, contents)) {
            found_contents = contents;
            return false;  // Stop iteration
          }
        }
        return true;  // Continue iteration
      });
  return found_contents;
}

bool IsTelemetryExtensionAppUiOpenAndSecure(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  return FindTelemetryExtensionOpenAndSecureAppUi(context, extension) !=
         nullptr;
}

}  // namespace chromeos
