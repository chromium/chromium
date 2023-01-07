// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/web_feed_ui_util.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/feed/web_feed_tab_helper.h"
#include "chrome/browser/feed/web_feed_util.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "content/public/browser/web_contents.h"

namespace feed {

void FollowSite(content::WebContents* web_contents) {
  auto on_followed = [](GURL url,
                        base::WeakPtr<content::WebContents> web_contents,
                        WebFeedSubscriptions::FollowWebFeedResult result) {
    if (!web_contents)
      return;

    // TODO(jianli): More UI hookup.

    // Update the web feed info cached in WebFeedTabHelper that provides
    // synchronous access for the tab menu.
    if (result.request_status == WebFeedSubscriptionRequestStatus::kSuccess) {
      WebFeedTabHelper* tab_helper =
          WebFeedTabHelper::FromWebContents(web_contents.get());
      if (tab_helper) {
        tab_helper->UpdateWebFeedInfo(url, TabWebFeedFollowState::kFollowed,
                                      result.web_feed_metadata.web_feed_id);
      }
    }
  };
  FollowWebFeed(web_contents,
                feedwire::webfeed::WebFeedChangeReason::WEB_PAGE_MENU,
                base::BindOnce(on_followed, web_contents->GetLastCommittedURL(),
                               web_contents->GetWeakPtr()));
}

void UnfollowSite(content::WebContents* web_contents) {
  WebFeedTabHelper* tab_helper =
      WebFeedTabHelper::FromWebContents(web_contents);
  if (!tab_helper || tab_helper->web_feed_id().empty())
    return;
  auto on_unfollowed = [](GURL url,
                          base::WeakPtr<content::WebContents> web_contents,
                          WebFeedSubscriptions::UnfollowWebFeedResult result) {
    if (!web_contents)
      return;

    // TODO(jianli): More UI hookup.

    // Update the web feed info cached in WebFeedTabHelper that provides
    // synchronous access for the tab menu.
    if (result.request_status == WebFeedSubscriptionRequestStatus::kSuccess) {
      WebFeedTabHelper* tab_helper =
          WebFeedTabHelper::FromWebContents(web_contents.get());
      if (tab_helper) {
        tab_helper->UpdateWebFeedInfo(url, TabWebFeedFollowState::kNotFollowed,
                                      std::string());
      }
    }
  };
  UnfollowWebFeed(
      tab_helper->web_feed_id(),
      /*is_durable_request=*/false,
      feedwire::webfeed::WebFeedChangeReason::WEB_PAGE_MENU,
      base::BindOnce(on_unfollowed, web_contents->GetLastCommittedURL(),
                     web_contents->GetWeakPtr()));
}

}  // namespace feed
