// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session_android.h"

#include <type_traits>

#include "chrome/browser/devtools/protocol/ads_handler.h"
#include "chrome/browser/devtools/protocol/autofill_handler.h"
#include "chrome/browser/devtools/protocol/browser_handler_android.h"
#include "chrome/browser/devtools/protocol/page_handler.h"
#include "chrome/browser/devtools/protocol/target_handler_android.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/devtools/protocol/extensions_handler.h"
#endif

namespace {

template <typename Handler>
bool IsDomainAvailableToUntrustedClient() {
  return std::disjunction_v<std::is_same<Handler, PageHandler>,
                            std::is_same<Handler, TargetHandlerAndroid>>;
}

}  // namespace

ChromeDevToolsSessionAndroid::ChromeDevToolsSessionAndroid(
    content::DevToolsAgentHostClientChannel* channel)
    : ChromeDevToolsSessionBase(channel) {
  content::DevToolsAgentHost* agent_host = channel->GetAgentHost();
  if (agent_host->GetWebContents() &&
      (agent_host->GetType() == content::DevToolsAgentHost::kTypePage ||
       agent_host->GetType() == content::DevToolsAgentHost::kTypeFrame)) {
    if (IsDomainAvailableToUntrustedClient<AutofillHandler>() ||
        channel->GetClient()->IsTrusted()) {
      autofill_handler_ =
          std::make_unique<AutofillHandler>(dispatcher(), agent_host->GetId());
    }
  }
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    if (IsDomainAvailableToUntrustedClient<AdsHandler>() ||
        channel->GetClient()->IsTrusted()) {
      ads_handler_ = std::make_unique<AdsHandler>(
          agent_host->GetWebContents(), dispatcher(),
          channel->GetClient()->IsTrusted());
    }
    if (IsDomainAvailableToUntrustedClient<PageHandler>() ||
        channel->GetClient()->IsTrusted()) {
      page_handler_ = std::make_unique<PageHandler>(
          agent_host, agent_host->GetWebContents(), dispatcher(),
          channel->GetClient()->IsTrusted());
    }
  }
  if (IsDomainAvailableToUntrustedClient<BrowserHandlerAndroid>() ||
      channel->GetClient()->IsTrusted()) {
    browser_handler_ = std::make_unique<BrowserHandlerAndroid>(
        dispatcher(), agent_host->GetId());
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Keep in sync with `ChromeDevToolsSession::ChromeDevToolsSession()`.
  if (IsDomainAvailableToUntrustedClient<ExtensionsHandler>() ||
      channel->GetClient()->IsTrusted()) {
    extensions_handler_ = std::make_unique<ExtensionsHandler>(
        dispatcher(), agent_host->GetId(),
        agent_host->GetType() == content::DevToolsAgentHost::kTypeBrowser);
  }
#endif
  if (IsDomainAvailableToUntrustedClient<TargetHandlerAndroid>() ||
      channel->GetClient()->IsTrusted()) {
    target_handler_ = std::make_unique<TargetHandlerAndroid>(
        dispatcher(), channel->GetClient()->IsTrusted(),
        channel->GetClient()->MayReadLocalFiles(), browser_handler_.get(),
        channel);
  }
}

ChromeDevToolsSessionAndroid::~ChromeDevToolsSessionAndroid() = default;
