// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace policy {

class ZstdContentEncodingPolicyTest : public PolicyTest {
 public:
  ZstdContentEncodingPolicyTest() {
    feature_list_.InitAndEnableFeature(net::features::kZstdContentEncoding);
  }
  ~ZstdContentEncodingPolicyTest() override = default;

  void SetUp() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    PolicyTest::SetUp();
  }

 protected:
  void SetZstdContentEncodingPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kZstdContentEncodingEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(enabled),
                 nullptr);
    UpdateProviderPolicy(policies);
  }

  void RunAcceptEncodingTest(bool expect_success) {
    std::string navigation_accept_encoding_header;
    base::RunLoop navigation_loop;

    embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          if (request.relative_url == "/fetch.html") {
            auto it = request.headers.find("Accept-Encoding");
            if (it != request.headers.end()) {
              navigation_accept_encoding_header = it->second;
            }
            navigation_loop.Quit();
          }
        }));

    ASSERT_TRUE(embedded_test_server()->Start());

    // The ZstdContentEncodingEnabled policy doesn't support dynamic
    // refresh. So we are using an incognito mode browser for testing the
    // policy.
    Browser* incognito_browser =
        OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        incognito_browser, embedded_test_server()->GetURL("/fetch.html")));

    EXPECT_EQ(expect_success,
              CheckAcceptEncodingHeader(navigation_accept_encoding_header));
  }

 private:
  bool CheckAcceptEncodingHeader(std::string accept_encoding_header) {
    if (accept_encoding_header.find("zstd") != std::string::npos) {
      return true;
    }
    return false;
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZstdContentEncodingPolicyTest,
                       ZstdContentEncodingDefault) {
  RunAcceptEncodingTest(/*expect_success=*/true);
}

IN_PROC_BROWSER_TEST_F(ZstdContentEncodingPolicyTest,
                       ZstdContentEncodingEnabled) {
  SetZstdContentEncodingPolicy(true);
  RunAcceptEncodingTest(/*expect_success=*/true);
}

IN_PROC_BROWSER_TEST_F(ZstdContentEncodingPolicyTest,
                       ZstdContentEncodingDisabled) {
  SetZstdContentEncodingPolicy(false);
  RunAcceptEncodingTest(/*expect_success=*/false);
}

}  // namespace policy
