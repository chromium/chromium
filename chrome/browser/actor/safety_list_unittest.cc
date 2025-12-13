// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list.h"

#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

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
    size_t expected_size;
  } kTestCases[] = {
      {
          "empty_list",
          R"json([])json",
          0u,
      },
      {
          "invalid_structure",
          R"json([{"from": "a.com", "to": "b.com"},[]])json",
          0u,
      },
      {
          "missing_from",
          R"json([{"from": "a.com", "to": "b.com"},{"to": "d.com"}])json",
          0u,
      },
      {
          "missing_to",
          R"json([{"from": "a.com", "to": "b.com"},{"from": "c.com"}])json",
          0u,
      },
      {
          "invalid_pattern",
          R"json([
            {"from": "a.com", "to": "b.com"},{"from": "c.com", "to": "http://"}
          ])json",
          0u,
      },
      {
          "valid_patterns",
          R"json([
            {"from": "a.com", "to": "b.com"},{"from": "c.com", "to": "d.com"}
          ])json",
          2u,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    EXPECT_EQ(test_case.expected_size,
              SafetyList::ParsePatternListFromJson(
                  base::test::ParseJsonList(test_case.json,
                                            base::JSON_ALLOW_TRAILING_COMMAS))
                  .size());
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

}  // namespace

}  // namespace actor
