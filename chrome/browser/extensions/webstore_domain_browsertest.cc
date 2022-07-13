// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

constexpr char kWebstoreDomain[] = "webstore.google.com";
constexpr char kNotWebstoreDomain[] = "noaccess.test";

}  // namespace

class WebstoreDomainBrowserTest : public ExtensionApiTest {
 public:
  WebstoreDomainBrowserTest() {
    feature_list_.InitAndEnableFeature(extensions_features::kNewWebstoreDomain);
    UseHttpsTestServer();
    // Override the test server SLL config with the webstore domain and another
    // domain.
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {kWebstoreDomain, kNotWebstoreDomain};
    embedded_test_server()->SetSSLConfig(cert_config);
  }
  ~WebstoreDomainBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  // While some of the pieces are still being put in place, the webstore API
  // availability is currently behind a feature and channel restriction.
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_override_{version_info::Channel::DEV};
};

// Tests that webstorePrivate and management are exposed to the webstore domain,
// but not to a non-webstore domain.
IN_PROC_BROWSER_TEST_F(WebstoreDomainBrowserTest, ExpectedAvailability) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const GURL webstore_url =
      embedded_test_server()->GetURL(kWebstoreDomain, "/empty.html");
  const GURL not_webstore_url =
      embedded_test_server()->GetURL(kNotWebstoreDomain, "/empty.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto is_api_available = [web_contents](const std::string& api_name) {
    constexpr char kScript[] =
        R"({
             domAutomationController.send(chrome.hasOwnProperty($1));
           })";
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents, content::JsReplace(kScript, api_name), &result));
    return result;
  };

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), webstore_url));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            webstore_url);
  EXPECT_TRUE(is_api_available("webstorePrivate"));
  EXPECT_TRUE(is_api_available("management"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), not_webstore_url));
  EXPECT_EQ(web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
            not_webstore_url);
  EXPECT_FALSE(is_api_available("management"));
  EXPECT_FALSE(is_api_available("webstorePrivate"));
}

}  // namespace extensions
