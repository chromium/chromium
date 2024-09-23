// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"

#include "base/strings/string_util.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

namespace {
// List of pages, which along with their subpages are allowed in kiosk mode.
KioskSettingsNavigationThrottle::SettingsPage kSettingsPages[] = {
    {"chrome://os-settings/manageAccessibility", true},
    {"chrome-extension://mndnfokpggljbaajbnioimlmbfngpief/chromevox/options/"
     "options.html",
     false},
    {"chrome-extension://klbcgckkldhdhonijdbnhhaiedfkllef/", true},
    {"chrome-extension://gjjabgpgjpampikjhjpfhneeoapjbjaf/", true},
    {"chrome-extension://dakbfdmgjiabojdgbiljlhgjbokobjpg/", true}};

// This list is used in tests to replace default `kSettingsPages` items.
std::vector<KioskSettingsNavigationThrottle::SettingsPage>*
    g_test_settings_pages = nullptr;

// WebContents that are marked with this UserData key should be restricted.
const void* const kRestrictedSettingsWindowKey = &kRestrictedSettingsWindowKey;

bool CheckUrlMatchSettingsPage(
    const KioskSettingsNavigationThrottle::SettingsPage& page,
    const std::string& url) {
  return (page.allow_subpages &&
          base::StartsWith(url,
                           page.url ? std::string(page.url) : std::string(),
                           base::CompareCase::SENSITIVE)) ||
         (!page.allow_subpages && url == page.url);
}

}  // namespace

// static
bool KioskSettingsNavigationThrottle::IsSettingsPage(const std::string& url) {
  if (g_test_settings_pages) {
    for (auto& page : *g_test_settings_pages) {
      if (CheckUrlMatchSettingsPage(page, url)) {
        return true;
      }
    }
    return false;
  }
  for (auto& page : kSettingsPages) {
    if (CheckUrlMatchSettingsPage(page, url)) {
      return true;
    }
  }
  return false;
}

// static
void KioskSettingsNavigationThrottle::SetSettingPagesForTesting(
    std::vector<SettingsPage>* pages) {
  g_test_settings_pages = pages;
}

// static
std::unique_ptr<content::NavigationThrottle>
KioskSettingsNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Kiosk check.
  if (!IsRunningInForcedAppMode()) {
    return nullptr;
  }
  // If the web contents were previously marked as restricted, attach a throttle
  // to it.
  if (handle->GetWebContents()->GetUserData(kRestrictedSettingsWindowKey)) {
    return std::make_unique<KioskSettingsNavigationThrottle>(handle);
  }
  // Otherwise, check whether the navigated to url is a settings page, and if
  // so, mark it.
  if (IsSettingsPage(handle->GetURL().spec())) {
    handle->GetWebContents()->SetUserData(
        kRestrictedSettingsWindowKey,
        std::make_unique<content::WebContents::Data>());
    return std::make_unique<KioskSettingsNavigationThrottle>(handle);
  }
  return nullptr;
}

KioskSettingsNavigationThrottle::KioskSettingsNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

KioskSettingsNavigationThrottle::ThrottleCheckResult
KioskSettingsNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

KioskSettingsNavigationThrottle::ThrottleCheckResult
KioskSettingsNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

const char* KioskSettingsNavigationThrottle::GetNameForLogging() {
  return "KioskSettingsNavigationThrottle";
}

KioskSettingsNavigationThrottle::ThrottleCheckResult
KioskSettingsNavigationThrottle::WillStartOrRedirectRequest() {
  return IsSettingsPage(navigation_handle()->GetURL().spec()) ? PROCEED
                                                              : CANCEL;
}

}  // namespace chromeos
