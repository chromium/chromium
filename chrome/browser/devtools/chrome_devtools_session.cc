// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session.h"

#include <memory>
#include <type_traits>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/devtools/protocol/ads_handler.h"
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
#include "chrome/browser/devtools/protocol/webmcp_handler.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/devtools/protocol/window_manager_handler.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#endif

namespace {

template <typename Handler>
bool IsDomainAvailableToUntrustedClient() {
  return std::disjunction_v<std::is_same<Handler, PageHandler>,
                            std::is_same<Handler, EmulationHandler>,
                            std::is_same<Handler, TargetHandler>,
                            std::is_same<Handler, WebMCPHandler>>;
}

}  // namespace

ChromeDevToolsSession::ChromeDevToolsSession(
    content::DevToolsAgentHostClientChannel* channel)
    : ChromeDevToolsSessionBase(channel) {
  content::DevToolsAgentHost* agent_host = channel->GetAgentHost();
  if (agent_host->GetWebContents() &&
      (agent_host->GetType() == content::DevToolsAgentHost::kTypePage ||
       agent_host->GetType() == ChromeDevToolsManagerDelegate::kTypeApp)) {
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
    if (IsDomainAvailableToUntrustedClient<SecurityHandler>() ||
        channel->GetClient()->IsTrusted()) {
      security_handler_ = std::make_unique<SecurityHandler>(
          agent_host->GetWebContents(), dispatcher());
    }
    if (IsDomainAvailableToUntrustedClient<CastHandler>() ||
        channel->GetClient()->IsTrusted()) {
      cast_handler_ = std::make_unique<CastHandler>(
          agent_host->GetWebContents(), dispatcher());
    }
    if (IsDomainAvailableToUntrustedClient<StorageHandler>() ||
        channel->GetClient()->IsTrusted()) {
      storage_handler_ = std::make_unique<StorageHandler>(
          agent_host->GetWebContents(), dispatcher());
    }
  }
  if (agent_host->GetWebContents() &&
      (agent_host->GetType() == content::DevToolsAgentHost::kTypePage ||
       agent_host->GetType() == content::DevToolsAgentHost::kTypeFrame)) {
    if (IsDomainAvailableToUntrustedClient<AutofillHandler>() ||
        channel->GetClient()->IsTrusted()) {
      autofill_handler_ =
          std::make_unique<AutofillHandler>(dispatcher(), agent_host->GetId());
    }
    if (IsDomainAvailableToUntrustedClient<WebMCPHandler>() ||
        channel->GetClient()->IsTrusted()) {
      webmcp_handler_ = std::make_unique<WebMCPHandler>(
          dispatcher(), agent_host->GetWebContents());
    }
  }
  if (IsDomainAvailableToUntrustedClient<ExtensionsHandler>() ||
      channel->GetClient()->IsTrusted()) {
    extensions_handler_ = std::make_unique<ExtensionsHandler>(
        dispatcher(), agent_host->GetId(),
        agent_host->GetType() == content::DevToolsAgentHost::kTypeBrowser);
  }
  if (IsDomainAvailableToUntrustedClient<EmulationHandler>() ||
      channel->GetClient()->IsTrusted()) {
    emulation_handler_ =
        std::make_unique<EmulationHandler>(agent_host, dispatcher());
  }
  if (IsDomainAvailableToUntrustedClient<TargetHandler>() ||
      channel->GetClient()->IsTrusted()) {
    target_handler_ = std::make_unique<TargetHandler>(
        dispatcher(), channel->GetClient()->IsTrusted(),
        channel->GetClient()->MayReadLocalFiles());
  }
  if (IsDomainAvailableToUntrustedClient<BrowserHandler>() ||
      channel->GetClient()->IsTrusted()) {
    browser_handler_ =
        std::make_unique<BrowserHandler>(dispatcher(), agent_host->GetId());
  }
  if (IsDomainAvailableToUntrustedClient<SystemInfoHandler>() ||
      channel->GetClient()->IsTrusted()) {
    system_info_handler_ = std::make_unique<SystemInfoHandler>(dispatcher());
  }

  if ((agent_host->GetType() == content::DevToolsAgentHost::kTypeBrowser ||
       agent_host->GetType() == content::DevToolsAgentHost::kTypePage) &&
      (channel->GetClient()->AllowUnsafeOperations()
#if BUILDFLAG(IS_CHROMEOS)
       // Also enable on ChromeOS in dev mode.
       || (base::CommandLine::ForCurrentProcess()->HasSwitch(
               chromeos::switches::kSystemDevMode) &&
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kEnableDevToolsPwaHandler))
#endif
           )) {
    if (IsDomainAvailableToUntrustedClient<PWAHandler>() ||
        channel->GetClient()->IsTrusted()) {
      pwa_handler_ =
          std::make_unique<PWAHandler>(dispatcher(), agent_host->GetId());
    }
  }
#if BUILDFLAG(IS_CHROMEOS)
  window_manager_handler_ =
      std::make_unique<WindowManagerHandler>(dispatcher());
#endif
}

ChromeDevToolsSession::~ChromeDevToolsSession() = default;
