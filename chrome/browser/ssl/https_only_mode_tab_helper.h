// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_HELPER_H_
#define CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_HELPER_H_

#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// A short-lived, per-tab helper for tracking HTTPS-Only Mode data about the
// navigation and for creating the blocking page for the early-timeout code
// path.
class HttpsOnlyModeTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HttpsOnlyModeTabHelper> {
 public:
  HttpsOnlyModeTabHelper(const HttpsOnlyModeTabHelper&) = delete;
  HttpsOnlyModeTabHelper& operator=(const HttpsOnlyModeTabHelper&) = delete;
  ~HttpsOnlyModeTabHelper() override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  // HTTPS-Only Mode metadata getters and setters:
  void set_is_navigation_upgraded(bool upgraded) {
    is_navigation_upgraded_ = upgraded;
  }
  bool is_navigation_upgraded() const { return is_navigation_upgraded_; }

  void set_is_navigation_fallback(bool fallback) {
    is_navigation_fallback_ = fallback;
  }
  bool is_navigation_fallback() const { return is_navigation_fallback_; }

  void set_is_timer_interstitial(bool fallback) {
    is_timer_interstitial_ = fallback;
  }
  bool is_timer_interstitial() const { return is_timer_interstitial_; }

  void set_fallback_url(const GURL& fallback_url) {
    fallback_url_ = fallback_url;
  }
  GURL fallback_url() const { return fallback_url_; }

 private:
  explicit HttpsOnlyModeTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HttpsOnlyModeTabHelper>;

  std::unique_ptr<ChromeSecurityBlockingPageFactory> factory_;

  // TODO(crbug.com/1218526): Track upgrade status per-navigation rather than
  // per-WebContents, in case multiple navigations occur in the WebContents and
  // the metadata is not cleared. This may be tricky however as the Interceptor
  // and the Throttle have slightly different views of the navigation -- the
  // Throttle has a NavigationHandle (and thus the Navigation ID) but the
  // Interceptor has the NavigationEntry's ID which does not match.
  bool is_navigation_upgraded_ = false;

  // Set to true if the current navigation is a fallback to HTTP.
  bool is_navigation_fallback_ = false;

  // Set to true if an interstitial triggered due to an HTTPS timeout is about
  // to be shown.
  bool is_timer_interstitial_ = false;

  // HTTP URL that the current navigation should fall back to on failure.
  GURL fallback_url_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_HELPER_H_
