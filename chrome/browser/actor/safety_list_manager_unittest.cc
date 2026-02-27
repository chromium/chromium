// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list_manager.h"

#include <cstddef>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {
namespace {

using Decision = SafetyListManager::Decision;
using ParseResult = SafetyListManager::ParseResult;

class SafetyListManagerTest : public ::testing::Test,
                              public ::testing::WithParamInterface<bool> {
 public:
  SafetyListManagerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {kGlicCrossOriginNavigationGating,
             {{
                 {"include_hardcoded_block_list_entries",
                  initialize_hardcoded_blocklist() ? "true" : "false"},
             }}},
        },
        /*disabled_features=*/{});
    manager_ = SafetyListManager::CreateForTesting();
  }

 protected:
  SafetyListManager& manager() { return *manager_; }

  bool initialize_hardcoded_blocklist() { return GetParam(); }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // `manager_` is made optional to delay its construction until after
  // `scoped_feature_list_` has been initialized. This ensures that the Finch
  // feature flags are correctly set when `SafetyListManager`'s constructor
  // is called.
  std::optional<SafetyListManager> manager_;
};

TEST_P(SafetyListManagerTest, InitializeWithHardcodedLists) {
  EXPECT_EQ(
      manager().Find(GURL("https://anything.com"),
                     GURL("https://www.googleplex.com")),
      initialize_hardcoded_blocklist() ? Decision::kBlock : Decision::kNone);
  EXPECT_EQ(
      manager().Find(GURL("https://anything.com"),
                     GURL("https://corp.google.com")),
      initialize_hardcoded_blocklist() ? Decision::kBlock : Decision::kNone);
}

