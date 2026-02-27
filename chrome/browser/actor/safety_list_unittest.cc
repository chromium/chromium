// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list.h"

#include <string>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;

TEST(SafetyListTest, ParseJsonList) {
  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<SafetyList, SafetyListParseResult>>
        matches;
  } kTestCases[] = {
      {
          "invalid_structure",
          R"json([{"from": "a.com", "to": "b.com"},[]])json",
          ErrorIs(SafetyListParseResult::kJsonListValueNotADictionary),
      },
      {
          "missing_from",
          R"json([{"from": "a.com", "to": "b.com"},{"to": "d.com"}])json",
          ErrorIs(SafetyListParseResult::kInvalidFromField),
      },
      {
          "missing_to",
          R"json([{"from": "a.com", "to": "b.com"},{"from": "c.com"}])json",
          ErrorIs(SafetyListParseResult::kInvalidToField),
      },
      {
          "invalid_from_pattern",
          R"json([
            {"from": "http://", "to": "b.com"}
          ])json",
          ErrorIs(SafetyListParseResult::kInvalidFromUrlPattern),
      },
      {
          "invalid_to_pattern",
          R"json([
            {"from": "a.com", "to": "http://"}
          ])json",
          ErrorIs(SafetyListParseResult::kInvalidToUrlPattern),
      },
      {
          "valid_patterns",
          R"json([
            {"from": "a.com", "to": "b.com"}
          ])json",
          ValueIs(SafetyList({{ContentSettingsPattern::FromString("a.com"),
                               ContentSettingsPattern::FromString("b.com")}})),
      },
      {
          "empty_list",
          R"json([])json",
          ValueIs(SafetyList()),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    EXPECT_THAT(SafetyList::ParseEntriesFromJson(
                    base::test::ParseJsonList(test_case.json)),
                test_case.matches);
  }
}

}  // namespace

}  // namespace actor
