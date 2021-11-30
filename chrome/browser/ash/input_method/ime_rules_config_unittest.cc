// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ime_rules_config.h"

#include <vector>

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

}  // namespace

using ::testing::UnorderedElementsAre;

class ImeRulesConfigTest : public testing::Test {
 public:
  ImeRulesConfigTest() = default;
  ~ImeRulesConfigTest() override = default;

  std::vector<std::string> GetAutocorrectDomainDenylistForTest(
      const ImeRulesConfig& rules) {
    return rules.auto_correct_domain_denylist_;
  }
};

TEST_F(ImeRulesConfigTest, LoadRulesFromFieldTrial) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  ImeRulesConfig rules;
  EXPECT_THAT(GetAutocorrectDomainDenylistForTest(rules),
              UnorderedElementsAre("docs.google.com", "chromium.org",
                                   "example.com", "test.com.au"));
}

TEST_F(ImeRulesConfigTest, IsAutoCorrectAllowed) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  ImeRulesConfig rules;
  EXPECT_TRUE(rules.IsAutoCorrectAllowed(GURL("http://abc.com")));
  EXPECT_FALSE(rules.IsAutoCorrectAllowed(GURL("https://www.example.com")));
  EXPECT_FALSE(rules.IsAutoCorrectAllowed(GURL("https://test.com.au")));
  EXPECT_FALSE(rules.IsAutoCorrectAllowed(
      GURL("https://docs.google.com/document/d/documentId/edit")));
}

}  // namespace input_method
}  // namespace ash
