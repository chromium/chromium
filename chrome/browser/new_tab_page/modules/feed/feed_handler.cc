// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/feed/feed_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_version.h"
#include "components/feed/core/v2/public/ntp_feed_content_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

class Browser;

namespace ntp {
namespace {

void HandleFetchArticlesResponse(
    ntp::feed::mojom::FeedHandler::GetFollowingFeedArticlesCallback callback,
    std::vector<::feed::NtpFeedContentFetcher::Article> articles) {
  std::vector<ntp::feed::mojom::ArticlePtr> result;
  for (auto& article : articles) {
    auto article_ptr = ntp::feed::mojom::Article::New();
    article_ptr->title = std::move(article.title);
    article_ptr->publisher = std::move(article.publisher);
    article_ptr->url = std::move(article.url);
    article_ptr->thumbnail_url = std::move(article.thumbnail_url);
    article_ptr->favicon_url = std::move(article.favicon_url);
    result.push_back(std::move(article_ptr));
  }
  std::move(callback).Run(mojo::Clone(result));
}

}  // namespace

// static
std::unique_ptr<FeedHandler> FeedHandler::Create(
    mojo::PendingReceiver<ntp::feed::mojom::FeedHandler> handler,
    Profile* profile) {
  auto url_loader_factory = profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string api_key;
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  return std::make_unique<FeedHandler>(
      std::move(handler),
      std::make_unique<::feed::NtpFeedContentFetcher>(
          identity_manager, url_loader_factory, api_key, profile->GetPrefs()),
      profile);
}

FeedHandler::FeedHandler(
    mojo::PendingReceiver<ntp::feed::mojom::FeedHandler> handler,
    std::unique_ptr<::feed::NtpFeedContentFetcher> ntp_feed_content_fetcher,
    Profile* profile)
    : handler_(this, std::move(handler)),
      ntp_feed_content_fetcher_(std::move(ntp_feed_content_fetcher)),
      profile_(profile) {}

FeedHandler::~FeedHandler() = default;

void FeedHandler::GetFollowingFeedArticles(
    ntp::feed::mojom::FeedHandler::GetFollowingFeedArticlesCallback callback) {
  ntp_feed_content_fetcher_->FetchFollowingFeedArticles(
      base::BindOnce(&HandleFetchArticlesResponse, std::move(callback)));
}

void FeedHandler::ArticleOpened() {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)
    return;

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return;

  // TODO(https://crbug.com/1341399): When possible, show the side panel feed in
  // a way that doesn't depend on Views and remove the DEPS rule.
  if (browser_view->side_panel_coordinator()) {
    browser_view->side_panel_coordinator()->Show(SidePanelEntry::Id::kFeed);
  }
}

}  // namespace ntp
