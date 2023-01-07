// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FEED_FEED_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FEED_FEED_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/feed/feed.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feed/core/v2/public/ntp_feed_content_fetcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace ntp {

// Handles loading of articles for the Feed NTP module and user interaction with
// articles.
class FeedHandler : public ntp::feed::mojom::FeedHandler {
 public:
  static std::unique_ptr<FeedHandler> Create(
      mojo::PendingReceiver<ntp::feed::mojom::FeedHandler> handler,
      Profile* profile);

  FeedHandler(
      mojo::PendingReceiver<ntp::feed::mojom::FeedHandler> handler,
      std::unique_ptr<::feed::NtpFeedContentFetcher> ntp_feed_content_fetcher,
      Profile* profile);
  ~FeedHandler() override;

  void GetFollowingFeedArticles(
      ntp::feed::mojom::FeedHandler::GetFollowingFeedArticlesCallback callback)
      override;
  void ArticleOpened() override;

 private:
  mojo::Receiver<ntp::feed::mojom::FeedHandler> handler_;
  std::unique_ptr<::feed::NtpFeedContentFetcher> ntp_feed_content_fetcher_;
  raw_ptr<Profile> profile_;
};

}  // namespace ntp

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_FEED_FEED_HANDLER_H_
