// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_HELPER_H_
#define CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_HELPER_H_

#include "base/containers/contains.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

// A short-lived, per-tab helper for tracking HTTPS-Only Mode data about the
// navigation.
class HttpsOnlyModeTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HttpsOnlyModeTabHelper> {
 public:
  HttpsOnlyModeTabHelper(const HttpsOnlyModeTabHelper&) = delete;
  HttpsOnlyModeTabHelper& operator=(const HttpsOnlyModeTabHelper&) = delete;
  ~HttpsOnlyModeTabHelper() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
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

  void set_fallback_url(const GURL& fallback_url) {
    fallback_url_ = fallback_url;
  }
  GURL fallback_url() const { return fallback_url_; }

  bool has_failed_upgrade(const GURL& url) const {
    return base::Contains(failed_upgrade_urls_, url);
  }
  void add_failed_upgrade(const GURL& url) { failed_upgrade_urls_.insert(url); }

  void set_is_exempt_error(bool is_exempt_error) {
    is_exempt_error_ = is_exempt_error;
  }
  bool is_exempt_error() const { return is_exempt_error_; }

 private:
  explicit HttpsOnlyModeTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HttpsOnlyModeTabHelper>;

  // TODO(crbug.com/40771668): Track upgrade status per-navigation rather than
  // per-WebContents, in case multiple navigations occur in the WebContents and
  // the metadata is not cleared. This may be tricky however as the Interceptor
  // and the Throttle have slightly different views of the navigation -- the
  // Throttle has a NavigationHandle (and thus the Navigation ID) but the
  // Interceptor has the NavigationEntry's ID which does not match.
  bool is_navigation_upgraded_ = false;

  // Set to true if the current navigation is a fallback to HTTP.
  bool is_navigation_fallback_ = false;

  // HTTP URL that the current navigation should fall back to on failure.
  GURL fallback_url_;

  // Holds the set of URLs that have failed to be upgraded to HTTPS in this
  // WebContents. This is used to immediately show the HTTP interstitial without
  // re-trying to upgrade the navigation -- currently this is only applied to
  // back/forward navigations as they interact badly with interceptors, and this
  // acts as the browser "remembering" the navigation state.
  //
  // In the case of HTTPS Upgrades, without HTTPS-First Mode enabled, these
  // hostnames will also be on the HTTP allowlist, bypassing upgrade attempts.
  std::set<GURL> failed_upgrade_urls_;

  // Set to true if the current navigation resulted in a net error that is
  // indicative of potentially-transient network conditions (such as a hostname
  // resolution failure, the network being disconnected, or an address being
  // unreachable) which don't signal that the server doesn't support HTTPS. This
  // is used to track whether to maintain upgrade state across reloads (such as
  // the automatic net error reload) and continue the upgrade attempt
  // post-reload.
  bool is_exempt_error_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_TAB_HELPER_H_
