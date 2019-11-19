// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_URL_LOADER_THROTTLE_PROVIDER_H_
#define ANDROID_WEBVIEW_RENDERER_AW_URL_LOADER_THROTTLE_PROVIDER_H_

#include "base/threading/thread_checker.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"

namespace android_webview {

// Instances must be constructed on the render thread, and then used and
// destructed on a single thread, which can be different from the render thread.
class AwURLLoaderThrottleProvider : public content::URLLoaderThrottleProvider {
 public:
  AwURLLoaderThrottleProvider(
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker,
      content::URLLoaderThrottleProviderType type);

  ~AwURLLoaderThrottleProvider() override;

  // content::URLLoaderThrottleProvider implementation.
  std::unique_ptr<content::URLLoaderThrottleProvider> Clone() override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request,
      content::ResourceType resource_type) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  AwURLLoaderThrottleProvider(const AwURLLoaderThrottleProvider& other);

  content::URLLoaderThrottleProviderType type_;

  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing> safe_browsing_remote_;
  mojo::Remote<safe_browsing::mojom::SafeBrowsing> safe_browsing_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_ASSIGN(AwURLLoaderThrottleProvider);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_URL_LOADER_THROTTLE_PROVIDER_H_
