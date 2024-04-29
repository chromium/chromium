// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_WEB_FEED_TAB_HELPER_H_
#define CHROME_BROWSER_FEED_WEB_FEED_TAB_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "components/feed/core/v2/public/types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace feed {

// Per-tab class that monitors the navigations and stores the necessary info
// to facilitate the synchronous access to web feed information.
class WebFeedTabHelper : public content::WebContentsObserver,
                         public content::WebContentsUserData<WebFeedTabHelper> {
 public:
  class WebFeedInfoFinder {
   public:
    virtual ~WebFeedInfoFinder() = default;
    virtual void FindForPage(
        content::WebContents* web_contents,
        base::OnceCallback<void(WebFeedMetadata)> callback) = 0;
  };

  WebFeedTabHelper(const WebFeedTabHelper&) = delete;
  WebFeedTabHelper& operator=(const WebFeedTabHelper&) = delete;

  ~WebFeedTabHelper() override;

  // Updates the web feed info stored in this per-tab class instance, as the
  // result of successful following or unfollowing. The update will be skipped
  // if the contents have navigated to a different URL (the passing URL does
  // not match the URL stored in this object).
  void UpdateWebFeedInfo(const GURL& url,
                         TabWebFeedFollowState follow_state,
                         const std::string& web_feed_id);

  // For testing purpose.
  void SetWebFeedInfoForTesting(const GURL& url,
                                TabWebFeedFollowState follow_state,
                                const std::string& web_feed_id);
  void SetWebFeedInfoFinderForTesting(
      std::unique_ptr<WebFeedInfoFinder> finder);

  GURL url() const { return url_; }
  TabWebFeedFollowState follow_state() const { return follow_state_; }
  std::string web_feed_id() const { return web_feed_id_; }

 private:
  friend class content::WebContentsUserData<WebFeedTabHelper>;

  explicit WebFeedTabHelper(content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  void OnWebFeedInfoRetrieved(const GURL& url, WebFeedMetadata metadata);

  GURL url_;
  TabWebFeedFollowState follow_state_ = TabWebFeedFollowState::kUnknown;
  std::string web_feed_id_;

  std::unique_ptr<WebFeedInfoFinder> web_feed_info_finder_;

  base::WeakPtrFactory<WebFeedTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_WEB_FEED_TAB_HELPER_H_
