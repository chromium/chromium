// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_settings_util.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {

struct TestCase {
  std::string input;
  std::string expected;
};

class GlicSettingsUtilTest : public testing::TestWithParam<TestCase> {};

TEST_P(GlicSettingsUtilTest, GetHelpCenterUrl) {
  const auto& test_case = GetParam();
  EXPECT_EQ(GetHelpCenterUrl(test_case.input).spec(), test_case.expected)
      << "Failed for input: " << test_case.input;
}

std::vector<TestCase> GenerateTestCases() {
  std::string_view suffix = GetPlatformHelpSuffix();
  return {
      {"https://support.google.com/gemini?p=chrome_ks",
       base::StrCat({"https://support.google.com/gemini?p=chrome_ks", suffix})},
      {"https://support.google.com/gemini?p=chrome_min",
       base::StrCat(
           {"https://support.google.com/gemini?p=chrome_min", suffix})},
      // Already has suffix.
      {base::StrCat({"https://support.google.com/gemini?p=chrome_ks", suffix}),
       base::StrCat({"https://support.google.com/gemini?p=chrome_ks", suffix})},
      // No 'p' parameter.
      {"https://support.google.com/gemini",
       "https://support.google.com/gemini"},
      // Other parameter.
      {"https://support.google.com/gemini?foo=bar",
       "https://support.google.com/gemini?foo=bar"},
  };
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicSettingsUtilTest,
                         testing::ValuesIn(GenerateTestCases()));

}  // namespace glic
