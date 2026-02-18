// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list.h"

#include <string>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;

TEST(SafetyListTest, ContainsUrlPair) {
  const struct {
    const std::string desc;
    const ContentSettingsPattern source_pattern;
    const ContentSettingsPattern destination_pattern;
    const char* from_url;
    const char* to_url;
    bool expected;
  } kTestCases[] = {
      {
          "source_wildcard_subdomain_match",
          ContentSettingsPattern::FromString("[*.]a.com"),
          ContentSettingsPattern::FromString("b.com"),
          "https://sub.a.com",
          "https://b.com",
          true,
      },
      {
          "source_wildcard_protocol_match",
          ContentSettingsPattern::FromString("[*.]a.com"),
          ContentSettingsPattern::FromString("b.com"),
          "https://a.com",
          "https://b.com",
          true,
      },
      {
          "source_wildcard_match",
          ContentSettingsPattern::FromString("*"),
          ContentSettingsPattern::FromString("b.com"),
          "https://a.com",
          "https://b.com",
          true,
      },
      {
          "source_wildcard_subdomain_no_match",
          ContentSettingsPattern::FromString("[*.]a.com"),
          ContentSettingsPattern::FromString("b.com"),
          "https://other.com",
          "https://b.com",
          false,
      },
      {
          "destination_wildcard_subdomain_match",
          ContentSettingsPattern::FromString("a.com"),
          ContentSettingsPattern::FromString("[*.]b.com"),
          "https://a.com",
          "https://sub.b.com",
          true,
      },
      {
          "destination_wildcard_match",
          ContentSettingsPattern::FromString("a.com"),
          ContentSettingsPattern::FromString("*"),
          "https://a.com",
          "https://b.com",
          true,
      },
      {
          "destination_wildcard_protocol_match",
          ContentSettingsPattern::FromString("a.com"),
          ContentSettingsPattern::FromString("[*.]b.com"),
          "https://a.com",
          "https://b.com",
          true,
      },
      {
          "destination_wildcard_subdomain_no_match",
          ContentSettingsPattern::FromString("a.com"),
          ContentSettingsPattern::FromString("[*.]b.com"),
          "https://a.com",
          "https://other.com",
          false,
      },
      {
          "pair_no_match",
          ContentSettingsPattern::FromString("a.com"),
          ContentSettingsPattern::FromString("b.com"),
          "https://c.com",
          "https://d.com",
          false,
      },
      {
          "pair_source_match_only",
          ContentSettingsPattern::FromString("a.com"),
          ContentSettingsPattern::FromString("b.com"),
          "https://a.com",
          "https://c.com",
          false,
      },
      {
          "pair_destination_match_only",
          ContentSettingsPattern::FromString("a.com"),
          ContentSettingsPattern::FromString("b.com"),
          "https://c.com",
          "https://b.com",
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    SafetyList::Patterns list;
    list.push_back({test_case.source_pattern, test_case.destination_pattern});
    SCOPED_TRACE(test_case.desc);
    EXPECT_EQ(test_case.expected,
              SafetyList(list).ContainsUrlPair(GURL(test_case.from_url),
                                               GURL(test_case.to_url)));
  }
}

TEST(SafetyListTest, ContainsUrlPair_MultipleEntries) {
  SafetyList::Patterns list;
  list.push_back({ContentSettingsPattern::FromString("a.com"),
                  ContentSettingsPattern::FromString("b.com")});
  list.push_back({ContentSettingsPattern::FromString("c.com"),
                  ContentSettingsPattern::FromString("d.com")});
  EXPECT_TRUE(SafetyList(list).ContainsUrlPair(GURL("https://c.com"),
                                               GURL("https://d.com")));
}

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
    EXPECT_THAT(SafetyList::ParsePatternListFromJson(
                    base::test::ParseJsonList(test_case.json)),
                test_case.matches);
  }
}

