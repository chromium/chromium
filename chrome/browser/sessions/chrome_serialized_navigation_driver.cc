// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/chrome_serialized_navigation_driver.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/common/referrer.h"

#if defined(OS_ANDROID)
#include "content/public/common/content_features.h"
#include "content/public/common/page_state.h"
#endif

namespace {

bool IsUberOrUberReplacementURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         (url.host_piece() == chrome::kChromeUIHistoryHost ||
          url.host_piece() == chrome::kChromeUIUberHost ||
          url.host_piece() == chrome::kChromeUISettingsHost ||
          url.host_piece() == chrome::kChromeUIHelpHost);
}

}  // namespace

ChromeSerializedNavigationDriver::~ChromeSerializedNavigationDriver() {}

// static
ChromeSerializedNavigationDriver*
ChromeSerializedNavigationDriver::GetInstance() {
  return base::Singleton<
      ChromeSerializedNavigationDriver,
      base::LeakySingletonTraits<ChromeSerializedNavigationDriver>>::get();
}

void ChromeSerializedNavigationDriver::Sanitize(
    sessions::SerializedNavigationEntry* navigation) const {
  content::Referrer old_referrer(navigation->referrer_url(),
                                 static_cast<network::mojom::ReferrerPolicy>(
                                     navigation->referrer_policy()));
  content::Referrer new_referrer = content::Referrer::SanitizeForRequest(
      navigation->virtual_url(), old_referrer);

  // Clear any Uber UI page state so that these pages are reloaded rather than
  // restored from page state. This fixes session restore when WebUI URLs
  // change.
  if (IsUberOrUberReplacementURL(navigation->virtual_url()) &&
      IsUberOrUberReplacementURL(navigation->original_request_url())) {
    navigation->set_encoded_page_state(std::string());
  }

  // No need to compare the policy, as it doesn't change during
  // sanitization. If there has been a change, the referrer needs to be
  // stripped from the page state as well.
  if (navigation->referrer_url() != new_referrer.url) {
    auto* driver = sessions::SerializedNavigationDriver::Get();
    navigation->set_referrer_url(GURL());
    navigation->set_referrer_policy(driver->GetDefaultReferrerPolicy());
    navigation->set_encoded_page_state(
        driver->StripReferrerFromPageState(navigation->encoded_page_state()));
  }

#if defined(OS_ANDROID)
  // Rewrite the old new tab and welcome page URLs to the new NTP URL.
  if (navigation->virtual_url().SchemeIs(content::kChromeUIScheme) &&
      (navigation->virtual_url().host_piece() == chrome::kChromeUIWelcomeHost ||
       navigation->virtual_url().host_piece() == chrome::kChromeUINewTabHost)) {
    navigation->set_virtual_url(GURL(chrome::kChromeUINativeNewTabURL));
    navigation->set_original_request_url(navigation->virtual_url());
    navigation->set_encoded_page_state(
        content::PageState::CreateFromURL(navigation->virtual_url())
            .ToEncodedData());
  }

  if (navigation->virtual_url().SchemeIs(content::kChromeUIScheme) &&
      (navigation->virtual_url().host_piece() == chrome::kChromeUIHistoryHost ||
       navigation->virtual_url().host_piece() ==
           chrome::kDeprecatedChromeUIHistoryFrameHost)) {
    // Rewrite the old history Web UI to the new android native history.
    navigation->set_virtual_url(GURL(chrome::kChromeUINativeHistoryURL));
    navigation->set_original_request_url(navigation->virtual_url());
    navigation->set_encoded_page_state(
        content::PageState::CreateFromURL(navigation->virtual_url())
            .ToEncodedData());
  }
#endif  // defined(OS_ANDROID)
}

ChromeSerializedNavigationDriver::ChromeSerializedNavigationDriver() {}
