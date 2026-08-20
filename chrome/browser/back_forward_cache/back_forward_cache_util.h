// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACK_FORWARD_CACHE_BACK_FORWARD_CACHE_UTIL_H_
#define CHROME_BROWSER_BACK_FORWARD_CACHE_BACK_FORWARD_CACHE_UTIL_H_

class GURL;

namespace content {
class BrowserContext;
}  // namespace content

namespace chrome_back_forward_cache {

// Returns true if the given `url` is the search results page from the current
// default search engine for `browser_context`.
bool ShouldPrioritizeForBackForwardCache(
    content::BrowserContext* browser_context,
    const GURL& url);

}  // namespace chrome_back_forward_cache

#endif  // CHROME_BROWSER_BACK_FORWARD_CACHE_BACK_FORWARD_CACHE_UTIL_H_
