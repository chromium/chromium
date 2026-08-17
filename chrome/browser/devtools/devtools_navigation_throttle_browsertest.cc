// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_throttle.h"

#include "base/command_line.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class TestDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  TestDevToolsAgentHostClient() = default;
  ~TestDevToolsAgentHostClient() override = default;

  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
};

class DevToolsNavigationThrottleBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

class DevToolsNavigationThrottleBlocklistBrowserTest
    : public DevToolsNavigationThrottleBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsNavigationThrottleBrowserTest::SetUpCommandLine(command_line);
    // Block "http://blocked.com" (wildcard port, matches any port).
    command_line->AppendSwitchASCII(switches::kDevToolsNavigationGatingRules,
                                    R"({"blocklist": ["http://blocked.com"]})");
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsNavigationThrottleBlocklistBrowserTest,
                       ProceedsWithoutCDPAttached) {
  GURL blocked_url =
      embedded_test_server()->GetURL("blocked.com", "/empty.html");

  // Even though it is on the blocklist, there is no DevTools agent attached.
  // The navigation must proceed.
  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), blocked_url));
}

IN_PROC_BROWSER_TEST_F(DevToolsNavigationThrottleBlocklistBrowserTest,
                       BlockedUrlBlockedWhenBlocklistSet) {
  GURL blocked_url =
      embedded_test_server()->GetURL("blocked.com", "/empty.html");

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(GetWebContents());
  TestDevToolsAgentHostClient client;
  agent_host->AttachClient(&client);

  EXPECT_FALSE(content::NavigateToURL(GetWebContents(), blocked_url));

  agent_host->DetachClient(&client);
}

class DevToolsNavigationThrottleAllowlistBrowserTest
    : public DevToolsNavigationThrottleBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsNavigationThrottleBrowserTest::SetUpCommandLine(command_line);
    // Allow "http://allowed.com" (wildcard port, matches any port).
    command_line->AppendSwitchASCII(switches::kDevToolsNavigationGatingRules,
                                    R"({"allowlist": ["http://allowed.com"]})");
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsNavigationThrottleAllowlistBrowserTest,
                       AllowedUrlProceedsWithCDPAttached) {
  GURL allowed_url =
      embedded_test_server()->GetURL("allowed.com", "/empty.html");

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(GetWebContents());
  TestDevToolsAgentHostClient client;
  agent_host->AttachClient(&client);

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), allowed_url));

  agent_host->DetachClient(&client);
}

IN_PROC_BROWSER_TEST_F(DevToolsNavigationThrottleAllowlistBrowserTest,
                       UnlistedUrlBlockedWhenAllowlistSet) {
  GURL unlisted_url =
      embedded_test_server()->GetURL("unlisted.com", "/empty.html");

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(GetWebContents());
  TestDevToolsAgentHostClient client;
  agent_host->AttachClient(&client);

  // The allowlist only contains "http://allowed.com". "http://unlisted.com"
  // must be blocked.
  EXPECT_FALSE(content::NavigateToURL(GetWebContents(), unlisted_url));

  agent_host->DetachClient(&client);
}

}  // namespace
