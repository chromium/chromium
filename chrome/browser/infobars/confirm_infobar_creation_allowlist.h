// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATION_ALLOWLIST_H_
#define CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATION_ALLOWLIST_H_

#include <string_view>

#include "base/types/pass_key.h"
#include "net/cert/cert_status_flags.h"

namespace content {
class WebContents;
}

namespace default_browser {
class PinInfoBarDelegate;
}

namespace extensions {
class IncognitoConnectabilityInfoBarDelegate;
class WebAuthFlowInfoBarDelegate;
}  // namespace extensions

namespace infobars {
class BrowserInfoBarManager;
}  // namespace infobars

namespace installer_downloader {
class InstallerDownloaderInfoBarDelegate;
}

namespace pdf::infobar {
class PdfInfoBarDelegate;
}

namespace policy::internal {
class LocalTestInfoBarVisibilityManager;
}

namespace session_restore_infobar {
class SessionRestoreInfoBarDelegate;
}

namespace web_app {
class WebAppBlockedMigrationInfoBarDelegate;
class WebAppUiManagerImpl;
}  // namespace web_app

// Compile-time gate controlling the creation of ConfirmInfoBars on Desktop.
//
// New Desktop infobars MUST use the centralized BrowserInfoBarManager framework
// (via InfoBarSpec) rather than directly subclassing ConfirmInfoBarDelegate or
// calling CreateConfirmInfoBar() / CreateSimpleAlertInfoBar().
class ConfirmInfoBarCreationAllowlist {
 public:
  using PassKey = base::PassKey<ConfirmInfoBarCreationAllowlist>;

  static PassKey GetPassKeyForTesting() { return PassKey(); }

 private:
  // Centralized Framework:
  friend class infobars::BrowserInfoBarManager;

  // Permanent Architectural Exceptions:
  friend class DevToolsWindow;
  friend class GlobalConfirmInfoBar;

  // Burndown List (Remaining Legacy Desktop Delegates):
  friend class AutomationInfoBarDelegate;
  friend class CollectedCookiesInfoBarDelegate;
  friend class DefaultBrowserInfoBarDelegate;
  friend class GoogleApiKeysInfoBarDelegate;
  friend class InfoBarInternalsHandler;
  friend class InstallationErrorInfoBarDelegate;
  friend class JsOptimizationsInfoBarDelegate;
  friend class KeystonePromotionInfoBarDelegate;
  friend void MaybeShowKnownInterceptionDisclosureDialog(content::WebContents*,
                                                         net::CertStatus);
  friend class ObsoleteSystemInfoBarDelegate;
  friend class OSCryptAsyncAvailabilityInfoBarDelegate;
  friend class PageInfoInfoBarDelegate;
  friend class StartupLaunchInfoBarManagerImpl;
  friend class ThemeInstalledInfoBarDelegate;
  friend class default_browser::PinInfoBarDelegate;
  friend class extensions::IncognitoConnectabilityInfoBarDelegate;
  friend class extensions::WebAuthFlowInfoBarDelegate;
  friend class installer_downloader::InstallerDownloaderInfoBarDelegate;
  friend class pdf::infobar::PdfInfoBarDelegate;
  friend class session_restore_infobar::SessionRestoreInfoBarDelegate;
  friend class web_app::WebAppBlockedMigrationInfoBarDelegate;
  friend class web_app::WebAppUiManagerImpl;

  // Burndown List (SimpleAlert callers):
  friend class ChromeSelectFilePolicy;
  friend class FlagsUI;
  friend class policy::internal::LocalTestInfoBarVisibilityManager;
  friend void ShowBadFlagsInfoBar(content::WebContents*, int, std::string_view);
  friend void ShowBadFlagsPrompt(content::WebContents*);

  ConfirmInfoBarCreationAllowlist() = default;

  // For the centralized framework and permanent architectural exceptions.
  static PassKey GetPassKey() { return PassKey(); }

  // For legacy callers in the burndown list pending migration/cleanup.
  static PassKey GetPassKeyForMigration() { return PassKey(); }
};

#endif  // CHROME_BROWSER_INFOBARS_CONFIRM_INFOBAR_CREATION_ALLOWLIST_H_
