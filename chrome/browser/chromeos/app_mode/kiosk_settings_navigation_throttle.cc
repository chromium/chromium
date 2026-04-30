// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace chromeos {

namespace {

class KioskSettingsWindowObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<KioskSettingsWindowObserver> {
 public:
  ~KioskSettingsWindowObserver() override = default;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument() &&
        !KioskSettingsNavigationThrottle::IsSettingsPage(
            navigation_handle->GetURL().spec())) {
      LOG(WARNING) << "Kiosk: Force closing window "
                   << navigation_handle->GetURL().spec();
      web_contents()->ClosePage();
    }
  }

 private:
  explicit KioskSettingsWindowObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        content::WebContentsUserData<KioskSettingsWindowObserver>(*contents) {}

  friend class content::WebContentsUserData<KioskSettingsWindowObserver>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(KioskSettingsWindowObserver);

// This list is used in tests to override `DefaultSettingsPages`.
const std::vector<KioskSettingsNavigationThrottle::SettingsPage>*
    g_test_settings_pages = nullptr;

bool UrlMatchesSettingsPage(
    const KioskSettingsNavigationThrottle::SettingsPage& page,
    const std::string& url) {
  return (page.allow_subpages &&
          base::StartsWith(url,
                           page.url ? std::string(page.url) : std::string(),
                           base::CompareCase::SENSITIVE)) ||
         (!page.allow_subpages && url == page.url);
}

bool hasKioskWindowObserver(content::NavigationHandle& handle) {
  return KioskSettingsWindowObserver::FromWebContents(
             handle.GetWebContents()) != nullptr;
}

void addKioskWindowObserver(content::NavigationHandle& handle) {
  KioskSettingsWindowObserver::CreateForWebContents(handle.GetWebContents());
}

}  // namespace

// static
const std::vector<KioskSettingsNavigationThrottle::SettingsPage>&
KioskSettingsNavigationThrottle::DefaultSettingsPages() {
  static base::NoDestructor<std::vector<SettingsPage>> settings_pages{
      {SettingsPage{"chrome://os-settings/manageAccessibility", true},

       // Allow pages used by 'manageAccessibility'
       SettingsPage{"chrome://os-settings/textToSpeech", true},
       SettingsPage{"chrome://os-settings/displayAndMagnification", true},
       SettingsPage{"chrome://os-settings/keyboardAndTextInput", true},
       SettingsPage{"chrome://os-settings/cursorAndTouchpad", true},
       SettingsPage{"chrome://os-settings/audioAndCaptions", true},

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

  content::NavigationHandle& handle = registry.GetNavigationHandle();

  // If this is a settings page, observe all navigation.
  if (IsSettingsPage(handle.GetURL().spec())) {
    if (!hasKioskWindowObserver(handle)) {
      addKioskWindowObserver(handle);
    }
  }

  // If we are observing this page, attach a throttle to it.
  if (hasKioskWindowObserver(handle)) {
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
