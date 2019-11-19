// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/devtools/protocol/forward.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class DevToolsAgentHost;
class DevToolsAgentHostClient;
}

class BrowserHandler;
class CastHandler;
class PageHandler;
class SecurityHandler;
class TargetHandler;
class WindowManagerHandler;

class ChromeDevToolsSession : public protocol::FrontendChannel {
 public:
  ChromeDevToolsSession(content::DevToolsAgentHost* agent_host,
                        content::DevToolsAgentHostClient* client);
  ~ChromeDevToolsSession() override;

  protocol::UberDispatcher* dispatcher() { return dispatcher_.get(); }

  void HandleCommand(
      const std::string& method,
      const std::string& message,
      content::DevToolsManagerDelegate::NotHandledCallback callback);

  TargetHandler* target_handler() { return target_handler_.get(); }

 private:
  // protocol::FrontendChannel:
  void sendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void flushProtocolNotifications() override;
  void fallThrough(int call_id,
                   const std::string& method,
                   const std::string& message) override;

  content::DevToolsAgentHost* const agent_host_;
  content::DevToolsAgentHostClient* const client_;
  base::flat_map<int, content::DevToolsManagerDelegate::NotHandledCallback>
      pending_commands_;

  std::unique_ptr<protocol::UberDispatcher> dispatcher_;
  std::unique_ptr<BrowserHandler> browser_handler_;
  std::unique_ptr<CastHandler> cast_handler_;
  std::unique_ptr<PageHandler> page_handler_;
  std::unique_ptr<SecurityHandler> security_handler_;
  std::unique_ptr<TargetHandler> target_handler_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<WindowManagerHandler> window_manager_protocl_handler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeDevToolsSession);
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_
