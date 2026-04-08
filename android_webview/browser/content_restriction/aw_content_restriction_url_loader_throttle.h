// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_URL_LOADER_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_URL_LOADER_THROTTLE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace android_webview {

class AwContentRestrictionManagerClient;
class AwContentRestrictionBlockedNavigationTracker;

// URLLoaderThrottle implementation for enforcing content restriction in
// WebViews.
class AwContentRestrictionURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit AwContentRestrictionURLLoaderThrottle(
      AwContentRestrictionManagerClient* client,
      AwContentRestrictionBlockedNavigationTracker* tracker,
      std::optional<int64_t> navigation_id);
  AwContentRestrictionURLLoaderThrottle(
      const AwContentRestrictionURLLoaderThrottle&) = delete;
  AwContentRestrictionURLLoaderThrottle& operator=(
      const AwContentRestrictionURLLoaderThrottle&) = delete;
  ~AwContentRestrictionURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

 private:
  // Internal callback helper used to handle the content classification result.
  void OnClassificationResult(bool is_allowed);

  raw_ptr<AwContentRestrictionManagerClient>
      content_restriction_manager_client_;
  raw_ptr<AwContentRestrictionBlockedNavigationTracker> tracker_;
  const std::optional<int64_t> navigation_id_;

  base::WeakPtrFactory<AwContentRestrictionURLLoaderThrottle> weak_ptr_factory_{
      this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_URL_LOADER_THROTTLE_H_
