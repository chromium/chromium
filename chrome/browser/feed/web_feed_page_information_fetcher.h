// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_WEB_FEED_PAGE_INFORMATION_FETCHER_H_
#define CHROME_BROWSER_FEED_WEB_FEED_PAGE_INFORMATION_FETCHER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "components/feed/core/v2/public/types.h"

namespace content {
class WebContents;
}

namespace feed {

// TODO(carlosk): Add tests.
class WebFeedPageInformationFetcher
    : public base::RefCounted<WebFeedPageInformationFetcher> {
 public:
  struct PageInformation {
    GURL url;
    // web_contents should be set.
    raw_ptr<content::WebContents> web_contents = nullptr;
  };

  // Fetches the canonical URL and RSS URLs for a web page, and then calls
  // `callback` with the results.
  static void Start(const PageInformation& page_info,
                    const WebFeedPageInformationRequestReason reason,
                    base::OnceCallback<void(WebFeedPageInformation)> callback);

  // For internal use only.
  WebFeedPageInformationFetcher(
      const PageInformation& initial_page_info,
      base::OnceCallback<void(WebFeedPageInformation)> callback);

 private:
  friend class base::RefCounted<WebFeedPageInformationFetcher>;

  ~WebFeedPageInformationFetcher();

  void CallCallbackIfReady();
  void OnCanonicalUrlFetched(const std::optional<::GURL>& url);
  void OnRssFetched(std::vector<GURL> rss_urls);

  WebFeedPageInformation page_info_;
  base::OnceCallback<void(WebFeedPageInformation)> callback_;
  bool rss_fetched_ = false;
  bool url_fetched_ = false;
};

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_WEB_FEED_PAGE_INFORMATION_FETCHER_H_
