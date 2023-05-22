// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_input.h"

#include "ash/constants/app_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace input_method {

class AssistiveInputAutoCorrectDisabledTest
    : public testing::TestWithParam<std::string> {
 public:
  AssistiveInputAutoCorrectDisabledTest() = default;
  ~AssistiveInputAutoCorrectDisabledTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AssistiveInputAutoCorrectDisabledTest,
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

TEST_P(AssistiveInputAutoCorrectDisabledTest, IsAutoCorrectDisabled) {
  EXPECT_TRUE(IsAssistiveInputDisabled(GURL(GetParam())));
}

class AssistiveInputAutoCorrectEnabledTest
    : public testing::TestWithParam<std::string> {
 public:
  AssistiveInputAutoCorrectEnabledTest() = default;
  ~AssistiveInputAutoCorrectEnabledTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AssistiveInputAutoCorrectEnabledTest,
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

TEST_P(AssistiveInputAutoCorrectEnabledTest, IsAutoCorrectEnabled) {
  EXPECT_FALSE(IsAssistiveInputDisabled(GURL(GetParam())));
}

}  // namespace input_method
}  // namespace ash
