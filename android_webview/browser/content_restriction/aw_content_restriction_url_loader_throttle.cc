// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_url_loader_throttle.h"

#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
#include "base/functional/bind.h"
#include "net/base/net_errors.h"

namespace android_webview {

AwContentRestrictionURLLoaderThrottle::AwContentRestrictionURLLoaderThrottle(
    AwContentRestrictionManagerClient* client,
    AwContentRestrictionBlockedNavigationTracker* tracker,
    std::optional<int64_t> navigation_id)
    : content_restriction_manager_client_(client),
      tracker_(tracker),
      navigation_id_(navigation_id) {}

AwContentRestrictionURLLoaderThrottle::
    ~AwContentRestrictionURLLoaderThrottle() = default;

void AwContentRestrictionURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK(content_restriction_manager_client_);
  if (navigation_id_.has_value() &&
      content_restriction_manager_client_->IsContentRestrictionEnabled()) {
    *defer = true;

    // TODO(crbug.com/481113476): Also process the request body.
    content_restriction_manager_client_->RequestContentClassification(
        navigation_id_.value(), *request,
        base::BindOnce(
            &AwContentRestrictionURLLoaderThrottle::OnClassificationResult,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AwContentRestrictionURLLoaderThrottle::OnClassificationResult(
    bool is_allowed) {
  DCHECK(delegate_);
  DCHECK(tracker_);
  if (is_allowed) {
    delegate_->Resume();
    return;
  }

  if (navigation_id_.has_value()) {
    tracker_->RegisterNavigationAsBlocked(navigation_id_.value());
  }
  delegate_->CancelWithError(net::ERR_BLOCKED_BY_CLIENT);
}

}  // namespace android_webview
