// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_ENTERPRISE_UTIL_H_
#define CHROME_BROWSER_INTERSTITIALS_ENTERPRISE_UTIL_H_

#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

namespace content {
class WebContents;
}

// If user is not in incognito mode, triggers
// "safeBrowsingPrivate.onSecurityInterstitialShown" extension event.
void MaybeTriggerSecurityInterstitialShownEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code);

// If user is not in incognito mode, triggers
// "safeBrowsingPrivate.onSecurityInterstitialProceeded" extension event.
void MaybeTriggerSecurityInterstitialProceededEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code);

#endif  // CHROME_BROWSER_INTERSTITIALS_ENTERPRISE_UTIL_H_