// Hardcoded domains should behave properly even if parts of the input were
// invalid.
TEST_P(SafetyListManagerTest, ParseSafetyLists_PreservesHardcodedLists) {
  const struct {
    std::string_view description;
    std::string_view json;
  } kTestCases[] = {
      {
          "Invalid top-level dict",
          R"json([])json",
      },
      {
          "Invalid blocklist",
          R"json({
            "navigation_allowed": [
              { "from": "foo.com", "to": "[*.]bar.com" }
            ],
            "navigation_blocked": {}
          })json",
      },
      {
          "Invalid allowlist",
          R"json({
            "navigation_allowed": {},
            "navigation_blocked": [
              { "from": "blocked.com", "to": "not-allowed.com"}
            ]
          })json",
      },
      {
          "Both lists valid",
          R"json({
            "navigation_allowed": [
              { "from": "foo.com", "to": "[*.]bar.com" },
            ],
            "navigation_blocked": [
              { "from": "blocked.com", "to": "not-allowed.com"}
            ]
          })json",
      },
      {
          "Empty dict is ok",
          R"json({})json",
      },
      {
          "Both lists valid, blocklist is empty",
          R"json({
            "navigation_allowed": [
              { "from": "foo.com", "to": "[*.]bar.com" },
            ]
          })json",
      },
      {
          "Both lists valid, allowlist is empty",
          R"json({
            "navigation_blocked": [
              { "from": "blocked.com", "to": "not-allowed.com"}
            ]
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    manager().ParseSafetyLists(test_case.json);
    EXPECT_EQ(
        manager().Find(GURL("https://anything.com"),
                       GURL("https://www.googleplex.com")),
        initialize_hardcoded_blocklist() ? Decision::kBlock : Decision::kNone);
    EXPECT_EQ(
        manager().Find(GURL("https://anything.com"),
                       GURL("https://corp.google.com")),
        initialize_hardcoded_blocklist() ? Decision::kBlock : Decision::kNone);
  }
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_Validity) {
  struct InvalidListTestCase {
    std::string_view desc;
    std::string_view json;
    ParseResult expected_allowed;
    ParseResult expected_blocked;
    size_t expected_allowed_count;
    size_t expected_blocked_count;
  } kTestCases[] = {
      {
          "not_dictionary",
          R"json({
            "navigation_allowed": [ "{}" ],
            "navigation_blocked": [ "{}" ]
          })json",
          ParseResult::kJsonListValueNotADictionary,
          ParseResult::kJsonListValueNotADictionary,
          0u,
          0u,
      },
      {
          "top_level_not_dictionary",
          R"json([])json",
          ParseResult::kInvalidJson,
          ParseResult::kInvalidJson,
          0u,
          0u,
      },
      {
          "key_value_not_a_list",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": 123
          })json",
          ParseResult::kJsonKeyValueNotAList,
          ParseResult::kJsonKeyValueNotAList,
          0u,
          0u,
      },
      {
          "to_field_missing",
          R"json({
            "navigation_allowed": [ { "from": "a.com" } ],
            "navigation_blocked": [ { "from": "a.com" } ]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidToField,
          0u,
          0u,
      },
      {
          "from_field_missing",
          R"json({
            "navigation_allowed": [ { "to": "b.com" } ],
            "navigation_blocked": [ { "to": "b.com" } ]
          })json",
          ParseResult::kInvalidFromField,
          ParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "to_field_not_string",
          R"json({
            "navigation_allowed": [{"from": "a.com", "to": 123}],
            "navigation_blocked": [{"from": "a.com", "to": 123}]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidToField,
          0u,
          0u,
      },
      {
          "from_field_not_string",
          R"json({
            "navigation_allowed": [{ "from": 123, "to": "b.com" }],
            "navigation_blocked": [{ "from": 123, "to": "b.com" }]
          })json",
          ParseResult::kInvalidFromField,
          ParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "to_value_not_valid_pattern",
          R"json({
            "navigation_allowed": [{ "from": "a.com", "to": "b.*.com" }],
            "navigation_blocked": [{ "from": "a.com", "to": "b.*.com" }]
          })json",
          ParseResult::kInvalidToUrlPattern,
          ParseResult::kInvalidToUrlPattern,
          0u,
          0u,
      },
      {
          "from_value_not_valid_pattern",
          R"json({
            "navigation_allowed": [{ "from": "a.*.com", "to": "b.com" }],
            "navigation_blocked": [{ "from": "a.*.com", "to": "b.com" }]
          })json",
          ParseResult::kInvalidFromUrlPattern,
          ParseResult::kInvalidFromUrlPattern,
          0u,
          0u,
      },
      {
          "empty_lists",
          R"json({ "navigation_allowed": [], "navigation_blocked": [] })json",
          ParseResult::kSuccess,
          ParseResult::kSuccess,
          0u,
          0u,
      },
      {
          "allowed_valid_blocked_invalid_field",
          R"json({
            "navigation_allowed": [{ "from": "a.com", "to": "b.com" }],
            "navigation_blocked": [{ "from": "a.com" }]
          })json",
          ParseResult::kSuccess,
          ParseResult::kInvalidToField,
          1u,
          0u,
      },
      {
          "allowed_invalid_type_blocked_valid",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": [{ "from": "a.com", "to": "b.com" }]
          })json",
          ParseResult::kJsonKeyValueNotAList,
          ParseResult::kSuccess,
          0u,
          1u,
      },
      {
          "mixed_invalid_fields",
          R"json({
            "navigation_allowed": [{ "from": "a.com" }],
            "navigation_blocked": [{ "to": "b.com" }]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "mixed_invalid_types",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": [ "{}" ]
          })json",
          ParseResult::kJsonKeyValueNotAList,
          ParseResult::kJsonListValueNotADictionary,
          0u,
          0u,
      },
      {
          "multiple_valid",
          R"json({
            "navigation_allowed": [
              { "from": "a.com", "to": "b.com" },
              { "from": "c.com", "to": "d.com" }
            ],
            "navigation_blocked": [
              { "from": "e.com", "to": "f.com" },
              { "from": "g.com", "to": "h.com" }
            ]
          })json",
          ParseResult::kSuccess,
          ParseResult::kSuccess,
          2u,
          2u,
      },
      {
          "multiple_invalid",
          R"json({
            "navigation_allowed": [
              { "from": "a.com" },
              { "to": "d.com" }
            ],
            "navigation_blocked": [
              { "from": "e.com" },
              { "to": "h.com" }
            ]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidToField,
          0u,
          0u,
      },
      {
          "mixed_valid_invalid",
          R"json({
            "navigation_allowed": [
              { "from": "a.com", "to": "b.com" },
              { "from": "c.com" }
            ],
            "navigation_blocked": [
              { "from": "e.com", "to": "f.com" },
              { "to": "h.com" }
            ]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "allowed_partially_valid",
          R"json({
            "navigation_allowed": [
              { "from": "a.com", "to": "b.com" },
              { "from": "c.com" }
            ],
            "navigation_blocked": [
              { "from": "e.com", "to": "f.com" }
            ]
          })json",
          ParseResult::kInvalidToField,
          ParseResult::kSuccess,
          0u,
          1u,
      },
  };

  for (const auto& test_case : kTestCases) {
    // Reset the manager to ensure a clean state for each test case.
    manager() = SafetyListManager::CreateForTesting();
    SCOPED_TRACE(test_case.desc);
    base::HistogramTester histogram_tester;
    manager().ParseSafetyLists(test_case.json);

    histogram_tester.ExpectUniqueSample(
        "Actor.SafetyListParseResult.NavigationAllowed",
        test_case.expected_allowed, 1);
    histogram_tester.ExpectUniqueSample(
        "Actor.SafetyListParseResult.NavigationBlocked",
        test_case.expected_blocked, 1);
  }
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_ValidPatterns) {
  manager().ParseSafetyLists(R"json(
    {
      "navigation_allowed": [
        { "from": "[*.]google.com", "to": "youtube.com" },
        { "from": "foo.com", "to": "[*.]bar.com" },
        { "from": "https://a.com:8080", "to": "https://*" },
        { "from": "127.0.0.1", "to": "*" }
      ],
      "navigation_blocked": [
        { "from": "blocked.com", "to": "not-allowed.com"}
      ]
    }
  )json");
  EXPECT_EQ(manager().Find(GURL("https://www.google.com"),
                           GURL("https://youtube.com")),
            Decision::kAllow);
  EXPECT_EQ(manager().Find(GURL("http://foo.com"), GURL("https://sub.bar.com")),
            Decision::kAllow);
  EXPECT_EQ(manager().Find(GURL("https://a.com:8080"), GURL("http://b.com")),
            Decision::kNone);
  EXPECT_EQ(manager().Find(GURL("https://a.com:8080"), GURL("https://b.com")),
            Decision::kAllow);
  EXPECT_EQ(manager().Find(GURL("http://127.0.0.1"), GURL("http://localhost")),
            Decision::kAllow);

  EXPECT_EQ(manager().Find(GURL("https://blocked.com"),
                           GURL("https://not-allowed.com")),
            Decision::kBlock);
  histogram_tester_.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationAllowed", ParseResult::kSuccess,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationBlocked", ParseResult::kSuccess,
      1);
}

