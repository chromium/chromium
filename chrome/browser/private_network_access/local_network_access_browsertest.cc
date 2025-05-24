// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"

// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

class LocalNetworkAccessBrowserTest : public policy::PolicyTest {
 public:
  LocalNetworkAccessBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_server_.Start());
    EXPECT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) final {
    // Ignore cert errors when connecting to https_server()
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckSecurityStateDefaultPolicy) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  // LNA fetch should pass (default is currently in warning mode).
  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kLnaPath))));
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessBrowserTest,
                       CheckSecurityStatePolicySet) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kLocalNetworkAccessRestrictionsEnabled,
            std::optional<base::Value>(true));
  UpdateProviderPolicy(policies);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  // Enable auto-denial of LNA permission request.
  bubble_factory->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Expect LNA fetch to fail.
  EXPECT_THAT(content::EvalJs(
                  web_contents(),
                  content::JsReplace("fetch($1).then(response => response.ok)",
                                     https_server().GetURL("b.com", kLnaPath))),
              content::EvalJsResult::IsError());
}
