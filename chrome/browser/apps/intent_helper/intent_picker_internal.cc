// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"

#include <utility>

#include "chrome/browser/apps/intent_helper/page_transition_util.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"

namespace apps {

namespace {

// Returns true if |url| is a known and valid redirector that will redirect a
// navigation elsewhere.
bool IsGoogleRedirectorUrl(const GURL& url) {
  // This currently only check for redirectors on the "google" domain.
  if (!page_load_metrics::IsGoogleSearchHostname(url))
    return false;

  return url.path_piece() == "/url" && url.has_query();
}

}  // namespace

bool ShouldCheckAppsForUrl(content::WebContents* web_contents) {
  // Do not check apps for url if no apps can be installed, e.g. in incognito.
  // Do not check apps for a no-state prefetcher navigation.
  if (!web_app::AreWebAppsUserInstallable(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) ||
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents) != nullptr) {
    return false;
  }

  // Do not check apps for url if we are already in an app browser.
  // It is possible that the web contents is not inserted to tab strip
  // model at this stage (e.g. open url in new tab). So if we cannot
  // find a browser at this moment, skip the check and this will be handled
  // in later stage.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && (browser->is_type_app() || browser->is_type_app_popup()))
    return false;

  return true;
}

void ShowIntentPickerBubbleForApps(content::WebContents* web_contents,
                                   std::vector<IntentPickerAppInfo> apps,
                                   bool show_stay_in_chrome,
                                   bool show_remember_selection,
                                   IntentPickerResponse callback) {
  if (apps.empty())
    return;

  // It should be safe to bind |web_contents| since closing the current tab will
  // close the intent picker and run the callback prior to the WebContents being
  // deallocated.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  browser->window()->ShowIntentPickerBubble(
      std::move(apps), show_stay_in_chrome, show_remember_selection,
      IntentPickerBubbleType::kLinkCapturing, absl::nullopt,
      std::move(callback));
}

bool InAppBrowser(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return !browser || browser->is_type_app() || browser->is_type_app_popup();
}

// Compares the host name of the referrer and target URL to decide whether
// the navigation needs to be overridden.
bool ShouldOverrideUrlLoading(const GURL& previous_url,
                              const GURL& current_url) {
  // When the navigation is initiated in a web page where sending a referrer
  // is disabled, |previous_url| can be empty. In this case, we should open
  // it in the desktop browser.
  if (!previous_url.is_valid() || previous_url.is_empty())
    return false;

  // Also check |current_url| just in case.
  if (!current_url.is_valid() || current_url.is_empty()) {
    DVLOG(1) << "Unexpected URL: " << current_url << ", opening it in Chrome.";
    return false;
  }

  // Check the scheme for both |previous_url| and |current_url| since an
  // extension could have referred us (e.g. Google Docs).
  if (!current_url.SchemeIsHTTPOrHTTPS() ||
      previous_url.SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }

  // Skip URL redirectors that are intermediate pages redirecting towards a
  // final URL.
  if (IsGoogleRedirectorUrl(current_url))
    return false;

  return true;
}

GURL GetStartingGURL(content::NavigationHandle* navigation_handle) {
  // This helps us determine a reference GURL for the current NavigationHandle.
  // This is the order or preference: Referrer > LastCommittedURL >
  // InitiatorOrigin. InitiatorOrigin *should* only be used on very rare cases,
  // e.g. when the navigation goes from https: to http: on a new tab, thus
  // losing the other potential referrers.
  const GURL referrer_url = navigation_handle->GetReferrer().url;
  if (referrer_url.is_valid() && !referrer_url.is_empty())
    return referrer_url;

  const GURL last_committed_url =
      navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (last_committed_url.is_valid() && !last_committed_url.is_empty())
    return last_committed_url;

  const auto& initiator_origin = navigation_handle->GetInitiatorOrigin();
  return initiator_origin.has_value() ? initiator_origin->GetURL() : GURL();
}

bool IsGoogleRedirectorUrlForTesting(const GURL& url) {
  return IsGoogleRedirectorUrl(url);
}

bool IsNavigateFromLink(content::NavigationHandle* navigation_handle) {
  // Always handle http(s) <form> submissions in Chrome for two reasons: 1) we
  // don't have a way to send POST data to ARC, and 2) intercepting http(s) form
  // submissions is not very important because such submissions are usually
  // done within the same domain. ShouldOverrideUrlLoading() below filters out
  // such submissions anyway.
  constexpr bool kAllowFormSubmit = false;

  ui::PageTransition page_transition = navigation_handle->GetPageTransition();

  return !ShouldIgnoreNavigation(page_transition, kAllowFormSubmit,
                                 navigation_handle->IsInFencedFrameTree(),
                                 navigation_handle->HasUserGesture()) &&
         !navigation_handle->WasStartedFromContextMenu() &&
         !navigation_handle->IsSameDocument();
}

void CloseOrGoBack(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (web_contents->GetController().CanGoBack())
    web_contents->GetController().GoBack();
  else
    web_contents->ClosePage();
}

PickerEntryType GetPickerEntryType(AppType app_type) {
  PickerEntryType picker_entry_type = PickerEntryType::kUnknown;
  switch (app_type) {
    case AppType::kUnknown:
    case AppType::kBuiltIn:
    case AppType::kCrostini:
    case AppType::kPluginVm:
    case AppType::kChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kRemote:
    case AppType::kBorealis:
    case AppType::kBruschetta:
    case AppType::kStandaloneBrowserExtension:
      break;
    case AppType::kArc:
      picker_entry_type = PickerEntryType::kArc;
      break;
    case AppType::kWeb:
    case AppType::kSystemWeb:
      picker_entry_type = PickerEntryType::kWeb;
      break;
    case AppType::kMacOs:
      picker_entry_type = PickerEntryType::kMacOs;
      break;
  }
  return picker_entry_type;
}

}  // namespace apps
