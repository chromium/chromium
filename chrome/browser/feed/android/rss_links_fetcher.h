// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ANDROID_RSS_LINKS_FETCHER_H_
#define CHROME_BROWSER_FEED_ANDROID_RSS_LINKS_FETCHER_H_

#include <vector>

#include "chrome/browser/feed/android/feed_service_factory.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/mojom/rss_link_reader.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

class TabAndroid;
namespace service_manager {
class InterfaceProvider;
}
namespace feed {
namespace internal {
service_manager::InterfaceProvider* GetRenderFrameRemoteInterfaces(
    TabAndroid* tab);
}

void FetchRssLinks(const GURL& url,
                   TabAndroid* page_tab,
                   base::OnceCallback<void(std::vector<GURL>)> callback);

void FetchRssLinks(const GURL& url,
                   mojo::Remote<feed::mojom::RssLinkReader> link_reader,
                   base::OnceCallback<void(std::vector<GURL>)> callback);

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_ANDROID_RSS_LINKS_FETCHER_H_
