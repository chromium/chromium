// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/gemini_enterprise_settings_policy_handler.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class GeminiEnterpriseSettingsPolicyHandlerTest : public testing::Test {
 public:
  GeminiEnterpriseSettingsPolicyHandlerTest()
      : handler_(Schema::Wrap(GetChromeSchemaData())) {}

 protected:
  void SetPolicy(const base::DictValue& policy_dict,
                 PolicyLevel level = POLICY_LEVEL_MANDATORY) {
    policies_.Set(key::kGeminiEnterpriseSettings, level, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, base::Value(policy_dict.Clone()),
                  nullptr);
  }

  void ExpectAppliedPref(const base::DictValue& expected) {
    handler_.ApplyPolicySettings(policies_, &prefs_);
    const base::Value* value = nullptr;
    ASSERT_TRUE(
        prefs_.GetValue(glic::prefs::kGlicGeminiEnterpriseSettings, &value));
    ASSERT_TRUE(value->is_dict());
    EXPECT_EQ(value->GetDict(), expected);
  }

  void ExpectSingleError(int message_id) {
    const std::vector<PolicyErrorMap::Data> errors =
        errors_.GetErrors(key::kGeminiEnterpriseSettings);
    ASSERT_EQ(1u, errors.size());
    EXPECT_EQ(PolicyMap::MessageType::kError, errors[0].level);
    EXPECT_NE(errors[0].message.find(l10n_util::GetStringUTF16(message_id)),
              std::u16string::npos);
  }

  GeminiEnterpriseSettingsPolicyHandler handler_;
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
};

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, PolicyNotSet) {
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  handler_.ApplyPolicySettings(policies_, &prefs_);
  const base::Value* value = nullptr;
  EXPECT_FALSE(
      prefs_.GetValue(glic::prefs::kGlicGeminiEnterpriseSettings, &value));
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, ValidUrl) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "https://business.gemini.google/");
  SetPolicy(policy_dict);

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref(policy_dict);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, ValidUrlWithPort) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "https://localhost.corp.google.com:10443/side-panel");
  SetPolicy(policy_dict);

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref(policy_dict);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, ValidUrlRecommendedLevel) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "https://business.gemini.google/");
  SetPolicy(policy_dict, POLICY_LEVEL_RECOMMENDED);

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref(policy_dict);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, ValidLegacyParameters) {
  base::DictValue policy_dict;
  policy_dict.Set("project_id", "my-project");
  policy_dict.Set("app_id", "my-app");
  policy_dict.Set("location", "global");
  SetPolicy(policy_dict);

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref(policy_dict);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, ValidUrlAndLegacyParameters) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "https://business.gemini.google/");
  policy_dict.Set("project_id", "my-project");
  policy_dict.Set("app_id", "my-app");
  policy_dict.Set("location", "global");
  SetPolicy(policy_dict);

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref(policy_dict);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest,
       EmptyUrlLeavesLegacyParametersInEffect) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "");
  policy_dict.Set("project_id", "my-project");
  policy_dict.Set("app_id", "my-app");
  policy_dict.Set("location", "global");
  SetPolicy(policy_dict);

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref(policy_dict);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, InvalidUrl) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "not-a-valid-url");
  SetPolicy(policy_dict);

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_INVALID_URL_ERROR);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, WhitespaceOnlyUrl) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "   ");
  SetPolicy(policy_dict);

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_INVALID_URL_ERROR);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, UrlWithEmbeddedCredentials) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "https://user:password@business.gemini.google/");
  SetPolicy(policy_dict);

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_INVALID_URL_ERROR);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, HttpUrl) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "http://business.gemini.google/");
  SetPolicy(policy_dict);

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_URL_NOT_HTTPS_ERROR);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, JavascriptUrl) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "javascript:alert(1)");
  SetPolicy(policy_dict);

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_URL_NOT_HTTPS_ERROR);
}

// A rejected `url` discards the whole policy, including otherwise valid legacy
// parameters, because the handler list skips ApplyPolicySettings entirely.
TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest,
       InvalidUrlDiscardsLegacyParameters) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "not-a-valid-url");
  policy_dict.Set("project_id", "my-project");
  policy_dict.Set("app_id", "my-app");
  policy_dict.Set("location", "global");
  SetPolicy(policy_dict);

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_INVALID_URL_ERROR);
}

TEST_F(GeminiEnterpriseSettingsPolicyHandlerTest, EmptyPolicy) {
  base::DictValue policy_dict;
  SetPolicy(policy_dict);

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());
}

}  // namespace policy
