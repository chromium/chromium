// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list_manager.h"

#include <cstddef>
#include <memory>

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
    manager_.emplace();
  }

 protected:
  SafetyListManager& manager() { return *manager_; }

  bool initialize_hardcoded_blocklist() { return GetParam(); }
  size_t EmptyOrOnlyHardcodedBlocklist() {
    return initialize_hardcoded_blocklist() ? 2u : 0u;
  }

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

TEST_P(SafetyListManagerTest, ParseSafetyLists_MalformedJson) {
  manager().ParseSafetyLists(R"json(not a json)json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(),
            EmptyOrOnlyHardcodedBlocklist());
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_NotADictionary) {
  manager().ParseSafetyLists(R"json("[]")json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(),
            EmptyOrOnlyHardcodedBlocklist());
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_EmptyLists) {
  manager().ParseSafetyLists(
      R"json({ "navigation_allowed": [], "navigation_blocked": [] })json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(),
            EmptyOrOnlyHardcodedBlocklist());
  EXPECT_FALSE(manager().get_allowed_list().ContainsUrlPair(
      GURL("https://a.com"), GURL("https://b.com")));
  EXPECT_FALSE(manager().get_blocked_list().ContainsUrlPair(
      GURL("https://a.com"), GURL("https://b.com")));
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_ListWithInvalidEntries) {
  manager().ParseSafetyLists(R"json(
    {
      "navigation_allowed": [
        "string_instead_of_dict",
        { "from_no_to": "a.com" },
        { "to_no_from": "b.com" },
        { "from": 123, "to": 456 },
        { "from": "a.com", "to": "b.com" }
      ]
    }
  )json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(),
            EmptyOrOnlyHardcodedBlocklist());
}

TEST_P(SafetyListManagerTest, ParseSafetyLists_InvalidPatterns) {
  manager().ParseSafetyLists(R"json(
    {
      "navigation_allowed": [
        { "from": "b.com", "to": "[" },
        { "from": "a.com", "to": "b.com" }
      ]
    }
  )json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(),
            EmptyOrOnlyHardcodedBlocklist());
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
}

INSTANTIATE_TEST_SUITE_P(All, SafetyListManagerTest, testing::Bool());

}  // namespace
}  // namespace actor
