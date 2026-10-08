// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/gemini_enterprise_url_policy_handler.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class GeminiEnterpriseUrlPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicy(base::Value value,
                 PolicyLevel level = POLICY_LEVEL_MANDATORY) {
    policies_.Set(key::kGeminiEnterpriseUrl, level, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }

  void ExpectAppliedPref(const std::string& expected) {
    handler_.ApplyPolicySettings(policies_, &prefs_);
    const base::Value* value = nullptr;
    ASSERT_TRUE(prefs_.GetValue(glic::prefs::kGlicGeminiEnterpriseUrl, &value));
    ASSERT_TRUE(value->is_string());
    EXPECT_EQ(value->GetString(), expected);
  }

  void ExpectSingleError(int message_id) {
    const std::vector<PolicyErrorMap::Data> errors =
        errors_.GetErrors(key::kGeminiEnterpriseUrl);
    ASSERT_EQ(1u, errors.size());
    EXPECT_EQ(PolicyMap::MessageType::kError, errors[0].level);
    EXPECT_NE(errors[0].message.find(l10n_util::GetStringUTF16(message_id)),
              std::u16string::npos);
  }

  GeminiEnterpriseUrlPolicyHandler handler_;
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
};

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, PolicyNotSet) {
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  handler_.ApplyPolicySettings(policies_, &prefs_);
  const base::Value* value = nullptr;
  EXPECT_FALSE(prefs_.GetValue(glic::prefs::kGlicGeminiEnterpriseUrl, &value));
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, WrongType) {
  base::DictValue policy_dict;
  policy_dict.Set("url", "https://business.gemini.google/");
  SetPolicy(base::Value(std::move(policy_dict)));

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.empty());
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, ValidUrl) {
  SetPolicy(base::Value("https://business.gemini.google/"));

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref("https://business.gemini.google/");
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, ValidUrlWithPort) {
  SetPolicy(base::Value("https://localhost.corp.google.com:10443/side-panel"));

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref("https://localhost.corp.google.com:10443/side-panel");
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, ValidUrlRecommendedLevel) {
  SetPolicy(base::Value("https://business.gemini.google/"),
            POLICY_LEVEL_RECOMMENDED);

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref("https://business.gemini.google/");
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, EmptyUrl) {
  SetPolicy(base::Value(""));

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref("");
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, InvalidUrl) {
  SetPolicy(base::Value("not-a-valid-url"));

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_INVALID_URL_ERROR);
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, WhitespaceOnlyUrl) {
  SetPolicy(base::Value("   "));

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_INVALID_URL_ERROR);
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, UrlWithEmbeddedCredentials) {
  SetPolicy(base::Value("https://user:password@business.gemini.google/"));

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_INVALID_URL_ERROR);
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, HttpUrl) {
  SetPolicy(base::Value("http://business.gemini.google/"));

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_URL_NOT_HTTPS_ERROR);
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, JavascriptUrl) {
  SetPolicy(base::Value("javascript:alert(1)"));

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_URL_NOT_HTTPS_ERROR);
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, AllowedSubdomainHost) {
  SetPolicy(
      base::Value("https://vertexaisearch.cloud.google.com/home/cid/12345678"));

  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.empty());

  ExpectAppliedPref(
      "https://vertexaisearch.cloud.google.com/home/cid/12345678");
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, DisallowedHost) {
  SetPolicy(base::Value("https://example.com/home/cid/12345678"));

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_GEMINI_ENTERPRISE_URL_HOST_NOT_ALLOWED_ERROR);
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, LookalikeHosts) {
  for (const char* url :
       {"https://business.gemini.google.example.com/",
        "https://examplecloud.google.com/", "https://cloud.google.com.evil/",
        "https://sub.business.gemini.google/"}) {
    SCOPED_TRACE(url);
    PolicyErrorMap errors;
    SetPolicy(base::Value(url));

    EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors));
    EXPECT_FALSE(errors.empty());
  }
}

TEST_F(GeminiEnterpriseUrlPolicyHandlerTest, LocalhostNotAllowed) {
  SetPolicy(base::Value("https://localhost:8443/side-panel"));

  EXPECT_FALSE(handler_.CheckPolicySettings(policies_, &errors_));
  ExpectSingleError(IDS_POLICY_GEMINI_ENTERPRISE_URL_HOST_NOT_ALLOWED_ERROR);
}

}  // namespace policy
