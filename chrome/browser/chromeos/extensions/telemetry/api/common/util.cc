// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/util.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/shimless_rma/backend/external_app_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/tabs/public/tab_interface.h"
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

  content::WebContents* shimless_rma_contents =
      ash::shimless_rma::ExternalAppDialog::GetWebContents();
  if (shimless_rma_contents &&
      shimless_rma_contents->GetBrowserContext() == context &&
      IsWebContentsSecureAppUi(pattern_set, shimless_rma_contents)) {
    // In shimless, ExternalAppDialog is always on the top so we can assume it
    // is always focused.
    return shimless_rma_contents;
  }

  // A focused UI must be:
  // 1. In a browser that is front-most;
  // 2. In a tab that is active.
  ash::BrowserDelegate* const last_active =
      ash::BrowserController::GetInstance()->GetLastUsedBrowser();
  if (last_active && last_active->GetBrowser().GetProfile() == profile) {
    content::WebContents* const contents = last_active->GetActiveWebContents();
    if (contents && IsWebContentsSecureAppUi(pattern_set, contents)) {
      return contents;
    }
  }
  if (focused_ui_required) {
    return nullptr;
  }

  content::WebContents* found_contents = nullptr;
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingActivationTime,
      [&](ash::BrowserDelegate& target_browser) {
        if (target_browser.GetBrowser().GetProfile() != profile) {
          return ash::BrowserController::kContinueIteration;
        }

        for (tabs::TabInterface* const tab : target_browser.GetTabIterator()) {
          content::WebContents* const contents = tab->GetContents();
          if (IsWebContentsSecureAppUi(pattern_set, contents)) {
            found_contents = contents;
            return ash::BrowserController::kBreakIteration;
          }
        }
        return ash::BrowserController::kContinueIteration;
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
