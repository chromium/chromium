// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace policy {

// Similar to PolicyTest but sets the proper policy before the browser is
// started.
class PolicyVariationsServiceTest : public PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kVariationsRestrictParameter, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value("restricted"), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyVariationsServiceTest, VariationsURLIsValid) {
  const std::string default_variations_url =
      variations::VariationsService::GetDefaultVariationsServerURLForTesting();

  const GURL url =
      g_browser_process->variations_service()->GetVariationsServerURL(
          variations::VariationsService::HttpOptions::USE_HTTPS);
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
}

}  // namespace policy
