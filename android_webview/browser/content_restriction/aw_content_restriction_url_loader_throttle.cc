// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_url_loader_throttle.h"

#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
#include "base/functional/bind.h"
#include "net/base/net_errors.h"

namespace android_webview {

AwContentRestrictionURLLoaderThrottle::AwContentRestrictionURLLoaderThrottle(
    AwContentRestrictionManagerClient* client)
    : content_restriction_manager_client_(client) {}

AwContentRestrictionURLLoaderThrottle::
    ~AwContentRestrictionURLLoaderThrottle() = default;

void AwContentRestrictionURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK(content_restriction_manager_client_);
  if (content_restriction_manager_client_->IsContentRestrictionEnabled()) {
    *defer = true;
    content_restriction_manager_client_->RequestContentClassification(
        *request,
        base::BindOnce(
            &AwContentRestrictionURLLoaderThrottle::OnClassificationResult,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AwContentRestrictionURLLoaderThrottle::OnClassificationResult(
    bool is_allowed) {
  DCHECK(delegate_);
  if (is_allowed) {
    delegate_->Resume();
    return;
  }

  // TODO(crbug.com/481113131): Integrate error page handler.
  delegate_->CancelWithError(net::ERR_BLOCKED_BY_CLIENT);
}

}  // namespace android_webview
