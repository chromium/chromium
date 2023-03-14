// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_tab_helper.h"

#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"

HttpsOnlyModeTabHelper::~HttpsOnlyModeTabHelper() = default;

void HttpsOnlyModeTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // The original HTTPS-First Mode implementation expects these to stay set
  // across navigations and handles clearing them separately. Only reset if
  // the HFMv2 implementation is being used to avoid interfering with HFMv1.
  if (base::FeatureList::IsEnabled(features::kHttpsFirstModeV2)) {
    set_fallback_url(GURL());
    set_is_navigation_fallback(false);
    set_is_navigation_upgraded(false);
  }
}

HttpsOnlyModeTabHelper::HttpsOnlyModeTabHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<HttpsOnlyModeTabHelper>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HttpsOnlyModeTabHelper);
