// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/glic_private/glic_private_api_test_base.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/component_extension_resources.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "url/gurl.h"

namespace extensions {

class GlicMessagingBrowserTest : public GlicPrivateApiTestBase {
 public:
  GlicMessagingBrowserTest() {
    feature_list_.InitWithFeatures(
        {extensions_features::kApiGlicPrivate,
         extensions_features::kApiGlicAccessFromGoogleWebpage,
         extensions_features::kApiGlicAccessFromPromotionPage},
        {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};


namespace {

#if !BUILDFLAG(IS_ANDROID)
content::EvalJsResult ExecuteInvoke(content::WebContents* web_contents,
                                    const std::string& prompt_id,
                                    const std::string& invocation_source) {
  std::string script = base::StringPrintf(
      R"(
      (async () => {
        if (!chrome.runtime || !chrome.runtime.sendMessage) {
          return 'no_runtime';
        }
        return new Promise((resolve) => {
          chrome.runtime.sendMessage(
              '%s', {type: 'glicPrivate.invoke', args: {
                promptId: '%s',
                invocationSource: '%s'
              }}, (response) => {
                if (chrome.runtime.lastError) {
                  resolve(chrome.runtime.lastError.message);
                } else {
                  resolve('success');
                }
              });
        });
      })()
      )",
      extension_misc::kGlicExtensionId, prompt_id.c_str(),
      invocation_source.c_str());

  return content::EvalJs(web_contents, script);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// Tests that gemini.google.com can successfully send a getState message to the
// glic component extension over HTTPS, but example.com cannot.
IN_PROC_BROWSER_TEST_F(GlicMessagingBrowserTest, ExternalConnectable) {
  // The glic extension should be loaded as a component extension.
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  struct {
    const std::string_view host;
    bool expected_to_connect;
  } test_cases[] = {
      // example.com is not in the externally_connectable.matches list.
      {"https://example.com/empty.html", false},
      // gemini.google.com is allowed over HTTPS in the manifest.
      {"https://gemini.google.com/empty.html", true},
  };

  for (const auto& test_case : test_cases) {
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), GURL(test_case.host)));

    // Try to send a message to the extension.
    std::string script = base::StringPrintf(
        R"(
        (async () => {
          if (!chrome.runtime || !chrome.runtime.sendMessage) {
            return 'no_runtime';
          }
          return new Promise((resolve) => {
            chrome.runtime.sendMessage(
                '%s', {type: 'glicPrivate.getState'}, (response) => {
                  if (chrome.runtime.lastError) {
                    resolve(chrome.runtime.lastError.message);
                  } else {
                    resolve('success');
                  }
                });
          });
        })()
        )",
        extension_misc::kGlicExtensionId);

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);

    std::string result_string = result.ExtractString();
    if (test_case.expected_to_connect) {
      EXPECT_EQ("success", result_string)
          << "Expected success for " << test_case.host
          << " but got: " << result_string;
    } else {
      // It should fail with "Could not establish connection" or no runtime.
      EXPECT_TRUE(
          result_string == "no_runtime" ||
          result_string ==
              "Could not establish connection. Receiving end does not exist.")
          << "Unexpected result for " << test_case.host << ": "
          << result_string;
    }
  }
}

// Invoke is not supported in Android yet.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicMessagingBrowserTest, InvokeAPI) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL("https://gemini.google.com/empty.html")));

  content::EvalJsResult result =
      ExecuteInvoke(GetActiveWebContents(), "1", "universal-cart");

  std::string result_string = result.ExtractString();
  EXPECT_EQ("Uncaught Error: local-glic-not-enabled", result_string);
}


#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace extensions
