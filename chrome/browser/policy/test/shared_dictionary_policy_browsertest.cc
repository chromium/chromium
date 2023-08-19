// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace policy {

class SharedDictionaryPolicyTest : public PolicyTest {
 public:
  SharedDictionaryPolicyTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {network::features::kCompressionDictionaryTransportBackend,
         network::features::kCompressionDictionaryTransport},
        /*disabled_features=*/{});
  }
  ~SharedDictionaryPolicyTest() override = default;

  void SetUp() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    PolicyTest::SetUp();
  }

 protected:
  void SetCompressionDictionaryTransportPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kCompressionDictionaryTransportEnabled,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(enabled), nullptr);
    UpdateProviderPolicy(policies);
  }

  void RunRegisterDictionaryTest(bool expect_success) {
    // The CompressionDictionaryTransportEnabled policy doesn't support dynamic
    // refresh. So we are using an incognito mode browser for testing the
    // policy.
    Browser* incognito_browser =
        OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        incognito_browser,
        embedded_test_server()->GetURL("/shared_dictionary/blank.html")));
    EXPECT_EQ(
        "This is a test dictionary.\n",
        EvalJs(incognito_browser->tab_strip_model()->GetActiveWebContents(),
               content::JsReplace(R"(
  (async () => {
    return await (await fetch($1)).text();
  })()
           )",
                                  embedded_test_server()->GetURL(
                                      "/shared_dictionary/test.dict")))
            .ExtractString());
    EXPECT_EQ(expect_success,
              !GetSharedDictionaryUsageInfo(incognito_browser).empty());
  }

 private:
  std::vector<net::SharedDictionaryUsageInfo> GetSharedDictionaryUsageInfo(
      Browser* browser) {
    base::test::TestFuture<const std::vector<net::SharedDictionaryUsageInfo>&>
        result;
    browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetSharedDictionaryUsageInfo(result.GetCallback());
    return result.Get();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharedDictionaryPolicyTest,
                       CompressionDictionaryTransportDefault) {
  RunRegisterDictionaryTest(/*expect_success=*/true);
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryPolicyTest,
                       CompressionDictionaryTransportEnabled) {
  SetCompressionDictionaryTransportPolicy(true);
  RunRegisterDictionaryTest(/*expect_success=*/true);
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryPolicyTest,
                       CompressionDictionaryTransportDisabled) {
  SetCompressionDictionaryTransportPolicy(false);
  RunRegisterDictionaryTest(/*expect_success=*/false);
}

}  // namespace policy
