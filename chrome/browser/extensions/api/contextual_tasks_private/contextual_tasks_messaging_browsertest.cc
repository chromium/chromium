// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"

namespace extensions {

class ContextualTasksExtensionMessagingTest : public ExtensionApiTest {
 public:
  ContextualTasksExtensionMessagingTest() {
    feature_list_.InitWithFeatures(
        {extensions_features::kApiContextualTasksPrivate,
         contextual_tasks::kContextualTasks},
        {});
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    UseHttpsTestServer();

    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {"google.com", "www.google.com", "example.com"};
    embedded_test_server()->SetSSLConfig(cert_config);
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url.starts_with("/search")) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content("<html><body>Search Page</body></html>");
            response->set_content_type("text/html");
            return response;
          }
          return nullptr;
        }));
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  void SetUpOnMainThread() override { ExtensionApiTest::SetUpOnMainThread(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    int port = embedded_test_server()->port();
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        base::StringPrintf("MAP * 127.0.0.1:%d", port));
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Contextual Tasks component extension's background script
// correctly allows external messages from allowed origins (google.com) and
// blocks unauthorized origins (example.com).
IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingTest,
                       ExternalConnectableAllowlist) {
  // Verify that the component extension is loaded.
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  struct {
    const std::string host_and_path;
    bool expected_to_connect;
  } test_cases[] = {
      // example.com is not allowed.
      {"https://example.com/search?q=foo", false},
      // google.com/search matches manifest and allowlist.
      {"https://google.com/search?q=foo", true},
  };

  for (const auto& test_case : test_cases) {
    ASSERT_TRUE(
        NavigateToURL(GetActiveWebContents(), GURL(test_case.host_and_path)));

    // Try to send a message to the extension from the page.
    std::string script = base::StringPrintf(
        R"(
        (async () => {
          if (!chrome.runtime || !chrome.runtime.sendMessage) {
            return 'no_runtime';
          }
          return new Promise((resolve) => {
            chrome.runtime.sendMessage(
                '%s', {type: 'contextualTasksPrivate.getState'}, (response) => {
                  if (chrome.runtime.lastError) {
                    resolve(chrome.runtime.lastError.message);
                  } else {
                    resolve('success');
                  }
                });
          });
        })()
        )",
        extension_misc::kContextualTasksExtensionId);

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);
    std::string result_string = result.ExtractString();

    // This test is only verifying that the correct pages have access to
    // extension functions rather than the actual result of
    // `contextualTasksPrivate.getState`.
    if (test_case.expected_to_connect) {
      EXPECT_EQ("success", result_string);
    } else {
      EXPECT_EQ("no_runtime", result_string);
    }
  }
}

}  // namespace extensions
