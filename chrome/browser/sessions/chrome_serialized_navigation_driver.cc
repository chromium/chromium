// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/chrome_serialized_navigation_driver.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/common/referrer.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
// Mutates |navigation| so that it targets |new_destination_url| and has no
// referrer information.
void ChangeDestination(const GURL& new_destination_url,
                       sessions::SerializedNavigationEntry* navigation) {
  navigation->set_virtual_url(new_destination_url);
  navigation->set_original_request_url(new_destination_url);
  navigation->set_encoded_page_state(
      blink::PageState::CreateFromURL(new_destination_url).ToEncodedData());

  // Make sure the referrer stored in the PageState (above) and in the
  // SerializedNavigationEntry (below) are in-sync.
  navigation->set_referrer_url(GURL());
  navigation->set_referrer_policy(
      static_cast<int>(network::mojom::ReferrerPolicy::kDefault));
}
#endif  // BUILDFLAG(IS_ANDROID)

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
  content::Referrer old_referrer(
      navigation->referrer_url(),
      content::Referrer::ConvertToPolicy(navigation->referrer_policy()));
  content::Referrer new_referrer = content::Referrer::SanitizeForRequest(
      navigation->virtual_url(), old_referrer);

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

#if BUILDFLAG(IS_ANDROID)
  // Rewrite the old new tab and welcome page URLs to the new NTP URL.
  if (navigation->virtual_url().SchemeIs(content::kChromeUIScheme) &&
      (navigation->virtual_url().host_piece() == chrome::kChromeUIWelcomeHost ||
       navigation->virtual_url().host_piece() == chrome::kChromeUINewTabHost)) {
    ChangeDestination(GURL(chrome::kChromeUINativeNewTabURL), navigation);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

ChromeSerializedNavigationDriver::ChromeSerializedNavigationDriver() {}
