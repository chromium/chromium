// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_
#define ANDROID_WEBVIEW_RENDERER_AW_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"

namespace android_webview {

// This must be constructed on the render thread, and then used and destructed
// on a single thread, which can be different from the render thread.
class AwWebSocketHandshakeThrottleProvider final
    : public blink::WebSocketHandshakeThrottleProvider {
 public:
  explicit AwWebSocketHandshakeThrottleProvider(
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);

  AwWebSocketHandshakeThrottleProvider& operator=(
      const AwWebSocketHandshakeThrottleProvider&) = delete;

  ~AwWebSocketHandshakeThrottleProvider() override;

  // Implements blink::WebSocketHandshakeThrottleProvider.
  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> Clone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle> CreateThrottle(
      int render_frame_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  AwWebSocketHandshakeThrottleProvider(
      const AwWebSocketHandshakeThrottleProvider& other);

  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing> safe_browsing_remote_;
  mojo::Remote<safe_browsing::mojom::SafeBrowsing> safe_browsing_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_
