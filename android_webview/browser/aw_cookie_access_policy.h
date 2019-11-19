// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_COOKIE_ACCESS_POLICY_H_
#define ANDROID_WEBVIEW_BROWSER_AW_COOKIE_ACCESS_POLICY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

class GURL;

namespace net {
class URLRequest;
}  // namespace net

namespace android_webview {

// Manages the cookie access (both setting and getting) policy for WebView.
// Currently we don't distinguish between sources (i.e. network vs. JavaScript)
// or between reading vs. writing cookies.
class AwCookieAccessPolicy {
 public:
  static AwCookieAccessPolicy* GetInstance();

  // Can we read/write any cookies? Can be called from any thread.
  bool GetShouldAcceptCookies();
  void SetShouldAcceptCookies(bool allow);

  // Can we read/write third party cookies?
  // |render_process_id| and |render_frame_id| must be valid.
  // Navigation requests are not associated with a renderer process. In this
  // case, |frame_tree_node_id| must be valid instead. Can only be called from
  // the IO thread.
  bool GetShouldAcceptThirdPartyCookies(int render_process_id,
                                        int render_frame_id,
                                        int frame_tree_node_id);
  bool GetShouldAcceptThirdPartyCookies(const net::URLRequest& request);

  // Whether or not to allow cookies for requests with these parameters.
  bool AllowCookies(const GURL& url,
                    const GURL& first_party,
                    int render_process_id,
                    int render_frame_id);

 private:
  friend class base::NoDestructor<AwCookieAccessPolicy>;
  friend class AwCookieAccessPolicyTest;

  AwCookieAccessPolicy();
  ~AwCookieAccessPolicy();

  bool CanAccessCookies(const GURL& url,
                        const GURL& site_for_cookies,
                        bool accept_third_party_cookies);
  bool accept_cookies_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AwCookieAccessPolicy);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_COOKIE_ACCESS_POLICY_H_
