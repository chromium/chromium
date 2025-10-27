// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list_manager.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {
namespace {

class SafetyListManagerTest : public ::testing::Test {
 public:
  SafetyListManagerTest() = default;

 protected:
  SafetyListManager& manager() { return manager_; }

 private:
  SafetyListManager manager_;
};

TEST_F(SafetyListManagerTest, ParseSafetyLists_MalformedJson) {
  manager().ParseSafetyLists(R"json(not a json)json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(), 0u);
}

TEST_F(SafetyListManagerTest, ParseSafetyLists_NotADictionary) {
  manager().ParseSafetyLists(R"json("[]")json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(), 0u);
}

TEST_F(SafetyListManagerTest, ParseSafetyLists_EmptyLists) {
  manager().ParseSafetyLists(
      R"json({ "navigation_allowed": [], "navigation_blocked": [] })json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(), 0u);
  EXPECT_FALSE(manager().get_allowed_list().ContainsUrlPair(
      GURL("https://a.com"), GURL("https://b.com")));
  EXPECT_FALSE(manager().get_blocked_list().ContainsUrlPair(
      GURL("https://a.com"), GURL("https://b.com")));
}

TEST_F(SafetyListManagerTest, ParseSafetyLists_ListWithInvalidEntries) {
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
  EXPECT_EQ(manager().get_blocked_list().size(), 0u);
}

TEST_F(SafetyListManagerTest, ParseSafetyLists_InvalidPatterns) {
  manager().ParseSafetyLists(R"json(
    {
      "navigation_allowed": [
        { "from": "b.com", "to": "[" },
        { "from": "a.com", "to": "b.com" }
      ]
    }
  )json");
  EXPECT_EQ(manager().get_allowed_list().size(), 0u);
  EXPECT_EQ(manager().get_blocked_list().size(), 0u);
}

TEST_F(SafetyListManagerTest, ParseSafetyLists_ValidPatterns) {
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
  EXPECT_EQ(blocked_list.size(), 1u);
  EXPECT_TRUE(blocked_list.ContainsUrlPair(GURL("https://blocked.com"),
                                           GURL("https://not-allowed.com")));
}

}  // namespace
}  // namespace actor
