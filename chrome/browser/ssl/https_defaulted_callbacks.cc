// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_defaulted_callbacks.h"

#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"
#include "chrome/common/chrome_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/url_constants.h"

bool ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttps(
    content::NavigationHandle* handle) {
  DCHECK_EQ(url::kHttpsScheme, handle->GetURL().scheme());

  // Check typed navigation upgrade status.
  if (base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps) &&
      TypedNavigationUpgradeThrottle::IsNavigationUsingHttpsAsDefaultScheme(
          handle)) {
    return true;
  }

  // Check HTTPS-Only Mode upgrade status.
  auto* https_only_mode_helper =
      HttpsOnlyModeTabHelper::FromWebContents(handle->GetWebContents());
  if (base::FeatureList::IsEnabled(features::kHttpsOnlyMode) &&
      https_only_mode_helper->is_navigation_upgraded()) {
    return true;
  }

  return false;
}
