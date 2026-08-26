// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_navigation_download_policy.h"

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"

namespace subresource_filter {

void AugmentNavigationDownloadPolicy(
    content::RenderFrameHost* frame_host,
    bool user_gesture,
    blink::NavigationDownloadPolicy* download_policy) {
  const auto* throttle_manager =
      ContentSubresourceFilterThrottleManager::FromPage(frame_host->GetPage());
  if (!throttle_manager) {
    return;
  }

  if (!throttle_manager->IsRenderFrameHostTaggedAsAd(frame_host)) {
    return;
  }

  download_policy->SetAllowed(blink::NavigationDownloadType::kAdFrame);
  if (!user_gesture) {
    download_policy->SetDisallowed(
        blink::NavigationDownloadType::kAdFrameNoGesture);
  }
}

}  // namespace subresource_filter
