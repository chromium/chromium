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
            "test",
            "example",
            "chromium",
            "docs.google"
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

  EXPECT_THAT(
      GetAutocorrectDomainDenylistForTest(),
      UnorderedElementsAre("docs.google", "chromium", "example", "test"));
}

class ImeRulesConfigEnabledTest : public testing::TestWithParam<std::string> {
 public:
  ImeRulesConfigEnabledTest() = default;
  ~ImeRulesConfigEnabledTest() override = default;

  std::vector<std::string> GetAutocorrectDomainDenylistForTest() {
    return ImeRulesConfig::GetInstance()->rule_auto_correct_domain_denylist_;
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ImeRulesConfigEnabledTest,
    testing::Values(
        "https://amazon.com",
        "https://b.corp.google.com",
        "https://buganizer.corp.google.com",
        "https://cider.corp.google.com",
        "https://classroom.google.com",
        "https://desmos.com",
        "https://docs.google.com",
        "https://facebook.com",
        "https://instagram.com",
        "https://outlook.live.com",
        "https://outlook.office.com",
        "https://quizlet.com",
        "https://whatsapp.com",

        "https://www.example.com",
        "https://test.com.au",
        "https://www.youtube.com",
        "https://b.corp.google.com/134",
        "https://docs.google.com/document/d/documentId/edit",
        "https://amazon.com.au",
        "https://amazon.com.au/gp/new-releases",
        "http://smile.amazon.com",
        "http://www.abc.smile.amazon.com.au/abc+com+au/some/other/text"));

TEST_P(ImeRulesConfigEnabledTest, IsAutoCorrectEnabled) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  auto* rules = ImeRulesConfig::GetInstance();
  EXPECT_TRUE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL(GetParam()))));
}

class ImeRulesConfigDisabledTest : public testing::TestWithParam<std::string> {
 public:
  ImeRulesConfigDisabledTest() = default;
  ~ImeRulesConfigDisabledTest() override = default;

  std::vector<std::string> GetAutocorrectDomainDenylistForTest() {
    return ImeRulesConfig::GetInstance()->rule_auto_correct_domain_denylist_;
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ImeRulesConfigDisabledTest,
    testing::Values("",
                    "http://",
                    "http://abc.com",
                    "http://abc.com/amazon+com",
                    "http://amazon",
                    "http://amazon/test",
                    "http://amazon.domain.com",
                    "http://smile.amazon.foo.com",
                    "http://my.own.quizlet.uniquie.co.uk/testing",
                    "http://sites.google.com/view/e14s-test",
                    "http://amazon/com/test",
                    "http://not-amazon.com/test",
                    "http://.com/test"));

TEST_P(ImeRulesConfigDisabledTest, IsAutoCorrectDisabled) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  auto* rules = ImeRulesConfig::GetInstance();
  EXPECT_FALSE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL(GetParam()))));
}

}  // namespace input_method
}  // namespace ash
