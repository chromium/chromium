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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  EXPECT_THAT(
      GetAutocorrectDomainDenylistForTest(),
      UnorderedElementsAre("docs.google", "chromium", "example", "test"));
}

class ImeRulesConfigAutoCorrectDisabledTest
    : public testing::TestWithParam<std::string> {
 public:
  ImeRulesConfigAutoCorrectDisabledTest() = default;
  ~ImeRulesConfigAutoCorrectDisabledTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ImeRulesConfigAutoCorrectDisabledTest,
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

TEST_P(ImeRulesConfigAutoCorrectDisabledTest, IsAutoCorrectDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  auto* rules = ImeRulesConfig::GetInstance();
  EXPECT_TRUE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL(GetParam()))));
}

class ImeRulesConfigAutoCorrectEnabledTest
    : public testing::TestWithParam<std::string> {
 public:
  ImeRulesConfigAutoCorrectEnabledTest() = default;
  ~ImeRulesConfigAutoCorrectEnabledTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ImeRulesConfigAutoCorrectEnabledTest,
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

TEST_P(ImeRulesConfigAutoCorrectEnabledTest, IsAutoCorrectEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});

  auto* rules = ImeRulesConfig::GetInstance();
  EXPECT_FALSE(rules->IsAutoCorrectDisabled(
      FakeTextFieldContextualInfo(GURL(GetParam()))));
}

class ImeRulesConfigMultiWordSuggestDisabledTest
    : public testing::TestWithParam<std::string> {
 public:
  ImeRulesConfigMultiWordSuggestDisabledTest() = default;
  ~ImeRulesConfigMultiWordSuggestDisabledTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ImeRulesConfigMultiWordSuggestDisabledTest,
    testing::Values("https://amazon.com",
                    "https://b.corp.google.com",
                    "https://buganizer.corp.google.com",
                    "https://cider.corp.google.com",
                    "https://classroom.google.com",
                    "https://desmos.com",
                    "https://docs.google.com",
                    "https://facebook.com",
                    "https://instagram.com",
                    "https://mail.google.com/mail",
                    "https://outlook.live.com",
                    "https://outlook.office.com",
                    "https://quizlet.com",
                    "https://whatsapp.com"));

TEST_P(ImeRulesConfigMultiWordSuggestDisabledTest, IsMultiWordSuggestDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});
  auto* rules = ImeRulesConfig::GetInstance();
  EXPECT_TRUE(rules->IsMultiWordSuggestDisabled(GURL(GetParam())));
}

class ImeRulesConfigMultiWordSuggestEnabledTest
    : public testing::TestWithParam<std::string> {
 public:
  ImeRulesConfigMultiWordSuggestEnabledTest() = default;
  ~ImeRulesConfigMultiWordSuggestEnabledTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ImeRulesConfigMultiWordSuggestEnabledTest,
    testing::Values("",
                    "http://",
                    "http://abc.com",
                    "http://abc.com/amazon+com",
                    "http://amazon",
                    "http://amazon/com/test",
                    "http://amazon/test",
                    "http://amazon.domain.com",
                    "https://mail.google.com/chat",
                    "http://my.own.quizlet.uniquie.co.uk/testing",
                    "http://not-amazon.com/test",
                    "http://sites.google.com/view/e14s-test",
                    "http://smile.amazon.foo.com",
                    "http://.com/test"));

TEST_P(ImeRulesConfigMultiWordSuggestEnabledTest, IsMultiWordSuggestEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kImeRuleConfig,
      {{"json_rules", kNormalAutocorrectRulesParams}});
  auto* rules = ImeRulesConfig::GetInstance();
  EXPECT_FALSE(rules->IsMultiWordSuggestDisabled(GURL(GetParam())));
}

}  // namespace input_method
}  // namespace ash
