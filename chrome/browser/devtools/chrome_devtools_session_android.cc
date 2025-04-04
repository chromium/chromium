// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session_android.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "chrome/browser/devtools/protocol/browser_handler_android.h"
#include "chrome/browser/devtools/protocol/target_handler_android.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"

namespace {

template <typename Handler>
bool IsDomainAvailableToUntrustedClient() {
  return std::disjunction_v<std::is_same<Handler, TargetHandlerAndroid>>;
}

}  // namespace

ChromeDevToolsSessionAndroid::ChromeDevToolsSessionAndroid(
    content::DevToolsAgentHostClientChannel* channel)
    : dispatcher_(this), client_channel_(channel) {
  content::DevToolsAgentHost* agent_host = channel->GetAgentHost();
  if (IsDomainAvailableToUntrustedClient<BrowserHandlerAndroid>() ||
      channel->GetClient()->IsTrusted()) {
    browser_handler_ = std::make_unique<BrowserHandlerAndroid>(
        &dispatcher_, agent_host->GetId());
  }
  if (IsDomainAvailableToUntrustedClient<TargetHandlerAndroid>() ||
      channel->GetClient()->IsTrusted()) {
    target_handler_ = std::make_unique<TargetHandlerAndroid>(
        &dispatcher_, channel->GetClient()->IsTrusted(),
        channel->GetClient()->MayReadLocalFiles());
  }
}

ChromeDevToolsSessionAndroid::~ChromeDevToolsSessionAndroid() = default;

base::HistogramBase::Sample32 GetCommandUmaId(std::string_view command_name) {
  return static_cast<base::HistogramBase::Sample32>(
      base::HashMetricName(command_name));
}

void ChromeDevToolsSessionAndroid::HandleCommand(
    base::span<const uint8_t> message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  crdtp::Dispatchable dispatchable(crdtp::SpanFrom(message));
  DCHECK(dispatchable.ok());  // Checked by content::DevToolsSession.
  crdtp::UberDispatcher::DispatchResult dispatched =
      dispatcher_.Dispatch(dispatchable);

  auto command_uma_id = GetCommandUmaId(std::string_view(
      reinterpret_cast<const char*>(dispatchable.Method().data()),
      dispatchable.Method().size()));
  std::string client_type = client_channel_->GetClient()->GetTypeForMetrics();
  DCHECK(client_type == "DevTools" || client_type == "Extension" ||
         client_type == "RemoteDebugger" || client_type == "Other");
  base::UmaHistogramSparse("DevTools.CDPCommandFrom" + client_type,
                           command_uma_id);

  if (!dispatched.MethodFound()) {
    std::move(callback).Run(message);
    return;
  }
  pending_commands_[dispatchable.CallId()] = std::move(callback);
  dispatched.Run();
}

// The following methods handle responses or notifications coming from
// the browser to the client.
void ChromeDevToolsSessionAndroid::SendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  pending_commands_.erase(call_id);
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ChromeDevToolsSessionAndroid::SendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  client_channel_->DispatchProtocolMessageToClient(message->Serialize());
}

void ChromeDevToolsSessionAndroid::FlushProtocolNotifications() {}

void ChromeDevToolsSessionAndroid::FallThrough(int call_id,
                                               crdtp::span<uint8_t> method,
                                               crdtp::span<uint8_t> message) {
  auto callback = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(callback).Run(message);
}
