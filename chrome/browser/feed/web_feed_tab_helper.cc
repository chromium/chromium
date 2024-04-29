// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/web_feed_tab_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/feed/web_feed_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace feed {

namespace {

class WebFeedInfoFinderImpl : public WebFeedTabHelper::WebFeedInfoFinder {
 public:
  WebFeedInfoFinderImpl() = default;
  ~WebFeedInfoFinderImpl() override = default;

  void FindForPage(
      content::WebContents* web_contents,
      base::OnceCallback<void(WebFeedMetadata)> callback) override {
    FindWebFeedInfoForPage(
        web_contents,
        WebFeedPageInformationRequestReason::kMenuItemPresentation,
        std::move(callback));
  }
};

}  // namespace

WebFeedTabHelper::WebFeedTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WebFeedTabHelper>(*web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  web_feed_info_finder_ = std::make_unique<WebFeedInfoFinderImpl>();
}

WebFeedTabHelper::~WebFeedTabHelper() = default;

void WebFeedTabHelper::PrimaryPageChanged(content::Page& page) {
  // This is a new navigation so we can invalidate any previously scheduled
  // operations.
  weak_ptr_factory_.InvalidateWeakPtrs();

  web_feed_info_finder_->FindForPage(
      web_contents(), base::BindOnce(&WebFeedTabHelper::OnWebFeedInfoRetrieved,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     web_contents()->GetLastCommittedURL()));
}

void WebFeedTabHelper::OnWebFeedInfoRetrieved(const GURL& url,
                                              WebFeedMetadata metadata) {
  url_ = url;
  if (metadata.subscription_status == WebFeedSubscriptionStatus::kSubscribed) {
    follow_state_ = TabWebFeedFollowState::kFollowed;
    web_feed_id_ = metadata.web_feed_id;
  } else if (metadata.subscription_status ==
             WebFeedSubscriptionStatus::kNotSubscribed) {
    follow_state_ = TabWebFeedFollowState::kNotFollowed;
  } else {
    follow_state_ = TabWebFeedFollowState::kUnknown;
  }
}

void WebFeedTabHelper::UpdateWebFeedInfo(const GURL& url,
                                         TabWebFeedFollowState follow_state,
                                         const std::string& web_feed_id) {
  if (url != url_)
    return;
  follow_state_ = follow_state;
  web_feed_id_ = web_feed_id;
}

void WebFeedTabHelper::SetWebFeedInfoForTesting(
    const GURL& url,
    TabWebFeedFollowState follow_state,
    const std::string& web_feed_id) {
  url_ = url;
  follow_state_ = follow_state;
  web_feed_id_ = web_feed_id;
}

void WebFeedTabHelper::SetWebFeedInfoFinderForTesting(
    std::unique_ptr<WebFeedInfoFinder> finder) {
  web_feed_info_finder_ = std::move(finder);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebFeedTabHelper);

}  // namespace feed
