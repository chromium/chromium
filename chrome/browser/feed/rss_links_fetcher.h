// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_RSS_LINKS_FETCHER_H_
#define CHROME_BROWSER_FEED_RSS_LINKS_FETCHER_H_

#include <vector>

#include "components/feed/core/v2/public/types.h"
#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}
namespace feed {

// Fetch all RSS links associated with the main frame of the specified web
// contents.
void FetchRssLinks(const GURL& url,
                   content::WebContents* web_contents,
                   base::OnceCallback<void(std::vector<GURL>)> callback);

void FetchRssLinksForTesting(
    const GURL& url,
    mojo::Remote<feed::mojom::RssLinkReader> link_reader,
    base::OnceCallback<void(std::vector<GURL>)> callback);

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_RSS_LINKS_FETCHER_H_
