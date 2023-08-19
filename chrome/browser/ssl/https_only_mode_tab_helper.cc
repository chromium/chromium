// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_tab_helper.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"

HttpsOnlyModeTabHelper::~HttpsOnlyModeTabHelper() = default;

void HttpsOnlyModeTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the user was on an exempt net error and the tab was reloaded, only
  // reset the exempt error state, but keep the upgrade state so the reload
  // will result in continuing to attempt the upgraded navigation (and if it
  // later fails, the fallback will be to the original fallback URL).
  bool should_maintain_upgrade_state =
      is_exempt_error() &&
      navigation_handle->GetReloadType() != content::ReloadType::NONE;
  set_is_exempt_error(false);
  if (!should_maintain_upgrade_state) {
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
