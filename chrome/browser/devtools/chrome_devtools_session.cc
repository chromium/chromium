// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session.h"

#include <memory>
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/devtools/protocol/cast_handler.h"
#include "chrome/browser/devtools/protocol/page_handler.h"
#include "chrome/browser/devtools/protocol/security_handler.h"
#include "chrome/browser/devtools/protocol/target_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/devtools/protocol/window_manager_handler.h"
#endif

ChromeDevToolsSession::ChromeDevToolsSession(
    content::DevToolsAgentHostClientChannel* channel)
    : dispatcher_(this), client_channel_(channel) {
  content::DevToolsAgentHost* agent_host = channel->GetAgentHost();
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    page_handler_ = std::make_unique<PageHandler>(agent_host->GetWebContents(),
                                                  &dispatcher_);
    security_handler_ = std::make_unique<SecurityHandler>(
        agent_host->GetWebContents(), &dispatcher_);
    if (channel->GetClient()->MayAttachToBrowser()) {
      cast_handler_ = std::make_unique<CastHandler>(
          agent_host->GetWebContents(), &dispatcher_);
    }
  }
  target_handler_ = std::make_unique<TargetHandler>(&dispatcher_);
  if (channel->GetClient()->MayAttachToBrowser()) {
    browser_handler_ =
        std::make_unique<BrowserHandler>(&dispatcher_, agent_host->GetId());
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  window_manager_handler_ =
      std::make_unique<WindowManagerHandler>(&dispatcher_);
#endif
}

ChromeDevToolsSession::~ChromeDevToolsSession() = default;

void ChromeDevToolsSession::HandleCommand(
    base::span<const uint8_t> message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  crdtp::Dispatchable dispatchable(crdtp::SpanFrom(message));
  DCHECK(dispatchable.ok());  // Checked by content::DevToolsSession.
  crdtp::UberDispatcher::DispatchResult dispatched =
      dispatcher_.Dispatch(dispatchable);
  if (!dispatched.MethodFound()) {
    std::move(callback).Run(message);
    return;
  }
  pending_commands_[dispatchable.CallId()] = std::move(callback);
  dispatched.Run();
}

// The following methods handle responses or notifications coming from
// the browser to the client.
void ChromeDevToolsSession::SendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  pending_commands_.erase(call_id);
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ChromeDevToolsSession::SendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ChromeDevToolsSession::FlushProtocolNotifications() {}

void ChromeDevToolsSession::FallThrough(int call_id,
                                        crdtp::span<uint8_t> method,
                                        crdtp::span<uint8_t> message) {
  auto callback = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(callback).Run(message);
}
