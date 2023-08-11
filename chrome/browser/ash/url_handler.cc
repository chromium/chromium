// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/url_handler.h"

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/os_url_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

namespace {
crosapi::mojom::OpenUrlParams::WindowOpenDisposition GetDispositionForLacros(
    WindowOpenDisposition disposition) {
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::
          kNewForegroundTab;
    case WindowOpenDisposition::NEW_WINDOW:
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kNewWindow;
    case WindowOpenDisposition::OFF_THE_RECORD:
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::
          kOffTheRecord;
    case WindowOpenDisposition::SINGLETON_TAB:
    case WindowOpenDisposition::SWITCH_TO_TAB:
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kSwitchToTab;
    default:
      // Others are currently not supported.
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::
          kNewForegroundTab;
  }
}
}  // namespace

namespace ash {

bool TryOpenUrl(const GURL& url,
                WindowOpenDisposition disposition,
                NavigateParams::PathBehavior path_behavior,
                ChromeSchemeSemantics chrome_scheme_semantics) {
  if (!crosapi::browser_util::IsLacrosEnabled()) {
    return false;
  }

  if (chromeos::IsKioskSession()) {
    // Kiosk sessions already hide the navigation bar and block window creation.
    // Moreover, they don't support SWAs which we might end up trying to run
    // below.
    return false;
  }

  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    // We don't intercept CURRENT_TAB navigations.
    return false;
  }

  if (disposition == WindowOpenDisposition::NEW_POPUP) {
    // Some applications still open popup windows that need to stay in Ash:
    // - Gallery (chrome://media-app), see OPEN_IN_SANDBOXED_VIEWER in
    //   ash/webui/media_app_ui/resources/js/launch.js.
    // - nassh, see showLoginPopup in nassh/js/nassh_relay_corp.js.
    return false;
  }

  // It's okay to always use the primary user profile because Lacros does not
  // support multi-user sign-in.
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
          user_manager::UserManager::Get()->GetPrimaryUser()));
  if (!profile) {
    base::debug::DumpWithoutCrashing();
    DVLOG(1) << "TryOpenUrl is called when the primary user profile "
                "does not exist. This is a bug.";
    NOTREACHED();
    return false;
  }

  // Handle capturing system apps directly, as otherwise an additional empty
  // browser window could be created.
  const absl::optional<ash::SystemWebAppType> capturing_system_app_type =
      ash::GetCapturingSystemAppForURL(profile, url);
  if (capturing_system_app_type) {
    ash::SystemAppLaunchParams swa_params;
    swa_params.url = url;
    ash::LaunchSystemWebAppAsync(profile, capturing_system_app_type.value(),
                                 swa_params);
    return true;
  }

  // Forcibly open various URLs (mostly chrome://) in the OS_URL_HANDLER SWA.
  // Terminal's tabs must remain in the Terminal SWA.
  // TODO(neis): Actually limit this exception to Terminal if possible. Also,
  // remove Terminal from ChromeWebUIControllerFactory's GetListOfAcceptableURLs
  // or at least make TryLaunchOsUrlHandler return false for it somehow.
  if (!url.SchemeIs(content::kChromeUIUntrustedScheme) &&
      ((chrome_scheme_semantics == ChromeSchemeSemantics::kAsh) ||
       !url.SchemeIs(content::kChromeUIScheme)) &&
      ash::TryLaunchOsUrlHandler(url)) {
    return true;
  }

  // Intercept requests from Ash and redirect them to Lacros via crosapi. This
  // is to make window.open and <a href target="_blank"> links in SWAs (e.g.
  // ChromeOS Settings app) open in Lacros rather than in Ash. NOTE: This is
  // breaking change for calls to window.open, as the return value will always
  // be null. By excluding popups and devtools:// and chrome:// URLs, we exclude
  // the existing uses of window.open that make use of the return value (these
  // will have to be dealt with separately) as well as some existing links that
  // currently must remain in Ash.
  bool should_open_in_lacros =
      !url.SchemeIs(content::kChromeDevToolsScheme) &&
      ((chrome_scheme_semantics == ChromeSchemeSemantics::kLacros) ||
       !url.SchemeIs(content::kChromeUIScheme)) &&
      // Terminal's tabs must remain in Ash.
      !url.SchemeIs(content::kChromeUIUntrustedScheme) &&
      // OS Settings's Accessibility section links to chrome-extensions://
      // URLs for Text-to-Speech engines that are installed in Ash.
      !url.SchemeIs(extensions::kExtensionScheme);

  if (should_open_in_lacros) {
    auto lacros_disposition = GetDispositionForLacros(disposition);
    if (lacros_disposition ==
        crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kSwitchToTab) {
      crosapi::BrowserManager::Get()->SwitchToTab(url, path_behavior);
    } else {
      crosapi::BrowserManager::Get()->OpenUrl(
          url, crosapi::mojom::OpenUrlFrom::kUnspecified, lacros_disposition);
    }
    return true;
  }

  // We should get here only in exceptional cases. Some of these exceptions may
  // not even be needed anymore. Record a crash dump for various cases so that
  // we can better understand the situation. For now, continue as usual
  // afterwards (i.e. don't handle the request here). We know that Terminal
  // still needs to open Ash windows, no need to dump in that case.
  if (!(url.SchemeIs(content::kChromeUIUntrustedScheme) && url.has_host() &&
        url.host() == "terminal")) {
    SCOPED_CRASH_KEY_STRING32("ash", "TryOpenUrl", url.possibly_invalid_spec());
    base::debug::DumpWithoutCrashing();
    LOG(WARNING) << "Allowing Ash window creation for url " << url;
  }
  return false;
}

}  // namespace ash
