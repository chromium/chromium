// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_throttle.h"

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/devtools/devtools_navigation_gating_rule_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

// static
void DevToolsNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDevToolsNavigationGatingRules)) {
    return;
  }

  if (!DevToolsNavigationGatingRuleManager::Get().MayBlockNavigation()) {
    return;
  }

  content::WebContents* web_contents =
      registry.GetNavigationHandle().GetWebContents();
  if (!web_contents) {
    return;
  }

  if (!content::DevToolsAgentHost::IsDebuggerAttached(web_contents)) {
    return;
  }

  registry.AddThrottle(std::make_unique<DevToolsNavigationThrottle>(registry));
}

DevToolsNavigationThrottle::DevToolsNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

DevToolsNavigationThrottle::~DevToolsNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
DevToolsNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest();
}

content::NavigationThrottle::ThrottleCheckResult
DevToolsNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest();
}

content::NavigationThrottle::ThrottleCheckResult
DevToolsNavigationThrottle::WillStartOrRedirectRequest() {
  const GURL& request_url = navigation_handle()->GetURL();

  DevToolsNavigationGatingRuleManager::Get().IsNavigationAllowed(
      request_url, base::BindOnce(&DevToolsNavigationThrottle::OnGatingDecision,
                                  weak_ptr_factory_.GetWeakPtr()));

  return content::NavigationThrottle::DEFER;
}

void DevToolsNavigationThrottle::OnGatingDecision(bool is_allowed) {
  if (is_allowed) {
    Resume();
    return;
  }

  CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT));
}

const char* DevToolsNavigationThrottle::GetNameForLogging() {
  return "DevToolsNavigationThrottle";
}
