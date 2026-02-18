// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list_manager.h"

#include <cstddef>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {
namespace {

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
  size_t EmptyOrOnlyHardcodedBlocklist() {
    return initialize_hardcoded_blocklist() ? 2u : 0u;
  }

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
  // The constructor should have already loaded the hardcoded lists.
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);

  const SafetyList& blocked_list = manager().get_blocked_list();
  EXPECT_EQ(blocked_list.size(), EmptyOrOnlyHardcodedBlocklist());

  if (initialize_hardcoded_blocklist()) {
    EXPECT_TRUE(blocked_list.ContainsUrlPair(
        GURL("https://anything.com"), GURL("https://www.googleplex.com")));
    EXPECT_TRUE(blocked_list.ContainsUrlPair(GURL("https://anything.com"),
                                             GURL("https://corp.google.com")));
  }
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_Validity) {
  struct InvalidListTestCase {
    std::string_view desc;
    std::string_view json;
    SafetyListParseResult expected_allowed;
    SafetyListParseResult expected_blocked;
    size_t expected_allowed_count;
    size_t expected_blocked_count;
  } kTestCases[] = {
      {
          "not_dictionary",
          R"json({
            "navigation_allowed": [ "{}" ],
            "navigation_blocked": [ "{}" ]
          })json",
          SafetyListParseResult::kJsonListValueNotADictionary,
          SafetyListParseResult::kJsonListValueNotADictionary,
          0u,
          0u,
      },
      {
          "top_level_not_dictionary",
          R"json([])json",
          SafetyListParseResult::kInvalidJson,
          SafetyListParseResult::kInvalidJson,
          0u,
          0u,
      },
      {
          "key_value_not_a_list",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": 123
          })json",
          SafetyListParseResult::kJsonKeyValueNotAList,
          SafetyListParseResult::kJsonKeyValueNotAList,
          0u,
          0u,
      },
      {
          "to_field_missing",
          R"json({
            "navigation_allowed": [ { "from": "a.com" } ],
            "navigation_blocked": [ { "from": "a.com" } ]
          })json",
          SafetyListParseResult::kInvalidToField,
          SafetyListParseResult::kInvalidToField,
          0u,
          0u,
      },
      {
          "from_field_missing",
          R"json({
            "navigation_allowed": [ { "to": "b.com" } ],
            "navigation_blocked": [ { "to": "b.com" } ]
          })json",
          SafetyListParseResult::kInvalidFromField,
          SafetyListParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "to_field_not_string",
          R"json({
            "navigation_allowed": [{"from": "a.com", "to": 123}],
            "navigation_blocked": [{"from": "a.com", "to": 123}]
          })json",
          SafetyListParseResult::kInvalidToField,
          SafetyListParseResult::kInvalidToField,
          0u,
          0u,
      },
      {
          "from_field_not_string",
          R"json({
            "navigation_allowed": [{ "from": 123, "to": "b.com" }],
            "navigation_blocked": [{ "from": 123, "to": "b.com" }]
          })json",
          SafetyListParseResult::kInvalidFromField,
          SafetyListParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "to_value_not_valid_pattern",
          R"json({
            "navigation_allowed": [{ "from": "a.com", "to": "b.*.com" }],
            "navigation_blocked": [{ "from": "a.com", "to": "b.*.com" }]
          })json",
          SafetyListParseResult::kInvalidToUrlPattern,
          SafetyListParseResult::kInvalidToUrlPattern,
          0u,
          0u,
      },
      {
          "from_value_not_valid_pattern",
          R"json({
            "navigation_allowed": [{ "from": "a.*.com", "to": "b.com" }],
            "navigation_blocked": [{ "from": "a.*.com", "to": "b.com" }]
          })json",
          SafetyListParseResult::kInvalidFromUrlPattern,
          SafetyListParseResult::kInvalidFromUrlPattern,
          0u,
          0u,
      },
      {
          "empty_lists",
          R"json({ "navigation_allowed": [], "navigation_blocked": [] })json",
          SafetyListParseResult::kSuccess,
          SafetyListParseResult::kSuccess,
          0u,
          0u,
      },
      {
          "allowed_valid_blocked_invalid_field",
          R"json({
            "navigation_allowed": [{ "from": "a.com", "to": "b.com" }],
            "navigation_blocked": [{ "from": "a.com" }]
          })json",
          SafetyListParseResult::kSuccess,
          SafetyListParseResult::kInvalidToField,
          1u,
          0u,
      },
      {
          "allowed_invalid_type_blocked_valid",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": [{ "from": "a.com", "to": "b.com" }]
          })json",
          SafetyListParseResult::kJsonKeyValueNotAList,
          SafetyListParseResult::kSuccess,
          0u,
          1u,
      },
      {
          "mixed_invalid_fields",
          R"json({
            "navigation_allowed": [{ "from": "a.com" }],
            "navigation_blocked": [{ "to": "b.com" }]
          })json",
          SafetyListParseResult::kInvalidToField,
          SafetyListParseResult::kInvalidFromField,
          0u,
          0u,
      },
      {
          "mixed_invalid_types",
          R"json({
            "navigation_allowed": 123,
            "navigation_blocked": [ "{}" ]
          })json",
          SafetyListParseResult::kJsonKeyValueNotAList,
          SafetyListParseResult::kJsonListValueNotADictionary,
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
          SafetyListParseResult::kSuccess,
          SafetyListParseResult::kSuccess,
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
          SafetyListParseResult::kInvalidToField,
          SafetyListParseResult::kInvalidToField,
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
          SafetyListParseResult::kInvalidToField,
          SafetyListParseResult::kInvalidFromField,
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
          SafetyListParseResult::kInvalidToField,
          SafetyListParseResult::kSuccess,
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

    EXPECT_EQ(manager().get_allowed_list().size(),
              test_case.expected_allowed_count);
    EXPECT_EQ(
        manager().get_blocked_list().size(),
        test_case.expected_blocked_count + EmptyOrOnlyHardcodedBlocklist());

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
  const SafetyList& allowed_list = manager().get_allowed_list();
  EXPECT_EQ(allowed_list.size(), 4u);
  EXPECT_TRUE(allowed_list.ContainsUrlPair(GURL("https://www.google.com"),
                                           GURL("https://youtube.com")));
  EXPECT_TRUE(allowed_list.ContainsUrlPair(GURL("http://foo.com"),
                                           GURL("https://sub.bar.com")));
  EXPECT_FALSE(allowed_list.ContainsUrlPair(GURL("https://a.com:8080"),
                                            GURL("http://b.com")));
  EXPECT_TRUE(allowed_list.ContainsUrlPair(GURL("https://a.com:8080"),
                                           GURL("https://b.com")));
  EXPECT_TRUE(allowed_list.ContainsUrlPair(GURL("http://127.0.0.1"),
                                           GURL("http://localhost")));

  const SafetyList& blocked_list = manager().get_blocked_list();
  EXPECT_EQ(blocked_list.size(), initialize_hardcoded_blocklist() ? 3u : 1u);
  EXPECT_TRUE(blocked_list.ContainsUrlPair(GURL("https://blocked.com"),
                                           GURL("https://not-allowed.com")));
  histogram_tester_.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationAllowed",
      SafetyListParseResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationBlocked",
      SafetyListParseResult::kSuccess, 1);
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
  SafetyList blocked_list = manager().get_blocked_list();
  EXPECT_EQ(blocked_list.size(), initialize_hardcoded_blocklist() ? 4u : 2u);
  EXPECT_TRUE(blocked_list.ContainsUrlPair(GURL("https://www.google.com"),
                                           GURL("https://youtube.com")));
  EXPECT_TRUE(blocked_list.ContainsUrlPair(GURL("http://foo.com"),
                                           GURL("https://sub.bar.com")));

  manager().ParseSafetyLists(R"json(
    {
      "navigation_blocked": [
        { "from": "[*.]yahoo.com", "to": "vimeo.com" },
        { "from": "bar.com", "to": "[*.]foo.com" }
      ]
    }
  )json");
  blocked_list = manager().get_blocked_list();
  EXPECT_EQ(blocked_list.size(), initialize_hardcoded_blocklist() ? 4u : 2u);
  EXPECT_FALSE(blocked_list.ContainsUrlPair(GURL("https://www.google.com"),
                                            GURL("https://youtube.com")));
  EXPECT_FALSE(blocked_list.ContainsUrlPair(GURL("http://foo.com"),
                                            GURL("https://sub.bar.com")));
  EXPECT_TRUE(blocked_list.ContainsUrlPair(GURL("https://www.yahoo.com"),
                                           GURL("https://vimeo.com")));
  EXPECT_TRUE(blocked_list.ContainsUrlPair(GURL("http://bar.com"),
                                           GURL("https://sub.foo.com")));
  histogram_tester_.ExpectBucketCount(
      "Actor.SafetyListParseResult.NavigationAllowed",
      SafetyListParseResult::kSuccess, 2);
  histogram_tester_.ExpectBucketCount(
      "Actor.SafetyListParseResult.NavigationBlocked",
      SafetyListParseResult::kSuccess, 2);
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
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(),
            EmptyOrOnlyHardcodedBlocklist());
  histogram_tester_.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationBlocked",
      SafetyListParseResult::kInvalidFromUrlPattern, 1);
  histogram_tester_.ExpectUniqueSample(
      "Actor.SafetyListParseResult.NavigationAllowed",
      SafetyListParseResult::kSuccess, 1);
}

INSTANTIATE_TEST_SUITE_P(All, SafetyListManagerTest, testing::Bool());

}  // namespace
}  // namespace actor
