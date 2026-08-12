// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session_android.h"

#include <type_traits>

#include "chrome/browser/devtools/protocol/autofill_handler.h"
#include "chrome/browser/devtools/protocol/browser_handler_android.h"
#include "chrome/browser/devtools/protocol/target_handler_android.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"

namespace {

template <typename Handler>
bool IsDomainAvailableToUntrustedClient() {
  return std::disjunction_v<std::is_same<Handler, TargetHandlerAndroid>>;
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
  if (IsDomainAvailableToUntrustedClient<BrowserHandlerAndroid>() ||
      channel->GetClient()->IsTrusted()) {
    browser_handler_ = std::make_unique<BrowserHandlerAndroid>(
        dispatcher(), agent_host->GetId());
  }
  if (IsDomainAvailableToUntrustedClient<TargetHandlerAndroid>() ||
      channel->GetClient()->IsTrusted()) {
    target_handler_ = std::make_unique<TargetHandlerAndroid>(
        dispatcher(), channel->GetClient()->IsTrusted(),
        channel->GetClient()->MayReadLocalFiles(), browser_handler_.get());
  }
}

ChromeDevToolsSessionAndroid::~ChromeDevToolsSessionAndroid() = default;
