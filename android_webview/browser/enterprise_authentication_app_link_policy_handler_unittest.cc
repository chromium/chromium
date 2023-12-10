// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/enterprise_authentication_app_link_policy_handler.h"

#include "base/json/json_reader.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"

namespace policy {

class EnterpriseAuthenticationAppLinkPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    Schema chrome_schema = Schema::Wrap(policy::GetChromeSchemaData());
    handler_list_.AddHandler(base::WrapUnique<ConfigurationPolicyHandler>(
        new EnterpriseAuthenticationAppLinkPolicyHandler(
            policy::key::kEnterpriseAuthenticationAppLinkPolicy,
            android_webview::prefs::kEnterpriseAuthAppLinkPolicy)));
  }
};

TEST_F(EnterpriseAuthenticationAppLinkPolicyHandlerTest, ValidPolicy) {
  PolicyMap policy;
  policy.Set(policy::key::kEnterpriseAuthenticationAppLinkPolicy,
             POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
             base::JSONReader::Read(
                 "["
                 "  {"
                 "    \"url\": \"https://www.testserver1.com/login\""
                 "  },"
                 "  {"
                 "    \"url\": \"https://www.testserver2.com/login\""
                 "  }"
                 "]"),
             nullptr);
  this->UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;
  std::optional<base::Value> expected = base::JSONReader::Read(R"(
    [
     "https://www.testserver1.com/login",
     "https://www.testserver2.com/login"
    ]
  )");

  EXPECT_TRUE(store_->GetValue(
      android_webview::prefs::kEnterpriseAuthAppLinkPolicy, &pref_value));
  ASSERT_TRUE(pref_value);
  EXPECT_EQ(expected, *pref_value);
}

TEST_F(EnterpriseAuthenticationAppLinkPolicyHandlerTest, InvalidPolicy) {
  PolicyMap policy;
  policy.Set(policy::key::kEnterpriseAuthenticationAppLinkPolicy,
             POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
             base::JSONReader::Read(
                 "["
                 "  {"
                 "    \"abc\": \"https://www.testserver1.com/login\""
                 "  },"
                 "]"),
             nullptr);
  this->UpdateProviderPolicy(policy);
  const base::Value* pref_value = nullptr;

  EXPECT_FALSE(store_->GetValue(
      android_webview::prefs::kEnterpriseAuthAppLinkPolicy, &pref_value));
  ASSERT_FALSE(pref_value);
}
}  // namespace policy