// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/web_feed_util.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/feed/web_feed_page_information_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace feed {

namespace {

WebFeedPageInformationFetcher::PageInformation ConstructPageInformation(
    content::WebContents* web_contents) {
  WebFeedPageInformationFetcher::PageInformation page_info;
  page_info.url = web_contents->GetLastCommittedURL();
  page_info.web_contents = web_contents;
  return page_info;
}

}  // namespace

WebFeedSubscriptions* GetSubscriptionsForProfile(Profile* profile) {
  FeedService* service = FeedServiceFactory::GetForBrowserContext(profile);
  if (!service)
    return nullptr;
  return &service->GetStream()->subscriptions();
}

void FindWebFeedInfoForPage(
    content::WebContents* web_contents,
    WebFeedPageInformationRequestReason reason,
    base::OnceCallback<void(WebFeedMetadata)> callback) {
  auto on_page_info_fetched =
      [](base::WeakPtr<content::WebContents> web_contents,
         base::OnceCallback<void(WebFeedMetadata)> callback,
         WebFeedPageInformation page_info) {
        if (!web_contents)
          return;
        Profile* profile =
            Profile::FromBrowserContext(web_contents->GetBrowserContext());
        if (!profile) {
          std::move(callback).Run({});
          return;
        }

        WebFeedSubscriptions* subscriptions =
            GetSubscriptionsForProfile(profile);
        if (!subscriptions) {
          std::move(callback).Run({});
          return;
        }

        subscriptions->FindWebFeedInfoForPage(page_info, std::move(callback));
      };

  WebFeedPageInformationFetcher::Start(
      ConstructPageInformation(web_contents), reason,
      base::BindOnce(on_page_info_fetched, web_contents->GetWeakPtr(),
                     std::move(callback)));
}

void FollowWebFeed(
    content::WebContents* web_contents,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(WebFeedSubscriptions::FollowWebFeedResult)>
        callback) {
  auto on_page_info_fetched =
      [](feedwire::webfeed::WebFeedChangeReason change_reason,
         base::WeakPtr<content::WebContents> web_contents,
         base::OnceCallback<void(WebFeedSubscriptions::FollowWebFeedResult)>
             callback,
         WebFeedPageInformation page_info) {
        if (page_info.url().is_empty() || !web_contents)
          return;

        Profile* profile =
            Profile::FromBrowserContext(web_contents->GetBrowserContext());
        if (!profile) {
          std::move(callback).Run({});
          return;
        }

        WebFeedSubscriptions* subscriptions =
            GetSubscriptionsForProfile(profile);
        if (!subscriptions) {
          std::move(callback).Run({});
          return;
        }

        subscriptions->FollowWebFeed(page_info, change_reason,
                                     std::move(callback));
      };

  WebFeedPageInformationFetcher::Start(
      ConstructPageInformation(web_contents),
      WebFeedPageInformationRequestReason::kUserRequestedFollow,
      base::BindOnce(on_page_info_fetched, change_reason,
                     web_contents->GetWeakPtr(), std::move(callback)));
}

void UnfollowWebFeed(
    const std::string& web_feed_id,
    bool is_durable_request,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(WebFeedSubscriptions::UnfollowWebFeedResult)>
        callback) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  WebFeedSubscriptions* subscriptions = GetSubscriptionsForProfile(profile);
  if (!subscriptions) {
    std::move(callback).Run({});
    return;
  }

  subscriptions->UnfollowWebFeed(web_feed_id, is_durable_request, change_reason,
                                 std::move(callback));
}

}  // namespace feed
