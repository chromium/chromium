// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class RestrictSigninToPatternPolicyHandlerTest : public testing::Test {
 protected:
  void SetPattern(std::string_view pattern) {
    policies_.Set(key::kRestrictSigninToPattern, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                  base::Value(pattern), nullptr);
  }

  void ApplyPolicies() {
    handler_list_->ApplyPolicySettings(policies_, &prefs_, &errors_, nullptr,
                                       nullptr);
  }

  void CheckBogusPatternWarnsAndApplies(std::string_view pattern) {
    SetPattern(pattern);
    ApplyPolicies();

    EXPECT_TRUE(errors_.HasError(key::kRestrictSigninToPattern));
    EXPECT_FALSE(errors_.HasFatalError(key::kRestrictSigninToPattern));
    EXPECT_FALSE(errors_.GetErrors(key::kRestrictSigninToPattern).empty());
    EXPECT_EQ(errors_.GetErrorMessages(key::kRestrictSigninToPattern,
                                       PolicyMap::MessageType::kWarning),
              l10n_util::GetStringUTF16(
                  IDS_POLICY_RESTRICT_SIGNIN_TO_PATTERN_WARNING));

    const base::Value* value = nullptr;
    EXPECT_TRUE(prefs_.GetValue(prefs::kGoogleServicesUsernamePattern, &value));
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->is_string());
    EXPECT_EQ(value->GetString(), pattern);
  }

  void CheckValidPatternNoWarningAndApplies(std::string_view pattern) {
    SetPattern(pattern);
    ApplyPolicies();

    EXPECT_FALSE(errors_.HasError(key::kRestrictSigninToPattern));

    const base::Value* value = nullptr;
    EXPECT_TRUE(prefs_.GetValue(prefs::kGoogleServicesUsernamePattern, &value));
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->is_string());
    EXPECT_EQ(value->GetString(), pattern);
  }

  std::unique_ptr<ConfigurationPolicyHandlerList> handler_list_ =
      BuildHandlerList(Schema::Wrap(GetChromeSchemaData()));
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
};

TEST_F(RestrictSigninToPatternPolicyHandlerTest, WarnsOnBogusPatternZero) {
  CheckBogusPatternWarnsAndApplies("0");
}

TEST_F(RestrictSigninToPatternPolicyHandlerTest, WarnsOnBogusPatternFalse) {
  CheckBogusPatternWarnsAndApplies("false");
}

TEST_F(RestrictSigninToPatternPolicyHandlerTest,
       WarnsOnBogusPatternLoginNotAllowed) {
  CheckBogusPatternWarnsAndApplies("Login@NotAllowed");
}

TEST_F(RestrictSigninToPatternPolicyHandlerTest,
       WarnsOnBogusPatternWildcardNotAllowed) {
  CheckBogusPatternWarnsAndApplies(".*@NotAllowed");
}

TEST_F(RestrictSigninToPatternPolicyHandlerTest, WarnsOnInvalidRegex) {
  CheckBogusPatternWarnsAndApplies("[");
}

TEST_F(RestrictSigninToPatternPolicyHandlerTest,
       WarnsOnMissingWildcardBeforeAt) {
  CheckBogusPatternWarnsAndApplies("@domain.com");
  errors_.Clear();
  prefs_.Clear();
  policies_.Clear();
  CheckBogusPatternWarnsAndApplies("^@domain.com");
}

TEST_F(RestrictSigninToPatternPolicyHandlerTest, ValidPatternsDoNotWarn) {
  constexpr std::string_view kValidPatterns[] = {
      ".*",
      ".+",
      "*@example.com",
      ".*@example\\.com",
      "^.*@(corp|sub)\\.example\\.com$",
      "alice@corp\\.com|bob@corp\\.com",
      ".*@google.com",
      ".*@acme\\.[a-z]{2,}",
      ".*@acme\\..*",
      ".*@acme\\.(?:com|org)",
      "",
  };

  for (std::string_view pattern : kValidPatterns) {
    SCOPED_TRACE(testing::Message() << "Testing pattern: " << pattern);
    errors_.Clear();
    prefs_.Clear();
    policies_.Clear();
    CheckValidPatternNoWarningAndApplies(pattern);
  }
}

}  // namespace policy
