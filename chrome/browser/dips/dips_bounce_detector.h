// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace site_engagement {
class SiteEngagementService;
}

class DIPSBounceDetector
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DIPSBounceDetector> {
 public:
  ~DIPSBounceDetector() override;
  DIPSBounceDetector(const DIPSBounceDetector&) = delete;
  DIPSBounceDetector& operator=(const DIPSBounceDetector&) = delete;

  using ServerRedirectHandler = base::RepeatingCallback<
      void(const GURL&, content::NavigationHandle*, int)>;

  using RedirectHandler =
      base::RepeatingCallback<void(const GURL&, const GURL&, const GURL&)>;

  void SetStatefulServerRedirectHandlerForTesting(
      ServerRedirectHandler handler) {
    stateful_server_redirect_handler_ = handler;
  }

  void SetStatefulRedirectHandlerForTesting(RedirectHandler handler) {
    stateful_redirect_handler_ = handler;
  }

 private:
  explicit DIPSBounceDetector(content::WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<DIPSBounceDetector>;

  // Called when any stateful redirect is detected.
  //
  // `prev_url` is the page previously committed before starting the chain of
  // redirects. `url` is the redirect URL itself (possibly one of many).
  // `next_url` is the final URL after the chain of redirects completes.
  void HandleStatefulRedirect(const GURL& prev_url,
                              const GURL& url,
                              const GURL& next_url);
  void HandleStatefulServerRedirect(
      const GURL& prev_url,
      content::NavigationHandle* navigation_handle,
      int redirect_index);

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  raw_ptr<site_engagement::SiteEngagementService> site_engagement_service_;
  // By default, this just calls this->HandleStatefulServerRedirect(), but it
  // can be overridden for tests.
  ServerRedirectHandler stateful_server_redirect_handler_;
  // By default, this just calls this->HandleStatefulRedirect(), but it
  // can be overridden for tests.
  RedirectHandler stateful_redirect_handler_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
