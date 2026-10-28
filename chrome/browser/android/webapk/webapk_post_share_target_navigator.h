// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_POST_SHARE_TARGET_NAVIGATOR_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_POST_SHARE_TARGET_NAVIGATOR_H_

#include <string>
#include "services/network/public/cpp/resource_request_body.h"

namespace content {
class WebContents;
}

class GURL;

namespace webapk {

// Navigate to share target gurl with |post_data| and |header_list|.
void NavigateShareTargetPost(
    const scoped_refptr<network::ResourceRequestBody>& post_data,
    const std::string& header_list,
    const GURL& share_target_gurl,
    content::WebContents* web_contents);

// Like NavigateShareTargetPost(), but if no service worker is registered for
// the origin of |share_target_gurl| (e.g. the web app has never been launched,
// so its service worker has never been installed), first loads
// |bootstrap_gurl| (the web app's start URL) so that the page can register its
// service worker, and sends the POST once a service worker controlling
// |share_target_gurl| has been activated. Without this, the POST would go to
// the network and the shared data would be lost. Sends the POST immediately if
// a service worker controls |share_target_gurl|, if the origin has other
// service worker registrations (loading the start URL would not add one for
// the action), or if |bootstrap_gurl| is not a same-origin HTTP(S) URL, and
// sends it anyway if no service worker activates before a timeout.
void NavigateShareTargetPostWhenServiceWorkerReady(
    const scoped_refptr<network::ResourceRequestBody>& post_data,
    const std::string& header_list,
    const GURL& share_target_gurl,
    const GURL& bootstrap_gurl,
    content::WebContents* web_contents);

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_POST_SHARE_TARGET_NAVIGATOR_H_
