// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINER_HELPER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINER_HELPER_H_

namespace content {
class BrowserContext;
}

namespace offline_pages {

bool AreThirdPartyCookiesBlocked(content::BrowserContext* browser_context);

bool IsNetworkPredictionDisabled(content::BrowserContext* browser_context);

enum class OfflinePagesCctApiPrerenderAllowedStatus {
  PRERENDER_ALLOWED,
  THIRD_PARTY_COOKIES_DISABLED,
  NETWORK_PREDICTION_DISABLED,
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINER_HELPER_H_
