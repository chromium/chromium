// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ime_rules_config.h"

#include <vector>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace input_method {
namespace {

const char kNormalAutocorrectRulesParams[] = R"(
    {
      "rules":{
        "ac-domain-denylist":{
          "items": [
            "test.com.au",
            "example.com",
            "chromium.org",
            "docs.google.com"
          ]
        }
     }
    })";

TextFieldContextualInfo FakeTextFieldContextualInfo(GURL url) {
  TextFieldContextualInfo info;
  info.tab_url = url;
  return info;
}
}  // namespace

using ::testing::UnorderedElementsAre;

class ImeRulesConfigTest : public testing::Test {
 public:
  ImeRulesConfigTest() = default;
  ~ImeRulesConfigTest() override = default;

  std::vector<std::string> GetAutocorrectDomainDenylistForTest() {
    return ImeRulesConfig::GetInstance()->rule_auto_correct_domain_denylist_;
  }
};

TEST_F(ImeRulesConfigTest, LoadRulesFromFieldTrial) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  EXPECT_THAT(GetAutocorrectDomainDenylistForTest(),
              UnorderedElementsAre("docs.google.com", "chromium.org",
                                   "example.com", "test.com.au"));
}

TEST_F(ImeRulesConfigTest, IsAutoCorrectDisabled) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  auto* rules = ImeRulesConfig::GetInstance();
  EXPECT_FALSE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL("http://abc.com"))));
  EXPECT_TRUE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL("https://www.example.com"))));
  EXPECT_TRUE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL("https://test.com.au"))));
  EXPECT_TRUE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL("https://www.youtube.com"))));
  EXPECT_TRUE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL("https://b.corp.google.com/134"))));
  EXPECT_TRUE(rules->IsAutoCorrectDisabled(FakeTextFieldContextualInfo(
      GURL("https://docs.google.com/document/d/documentId/edit"))));
}

}  // namespace input_method
}  // namespace ash
