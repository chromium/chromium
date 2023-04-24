// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ime_rules_config.h"

#include "ash/constants/app_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace input_method {
namespace {

TextFieldContextualInfo FakeTextFieldContextualInfo(GURL url) {
  TextFieldContextualInfo info;
  info.tab_url = url;
  return info;
}
}  // namespace

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
        "https://reddit.com",
        "https://teams.microsoft.com",
        "https://twitter.com",
        "https://whatsapp.com",

        "https://www.youtube.com",
        "https://b.corp.google.com/134",
        "https://docs.google.com/document/d/documentId/edit",
        "https://amazon.com.au",
        "https://amazon.com.au/gp/new-releases",
        "http://smile.amazon.com",
        "http://www.abc.smile.amazon.com.au/abc+com+au/some/other/text"));

TEST_P(ImeRulesConfigAutoCorrectDisabledTest, IsAutoCorrectDisabled) {
  EXPECT_TRUE(
      IsAutoCorrectDisabled(FakeTextFieldContextualInfo(GURL(GetParam()))));
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
  EXPECT_FALSE(
      IsAutoCorrectDisabled(FakeTextFieldContextualInfo(GURL(GetParam()))));
}
}  // namespace input_method
}  // namespace ash
