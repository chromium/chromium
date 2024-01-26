// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/web_feed_page_information_fetcher.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/feed/rss_links_fetcher.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace feed {

using PageInformation = WebFeedPageInformationFetcher::PageInformation;

namespace {

void FetchPageCanonicalUrl(
    const PageInformation& page_info,
    base::OnceCallback<void(const std::optional<::GURL>&)> callback) {
  DCHECK(page_info.web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  page_info.web_contents->GetPrimaryMainFrame()->GetCanonicalUrl(
      std::move(callback));
}

}  // namespace

// static
void WebFeedPageInformationFetcher::Start(
    const PageInformation& page_info,
    const WebFeedPageInformationRequestReason reason,
    base::OnceCallback<void(WebFeedPageInformation)> callback) {
  DVLOG(2) << "PageInformationRequested reason=" << reason;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.WebFeed.PageInformationRequested", reason);

  // Make sure the renderer still exists, i.e., not crashed, since this may
  // be triggered asynchronously.
  if (!page_info.web_contents->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    std::move(callback).Run(WebFeedPageInformation());
    return;
  }

  // Perform two async operations, and call `callback` only after both are
  // complete. Keep state as RefCounted, owned by the callbacks.
  auto self = base::MakeRefCounted<WebFeedPageInformationFetcher>(
      page_info, std::move(callback));

  FetchRssLinks(
      page_info.url, page_info.web_contents,
      base::BindOnce(&WebFeedPageInformationFetcher::OnRssFetched, self));
  FetchPageCanonicalUrl(
      page_info,
      base::BindOnce(&WebFeedPageInformationFetcher::OnCanonicalUrlFetched,
                     self));
}

WebFeedPageInformationFetcher::WebFeedPageInformationFetcher(
    const PageInformation& initial_page_info,
    base::OnceCallback<void(WebFeedPageInformation)> callback)
    : callback_(std::move(callback)) {
  page_info_.SetUrl(initial_page_info.url);
}

WebFeedPageInformationFetcher::~WebFeedPageInformationFetcher() = default;

void WebFeedPageInformationFetcher::CallCallbackIfReady() {
  if (rss_fetched_ && url_fetched_)
    std::move(callback_).Run(std::move(page_info_));
}

void WebFeedPageInformationFetcher::OnCanonicalUrlFetched(
    const std::optional<::GURL>& url) {
  if (url) {
    page_info_.SetCanonicalUrl(*url);
  }

  url_fetched_ = true;
  CallCallbackIfReady();
}

void WebFeedPageInformationFetcher::OnRssFetched(std::vector<GURL> rss_urls) {
  page_info_.SetRssUrls(std::move(rss_urls));
  rss_fetched_ = true;
  CallCallbackIfReady();
}

}  // namespace feed
