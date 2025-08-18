// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REFERRER_CACHE_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REFERRER_CACHE_UTILS_H_

#include "components/safe_browsing/core/browser/referrer_chain_provider.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_connectors {

// This function returns a `safe_browsing::ReferrerChain` to be used for DLP
// checks or reporting for enterprise functionality.
safe_browsing::ReferrerChain GetReferrerChain(
    const GURL& url,
    content::WebContents& web_contents);

// Obtains the referrer chain for the given arguments and caches it in
// `web_contents`'s user data.
void SetReferrerChain(const GURL& url, content::WebContents& web_contents);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REFERRER_CACHE_UTILS_H_
