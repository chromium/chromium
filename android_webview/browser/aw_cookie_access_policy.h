// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_COOKIE_ACCESS_POLICY_H_
#define ANDROID_WEBVIEW_BROWSER_AW_COOKIE_ACCESS_POLICY_H_

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/types/optional_ref.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "net/base/network_delegate.h"
#include "net/storage_access_api/status.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class GURL;

namespace net {
class SiteForCookies;
}  // namespace net

namespace android_webview {

// Manages the cookie access (both setting and getting) policy for WebView.
// Currently we don't distinguish between sources (i.e. network vs. JavaScript)
// or between reading vs. writing cookies.
class AwCookieAccessPolicy {
 public:
  AwCookieAccessPolicy();
  ~AwCookieAccessPolicy();

  AwCookieAccessPolicy(const AwCookieAccessPolicy&) = delete;
  AwCookieAccessPolicy& operator=(const AwCookieAccessPolicy&) = delete;

  // Can we read/write any cookies? Can be called from any thread.
  bool GetShouldAcceptCookies();
  void SetShouldAcceptCookies(bool allow);

  // Can we read/write third party cookies?
  // `frame_tree_node_id` or `global_frame_token` must be valid.
  // Navigation requests are not associated with a renderer process. In this
  // case, `frame_tree_node_id` must be valid instead. Can only be called from
  // the IO thread.
  bool GetShouldAcceptThirdPartyCookies(
      base::optional_ref<const content::GlobalRenderFrameHostToken>
          global_frame_token,
      content::FrameTreeNodeId frame_tree_node_id);

  // Whether or not to allow cookies for requests with these parameters.
  net::NetworkDelegate::PrivacySetting AllowCookies(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      base::optional_ref<const content::GlobalRenderFrameHostToken>
          global_frame_token,
      net::StorageAccessApiStatus storage_access_api_status);

  net::NetworkDelegate::PrivacySetting CanAccessCookies(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      bool accept_third_party_cookies,
      net::StorageAccessApiStatus storage_access_api_status);

 private:
  friend class AwCookieAccessPolicyTest;

  bool accept_cookies_ = true;
  base::Lock lock_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_COOKIE_ACCESS_POLICY_H_
