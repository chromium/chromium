// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_ANDROID_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_ANDROID_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class DevToolsAgentHostClientChannel;
}  // namespace content

class BrowserHandlerAndroid;
class TargetHandlerAndroid;

class ChromeDevToolsSessionAndroid : public protocol::FrontendChannel {
 public:
  explicit ChromeDevToolsSessionAndroid(
      content::DevToolsAgentHostClientChannel* channel);

  ChromeDevToolsSessionAndroid(const ChromeDevToolsSessionAndroid&) = delete;
  ChromeDevToolsSessionAndroid& operator=(const ChromeDevToolsSessionAndroid&) =
      delete;

  ~ChromeDevToolsSessionAndroid() override;

  void HandleCommand(
      base::span<const uint8_t> message,
      content::DevToolsManagerDelegate::NotHandledCallback callback);

  TargetHandlerAndroid* target_handler() { return target_handler_.get(); }

 private:
  // protocol::FrontendChannel:
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void FlushProtocolNotifications() override;
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override;

  base::flat_map<int, content::DevToolsManagerDelegate::NotHandledCallback>
      pending_commands_;

  protocol::UberDispatcher dispatcher_;
  std::unique_ptr<BrowserHandlerAndroid> browser_handler_;
  std::unique_ptr<TargetHandlerAndroid> target_handler_;
  raw_ptr<content::DevToolsAgentHostClientChannel> client_channel_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_ANDROID_H_
