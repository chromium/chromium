// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session.h"

#include <memory>
#include <string_view>
#include <type_traits>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/devtools/protocol/autofill_handler.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/devtools/protocol/cast_handler.h"
#include "chrome/browser/devtools/protocol/emulation_handler.h"
#include "chrome/browser/devtools/protocol/extensions_handler.h"
#include "chrome/browser/devtools/protocol/page_handler.h"
#include "chrome/browser/devtools/protocol/pwa_handler.h"
#include "chrome/browser/devtools/protocol/security_handler.h"
#include "chrome/browser/devtools/protocol/storage_handler.h"
#include "chrome/browser/devtools/protocol/system_info_handler.h"
#include "chrome/browser/devtools/protocol/target_handler.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/devtools/protocol/window_manager_handler.h"
#endif

namespace {

template <typename Handler>
bool IsDomainAvailableToUntrustedClient() {
  return std::disjunction_v<std::is_same<Handler, PageHandler>,
                            std::is_same<Handler, EmulationHandler>,
                            std::is_same<Handler, TargetHandler>>;
}

}  // namespace

ChromeDevToolsSession::ChromeDevToolsSession(
    content::DevToolsAgentHostClientChannel* channel)
    : dispatcher_(this), client_channel_(channel) {
  content::DevToolsAgentHost* agent_host = channel->GetAgentHost();
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    if (IsDomainAvailableToUntrustedClient<PageHandler>() ||
        channel->GetClient()->IsTrusted()) {
      page_handler_ = std::make_unique<PageHandler>(
          agent_host, agent_host->GetWebContents(), &dispatcher_);
    }
    if (IsDomainAvailableToUntrustedClient<SecurityHandler>() ||
        channel->GetClient()->IsTrusted()) {
      security_handler_ = std::make_unique<SecurityHandler>(
          agent_host->GetWebContents(), &dispatcher_);
    }
    if (IsDomainAvailableToUntrustedClient<CastHandler>() ||
        channel->GetClient()->IsTrusted()) {
      cast_handler_ = std::make_unique<CastHandler>(
          agent_host->GetWebContents(), &dispatcher_);
    }
    if (IsDomainAvailableToUntrustedClient<StorageHandler>() ||
        channel->GetClient()->IsTrusted()) {
      storage_handler_ = std::make_unique<StorageHandler>(
          agent_host->GetWebContents(), &dispatcher_);
    }
  }
  if (agent_host->GetWebContents() &&
      (agent_host->GetType() == content::DevToolsAgentHost::kTypePage ||
       agent_host->GetType() == content::DevToolsAgentHost::kTypeFrame)) {
    if (IsDomainAvailableToUntrustedClient<AutofillHandler>() ||
        channel->GetClient()->IsTrusted()) {
      autofill_handler_ =
          std::make_unique<AutofillHandler>(&dispatcher_, agent_host->GetId());
    }
  }
  if (IsDomainAvailableToUntrustedClient<ExtensionsHandler>() ||
      channel->GetClient()->IsTrusted()) {
    extensions_handler_ = std::make_unique<ExtensionsHandler>(
        &dispatcher_, agent_host->GetId(),
        channel->GetClient()->AllowUnsafeOperations() &&
            base::CommandLine::ForCurrentProcess()->HasSwitch(
                ::switches::kEnableUnsafeExtensionDebugging) &&
            agent_host->GetType() == content::DevToolsAgentHost::kTypeBrowser);
  }
  if (IsDomainAvailableToUntrustedClient<EmulationHandler>() ||
      channel->GetClient()->IsTrusted()) {
    emulation_handler_ =
        std::make_unique<EmulationHandler>(agent_host, &dispatcher_);
  }
  if (IsDomainAvailableToUntrustedClient<TargetHandler>() ||
      channel->GetClient()->IsTrusted()) {
    target_handler_ = std::make_unique<TargetHandler>(
        &dispatcher_, channel->GetClient()->IsTrusted());
  }
  if (IsDomainAvailableToUntrustedClient<BrowserHandler>() ||
      channel->GetClient()->IsTrusted()) {
    browser_handler_ =
        std::make_unique<BrowserHandler>(&dispatcher_, agent_host->GetId());
  }
  if (IsDomainAvailableToUntrustedClient<SystemInfoHandler>() ||
      channel->GetClient()->IsTrusted()) {
    system_info_handler_ = std::make_unique<SystemInfoHandler>(&dispatcher_);
  }
  if ((agent_host->GetType() == content::DevToolsAgentHost::kTypeBrowser ||
       agent_host->GetType() == content::DevToolsAgentHost::kTypePage) &&
      channel->GetClient()->AllowUnsafeOperations()) {
    if (IsDomainAvailableToUntrustedClient<PWAHandler>() ||
        channel->GetClient()->IsTrusted()) {
      pwa_handler_ =
          std::make_unique<PWAHandler>(&dispatcher_, agent_host->GetId());
    }
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  window_manager_handler_ =
      std::make_unique<WindowManagerHandler>(&dispatcher_);
#endif
}

ChromeDevToolsSession::~ChromeDevToolsSession() = default;

base::HistogramBase::Sample GetCommandUmaId(std::string_view command_name) {
  return static_cast<base::HistogramBase::Sample>(
      base::HashMetricName(command_name));
}

void ChromeDevToolsSession::HandleCommand(
    base::span<const uint8_t> message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  crdtp::Dispatchable dispatchable(crdtp::SpanFrom(message));
  DCHECK(dispatchable.ok());  // Checked by content::DevToolsSession.
  crdtp::UberDispatcher::DispatchResult dispatched =
      dispatcher_.Dispatch(dispatchable);

  auto command_uma_id = GetCommandUmaId(std::string_view(
      reinterpret_cast<const char*>(dispatchable.Method().begin()),
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
