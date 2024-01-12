// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_URL_LOADER_THROTTLE_PROVIDER_H_
#define ANDROID_WEBVIEW_RENDERER_AW_URL_LOADER_THROTTLE_PROVIDER_H_

#include "base/sequence_checker.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

namespace android_webview {

// Instances must be constructed on the render main thread, and then used and
// destructed on a single sequence, which can be different from the render main
// thread.
class AwURLLoaderThrottleProvider : public blink::URLLoaderThrottleProvider {
 public:
  AwURLLoaderThrottleProvider(
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker,
      blink::URLLoaderThrottleProviderType type);

  AwURLLoaderThrottleProvider& operator=(const AwURLLoaderThrottleProvider&) =
      delete;

  ~AwURLLoaderThrottleProvider() override;

  // blink::URLLoaderThrottleProvider implementation.
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      const network::ResourceRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  AwURLLoaderThrottleProvider(const AwURLLoaderThrottleProvider& other);

  blink::URLLoaderThrottleProviderType type_;

  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing> safe_browsing_remote_;
  mojo::Remote<safe_browsing::mojom::SafeBrowsing> safe_browsing_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_URL_LOADER_THROTTLE_PROVIDER_H_
