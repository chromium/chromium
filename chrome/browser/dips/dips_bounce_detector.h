// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/dips/cookie_access_type.h"
#include "chrome/browser/dips/cookie_mode.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace site_engagement {
class SiteEngagementService;
}

class DIPSService;

class DIPSBounceDetector
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DIPSBounceDetector> {
 public:
  ~DIPSBounceDetector() override;
  DIPSBounceDetector(const DIPSBounceDetector&) = delete;
  DIPSBounceDetector& operator=(const DIPSBounceDetector&) = delete;

  using ServerRedirectHandler = base::RepeatingCallback<
      void(const GURL&, content::NavigationHandle*, int, CookieAccessType)>;

  using RedirectHandler = base::RepeatingCallback<
      void(const GURL&, const GURL&, const GURL&, CookieAccessType)>;

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

  DIPSCookieMode GetCookieMode() const;

  // Called when any stateful redirect is detected.
  //
  // `prev_url` is the page previously committed before starting the chain of
  // redirects. `url` is the redirect URL itself (possibly one of many).
  // `next_url` is the final URL after the chain of redirects completes.
  void HandleStatefulRedirect(const GURL& prev_url,
                              const GURL& url,
                              const GURL& next_url,
                              CookieAccessType access);
  void HandleStatefulServerRedirect(
      const GURL& prev_url,
      content::NavigationHandle* navigation_handle,
      int redirect_index,
      CookieAccessType access);

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // raw_ptr<> is safe here DIPSService is a KeyedService, associated with the
  // BrowserContext/Profile which will outlive the WebContents that
  // DIPSBounceDetector is observing.
  raw_ptr<DIPSService> dips_service_;
  // raw_ptr<> is safe here for the same reasons as above.
  raw_ptr<site_engagement::SiteEngagementService> site_engagement_service_;
  // By default, this just calls this->HandleStatefulServerRedirect(), but it
  // can be overridden for tests.
  ServerRedirectHandler stateful_server_redirect_handler_;
  // By default, this just calls this->HandleStatefulRedirect(), but it
  // can be overridden for tests.
  RedirectHandler stateful_redirect_handler_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// RedirectCategory is basically the cross-product of CookieAccessType and a
// boolean value indicating site engagement. It's used in UMA enum histograms.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RedirectCategory {
  kNoCookies_NoEngagement = 0,
  kReadCookies_NoEngagement = 1,
  kWriteCookies_NoEngagement = 2,
  kReadWriteCookies_NoEngagement = 3,
  kNoCookies_HasEngagement = 4,
  kReadCookies_HasEngagement = 5,
  kWriteCookies_HasEngagement = 6,
  kReadWriteCookies_HasEngagement = 7,
  kMaxValue = kReadWriteCookies_HasEngagement,
};

#endif  // CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
