// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_NAVIGATION_DOWNLOAD_POLICY_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_NAVIGATION_DOWNLOAD_POLICY_H_

namespace blink {
struct NavigationDownloadPolicy;
}  // namespace blink

namespace content {
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

// Possibly augments `download_policy` based on the status of `frame_host` as
// well as `user_gesture`.
void AugmentNavigationDownloadPolicy(
    content::RenderFrameHost* frame_host,
    bool user_gesture,
    blink::NavigationDownloadPolicy* download_policy);

}  // namespace subresource_filter

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_NAVIGATION_DOWNLOAD_POLICY_H_
