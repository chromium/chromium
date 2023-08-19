// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_defaulted_callbacks.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/url_constants.h"

bool ShouldIgnoreSslInterstitialBecauseNavigationDefaultedToHttps(
    content::NavigationHandle* handle) {
  DCHECK_EQ(url::kHttpsScheme, handle->GetURL().scheme());

  // Check typed navigation upgrade status.
  if (base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps) &&
      TypedNavigationUpgradeThrottle::IsNavigationUsingHttpsAsDefaultScheme(
          handle)) {
    return true;
  }

  // Check HTTPS-Only Mode upgrade status.
  // Suppress any SSL errors if the navigation was upgraded to HTTPS by
  // HTTPS-First Mode but it has not yet fallen back to HTTP. If the user
  // already clicked through the HTTPS-First Mode interstitial then the SSL
  // error should no longer be suppressed.
  auto* https_only_mode_helper =
      HttpsOnlyModeTabHelper::FromWebContents(handle->GetWebContents());
  bool is_upgraded = https_only_mode_helper &&
                     https_only_mode_helper->is_navigation_upgraded();

  Profile* profile = Profile::FromBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  if (!profile) {
    // In the edge case of there not being a Profile associated with this
    // navigation, fail safe by not suppressing SSL interstitials.
    return false;
  }
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());
  bool is_allowlisted =
      state && state->IsHttpAllowedForHost(handle->GetURL().host(),
                                           handle->GetWebContents()
                                               ->GetPrimaryMainFrame()
                                               ->GetStoragePartition());

  return is_upgraded && !is_allowlisted;
}
