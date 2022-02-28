// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/web_feed_follow_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/feed/web_feed_page_information_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace feed {

WebFeedSubscriptions* GetSubscriptionsForProfile(Profile* profile) {
  FeedService* service = FeedServiceFactory::GetForBrowserContext(profile);
  if (!service)
    return nullptr;
  return &service->GetStream()->subscriptions();
}

void FollowSite(content::WebContents* web_contents) {
  auto on_page_info_fetched = [](WebFeedPageInformation page_info) {
    Profile* profile = ProfileManager::GetLastUsedProfile();
    if (!profile)
      return;

    WebFeedSubscriptions* subscriptions = GetSubscriptionsForProfile(profile);
    if (!subscriptions) {
      return;
    }
    auto on_followed = [](WebFeedSubscriptions::FollowWebFeedResult result) {
      // TODO(jianli): More UI hookup.
    };
    subscriptions->FollowWebFeed(page_info, base::BindOnce(on_followed));
  };

  WebFeedPageInformationFetcher::PageInformation page_info;
  page_info.url =
      web_contents->GetController().GetLastCommittedEntry()->GetURL();
  page_info.web_contents = web_contents;

  WebFeedPageInformationFetcher::Start(
      page_info, WebFeedPageInformationRequestReason::kUserRequestedFollow,
      base::BindOnce(on_page_info_fetched));
}

}  // namespace feed
