// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_is_test.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

namespace {

// This list is used in tests to override `DefaultSettingsPages`.
const std::vector<KioskSettingsNavigationThrottle::SettingsPage>*
    g_test_settings_pages = nullptr;

// `WebContents` that are marked with this UserData key should be restricted.
const void* const kRestrictedSettingsWindowKey = &kRestrictedSettingsWindowKey;

bool UrlMatchesSettingsPage(
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
const std::vector<KioskSettingsNavigationThrottle::SettingsPage>&
KioskSettingsNavigationThrottle::DefaultSettingsPages() {
  static base::NoDestructor<std::vector<SettingsPage>> settings_pages{
      {SettingsPage{"chrome://os-settings/manageAccessibility", true},
       SettingsPage{
           "chrome-extension://mndnfokpggljbaajbnioimlmbfngpief/chromevox/"
           "options/options.html",
           false},
       SettingsPage{"chrome-extension://klbcgckkldhdhonijdbnhhaiedfkllef/",
                    true},
       SettingsPage{"chrome-extension://gjjabgpgjpampikjhjpfhneeoapjbjaf/",
                    true},
       SettingsPage{"chrome-extension://dakbfdmgjiabojdgbiljlhgjbokobjpg/",
                    true}}};

  return *settings_pages;
}

// static
bool KioskSettingsNavigationThrottle::IsSettingsPage(const std::string& url) {
  auto& pages = g_test_settings_pages != nullptr ? *g_test_settings_pages
                                                 : DefaultSettingsPages();
  for (auto& page : pages) {
    if (UrlMatchesSettingsPage(page, url)) {
      return true;
    }
  }
  return false;
}

// static
void KioskSettingsNavigationThrottle::SetSettingPagesForTesting(
    const std::vector<SettingsPage>* pages) {
  CHECK_IS_TEST();
  g_test_settings_pages = pages;
}

// static
void KioskSettingsNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Kiosk check.
  if (!IsRunningInForcedAppMode()) {
    return;
  }
  // If the web contents were previously marked as restricted, attach a throttle
  // to it.
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (handle.GetWebContents()->GetUserData(kRestrictedSettingsWindowKey)) {
    registry.AddThrottle(
        std::make_unique<KioskSettingsNavigationThrottle>(registry));
  }
  // Otherwise, check whether the navigated to url is a settings page, and if
  // so, mark it.
  if (IsSettingsPage(handle.GetURL().spec())) {
    handle.GetWebContents()->SetUserData(
        kRestrictedSettingsWindowKey,
        std::make_unique<content::WebContents::Data>());
    registry.AddThrottle(
        std::make_unique<KioskSettingsNavigationThrottle>(registry));
  }
}

KioskSettingsNavigationThrottle::KioskSettingsNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

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
