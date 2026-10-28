// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_url_loader_throttle_provider.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_thread.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"

namespace android_webview {

// TODO(crbug.com/324108312): Remove the unused `broker` parameter.
AwURLLoaderThrottleProvider::AwURLLoaderThrottleProvider(
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker,
    blink::URLLoaderThrottleProviderType type)
    : type_(type) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AwURLLoaderThrottleProvider::AwURLLoaderThrottleProvider(
    const AwURLLoaderThrottleProvider& other)
    : type_(other.type_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
AwURLLoaderThrottleProvider::Clone() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  return base::WrapUnique(new AwURLLoaderThrottleProvider(*this));
}

AwURLLoaderThrottleProvider::~AwURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
AwURLLoaderThrottleProvider::CreateThrottles(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    const network::ResourceRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;

  // Some throttles have already been added in the browser for frame resources.
  // Don't add them for frame requests.
  bool is_frame_resource =
      blink::IsRequestDestinationFrame(request.destination);

  DCHECK(!is_frame_resource ||
         type_ == blink::URLLoaderThrottleProviderType::kFrame);

  if (!is_frame_resource) {
    throttles.emplace_back(
        std::make_unique<safe_browsing::RendererURLLoaderThrottle>());
  }

  return throttles;
}

void AwURLLoaderThrottleProvider::SetOnline(bool is_online) {}

}  // namespace android_webview
