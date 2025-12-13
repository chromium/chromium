// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REFERRER_CACHE_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REFERRER_CACHE_UTILS_H_

#include "base/supports_user_data.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

namespace enterprise_connectors {

// This function returns a `safe_browsing::ReferrerChain` to be used for DLP
// checks or reporting for enterprise functionality.
safe_browsing::ReferrerChain GetReferrerChain(
    const GURL& url,
    content::WebContents& web_contents);

// Create a chain containing `download_item`'s URL and whatever corresponding
// chain can be found from the cache in its web contents.
safe_browsing::ReferrerChain GetOrCreateReferrerChain(
    download::DownloadItem& download_item);

// Get the cached chain for the given download if it exists.
safe_browsing::ReferrerChain GetCachedReferrerChain(
    download::DownloadItem& download_item);

// Obtains the referrer chain for the given arguments and caches it in
// `web_contents`'s user data.
void SetReferrerChain(const GURL& url, content::WebContents& web_contents);

// Cache the given `referrer_chain` into the passed download item's user data.
void SetReferrerChain(safe_browsing::ReferrerChain referrer_chain,
                      download::DownloadItem& download_item);

// Test-only helper to check if a value has already been cached for the existing
// web contents or download item.
bool HasCachedChainForTesting(base::SupportsUserData& supports_user_data);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REFERRER_CACHE_UTILS_H_