TEST_P(SafetyListManagerTest, ParseBlockLists_MultipleParses) {
  manager().ParseSafetyLists(R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]google.com", "to": "youtube.com" },
        { "from": "foo.com", "to": "[*.]bar.com" }
      ]
    }
  )json");
  EXPECT_EQ(manager().Find(GURL("https://www.google.com"),
                           GURL("https://youtube.com")),
            Decision::kBlock);
  EXPECT_EQ(manager().Find(GURL("http://foo.com"), GURL("https://sub.bar.com")),
            Decision::kBlock);

  manager().ParseSafetyLists(R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]yahoo.com", "to": "vimeo.com" },
        { "from": "bar.com", "to": "[*.]foo.com" }
      ]
    }
  )json");
  EXPECT_EQ(manager().Find(GURL("https://www.google.com"),
                           GURL("https://youtube.com")),
            Decision::kNone);
  EXPECT_EQ(manager().Find(GURL("http://foo.com"), GURL("https://sub.bar.com")),
            Decision::kNone);
  EXPECT_EQ(
      manager().Find(GURL("https://www.yahoo.com"), GURL("https://vimeo.com")),
      Decision::kBlock);
  EXPECT_EQ(manager().Find(GURL("http://bar.com"), GURL("https://sub.foo.com")),
            Decision::kBlock);
  histogram_tester_.ExpectBucketCount(
      "Actor.SafetyListParseResult.NavigationAllowed", ParseResult::kSuccess,
      2);
  histogram_tester_.ExpectBucketCount(
      "Actor.SafetyListParseResult.NavigationBlocked", ParseResult::kSuccess,
      2);
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_BlockedListInvalid) {
  manager().ParseSafetyLists(R"json(
    {
      "navigation_allowed": [],
      "navigation_blocked": [
        { "from": "a.*.com", "to": "b.com" }
      ]
    }
  )json");
  histogram_tester_.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationBlocked",
      ParseResult::kInvalidFromUrlPattern, 1);
  histogram_tester_.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationAllowed", ParseResult::kSuccess,
      1);
}

