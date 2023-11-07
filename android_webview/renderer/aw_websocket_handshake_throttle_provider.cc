// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_websocket_handshake_throttle_provider.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/safe_browsing/content/renderer/websocket_sb_handshake_throttle.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"

namespace android_webview {

AwWebSocketHandshakeThrottleProvider::AwWebSocketHandshakeThrottleProvider(
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  DETACH_FROM_THREAD(thread_checker_);
  broker->GetInterface(safe_browsing_remote_.InitWithNewPipeAndPassReceiver());
}

AwWebSocketHandshakeThrottleProvider::~AwWebSocketHandshakeThrottleProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

AwWebSocketHandshakeThrottleProvider::AwWebSocketHandshakeThrottleProvider(
    const AwWebSocketHandshakeThrottleProvider& other) {
  DETACH_FROM_THREAD(thread_checker_);
  if (other.safe_browsing_) {
    other.safe_browsing_->Clone(
        safe_browsing_remote_.InitWithNewPipeAndPassReceiver());
  }
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
AwWebSocketHandshakeThrottleProvider::Clone(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (safe_browsing_remote_) {
    safe_browsing_.Bind(std::move(safe_browsing_remote_),
                        std::move(task_runner));
  }
  return base::WrapUnique(new AwWebSocketHandshakeThrottleProvider(*this));
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
AwWebSocketHandshakeThrottleProvider::CreateThrottle(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (safe_browsing_remote_) {
    safe_browsing_.Bind(std::move(safe_browsing_remote_),
                        std::move(task_runner));
  }
  return std::make_unique<safe_browsing::WebSocketSBHandshakeThrottle>(
      safe_browsing_.get(), local_frame_token);
}

}  // namespace android_webview
