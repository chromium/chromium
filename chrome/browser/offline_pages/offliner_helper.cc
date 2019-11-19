// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offliner_helper.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace offline_pages {

bool AreThirdPartyCookiesBlocked(content::BrowserContext* browser_context) {
  auto settings = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  return settings->ShouldBlockThirdPartyCookies();
}

bool IsNetworkPredictionDisabled(content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)
             ->GetPrefs()
             ->GetInteger(prefs::kNetworkPredictionOptions) ==
         chrome_browser_net::NETWORK_PREDICTION_NEVER;
}

}  // namespace offline_pages