TEST(SafetyListTest, ContainsUrlPairWithWildcardSource) {
  const struct {
    const std::string desc;
    const ContentSettingsPattern source_pattern;
    const ContentSettingsPattern destination_pattern;
    const char* from_url;
    const char* to_url;
    bool expected;
  } kTestCases[] = {
      {
          "wildcard_source_match_different_origin",
          ContentSettingsPattern::FromString("*"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://a.com",
          "https://blocked.com",
          true,
      },
      {
          "wildcard_source_match_same_origin",
          ContentSettingsPattern::FromString("*"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://blocked.com",
          "https://blocked.com",
          true,
      },
      {
          "domain_wildcard_source_match_different_origin",
          ContentSettingsPattern::FromString("[*.]a.com"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://sub.a.com",
          "https://blocked.com",
          true,
      },
      {
          "domain_wildcard_source_match_same_origin",
          ContentSettingsPattern::FromString("[*.]a.com"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://blocked.com",
          "https://blocked.com",
          false,
      },
      {
          "specific_source_no_match_different_origin",
          ContentSettingsPattern::FromString("a.com"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://a.com",
          "https://blocked.com",
          false,
      },
      {
          "specific_source_no_match_same_origin",
          ContentSettingsPattern::FromString("blocked.com"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://blocked.com",
          "https://blocked.com",
          false,
      },
      {
          "wildcard_source_different_destination_no_match",
          ContentSettingsPattern::FromString("*"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://a.com",
          "https://allowed.com",
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    SafetyList::Patterns list;
    list.push_back({test_case.source_pattern, test_case.destination_pattern});
    SCOPED_TRACE(test_case.desc);
    EXPECT_EQ(test_case.expected,
              SafetyList(list).ContainsUrlPairWithWildcardSource(
                  GURL(test_case.from_url), GURL(test_case.to_url)));
  }
}

TEST(SafetyListTest, ContainsPatternMatchingSelfNavigation) {
  const struct {
    const std::string desc;
    const ContentSettingsPattern source_pattern;
    const ContentSettingsPattern destination_pattern;
    const char* url;
    bool expected;
  } kTestCases[] = {
      {
          "wildcard_source_and_matches_domain",
          ContentSettingsPattern::FromString("*"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://blocked.com",
          true,
      },
      {
          "wildcard_source_and_matches_host",
          ContentSettingsPattern::FromString("*"),
          ContentSettingsPattern::FromString("[*.]bad.blocked.com"),
          "https://bad.blocked.com",
          true,
      },
      {
          "wildcard_source_and_does_not_match_domain",
          ContentSettingsPattern::FromString("*"),
          ContentSettingsPattern::FromString("[*.]blocked.com"),
          "https://allowed.com",
          false,
      },
      {
          "wildcard_source_and_does_not_match_host",
          ContentSettingsPattern::FromString("*"),
          ContentSettingsPattern::FromString("[*.]bad.blocked.com"),
          "https://good.blocked.com",
          false,
      },
      {
          "matches_both_domains",
          ContentSettingsPattern::FromString("blocked.com"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://blocked.com",
          true,
      },
      {
          "matches_both_hosts",
          ContentSettingsPattern::FromString("[*.]bad.blocked.com"),
          ContentSettingsPattern::FromString("[*.]bad.blocked.com"),
          "https://bad.blocked.com",
          true,
      },
      {
          "source_does_not_match_source_domain",
          ContentSettingsPattern::FromString("another.com"),
          ContentSettingsPattern::FromString("blocked.com"),
          "https://blocked.com",
          false,
      },
      {
          "source_does_not_match_source_host",
          ContentSettingsPattern::FromString("[*.]other.blocked.com"),
          ContentSettingsPattern::FromString("[*.]bad.blocked.com"),
          "https://bad.blocked.com",
          false,
      },
      {
          "source_does_not_match_source_host_wildcard",
          ContentSettingsPattern::FromString("[*.]other.blocked.com"),
          ContentSettingsPattern::FromString("*"),
          "https://bad.blocked.com",
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    SafetyList::Patterns list;
    list.push_back({test_case.source_pattern, test_case.destination_pattern});
    SCOPED_TRACE(test_case.desc);
    EXPECT_EQ(test_case.expected,
              SafetyList(list).ContainsPatternMatchingSelfNavigation(
                  GURL(test_case.url)));
  }
}

}  // namespace

}  // namespace actor