TEST_P(SafetyListManagerTest, Find) {
  const struct {
    std::string_view desc;
    std::string_view json;
    std::string_view from_url;
    std::string_view to_url;
    Decision expected;
  } kTestCases[] = {
      {
          "source wildcard subdomain match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "[*.]a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://sub.a.com",
          "https://b.com",
          Decision::kBlock,
      },
      {
          "source wildcard root match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "[*.]a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kBlock,
      },
      {
          "source wildcard match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "*", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kBlock,
      },
      {
          "source wildcard subdomain mismatch",
          R"json(
            {
              "navigation_blocked": [
                { "from": "[*.]a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://other.com",
          "https://b.com",
          Decision::kNone,
      },
      {
          "destination wildcard subdomain match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "[*.]b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://sub.b.com",
          Decision::kBlock,
      },
      {
          "destination wildcard match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "*" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kBlock,
      },
      {
          "destination wildcard root match",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "[*.]b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kBlock,
      },
      {
          "destination wildcard subdomain mismatch",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "[*.]b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://other.com",
          Decision::kNone,
      },
      {
          "both mismatch",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://c.com",
          "https://d.com",
          Decision::kNone,
      },
      {
          "multiple entries, single list, match one",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" },
                { "from": "c.com", "to": "d.com" }
              ]
            }
          )json",
          "https://c.com",
          "https://d.com",
          Decision::kBlock,
      },
      {
          "multiple entries, both lists, match one",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" },
                { "from": "c.com", "to": "d.com" }
              ],
              "navigation_allowed": [
                { "from": "e.com", "to": "f.com" },
                { "from": "g.com", "to": "h.com" }
              ]
            }
          )json",
          "https://e.com",
          "https://f.com",
          Decision::kAllow,
      },
      {
          "overlapping entries, specific allow, generic block",
          R"json(
            {
              "navigation_blocked": [
                { "from": "*", "to": "b.com" }
              ],
              "navigation_allowed": [
                { "from": "a.com", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kAllow,
      },
      {
          "overlapping entries, generic allow, specific block",
          R"json(
            {
              "navigation_blocked": [
                { "from": "a.com", "to": "b.com" }
              ],
              "navigation_allowed": [
                { "from": "*", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kBlock,
      },
      {
          "overlapping entries, equal specificities -> blocklist wins",
          R"json(
            {
              "navigation_blocked": [
                { "from": "*", "to": "b.com" }
              ],
              "navigation_allowed": [
                { "from": "*", "to": "b.com" }
              ]
            }
          )json",
          "https://a.com",
          "https://b.com",
          Decision::kBlock,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    manager().ParseSafetyLists(test_case.json);
    EXPECT_EQ(manager().Find(GURL(test_case.from_url), GURL(test_case.to_url)),
              test_case.expected);
  }
}

INSTANTIATE_TEST_SUITE_P(All, SafetyListManagerTest, testing::Bool());

}  // namespace
}  // namespace actor
