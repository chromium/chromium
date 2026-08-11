// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_gating_rule_manager.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class DevToolsNavigationGatingRuleManagerTest : public testing::Test {
 protected:
  bool IsNavigationAllowed(DevToolsNavigationGatingRuleManager& manager,
                           const GURL& url) {
    base::test::TestFuture<bool> future;
    manager.IsNavigationAllowed(url, future.GetCallback());
    return future.Get();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DevToolsNavigationGatingRuleManagerTest, EmptyRulesAllowsAll) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting("{}");
  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("http://a.com")));
  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://b.com/foo")));
}

TEST_F(DevToolsNavigationGatingRuleManagerTest, AllowlistRestrictsNavigations) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(
          R"({"allowlist": ["https://a.com", "http://b.com"]})");

  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://a.com")));
  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://a.com/foo")));
  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("http://b.com:80/bar")));

  EXPECT_FALSE(
      IsNavigationAllowed(manager, GURL("http://a.com")));  // Scheme mismatch
  EXPECT_FALSE(
      IsNavigationAllowed(manager, GURL("https://c.com")));  // Host mismatch
}

TEST_F(DevToolsNavigationGatingRuleManagerTest, EmptyAllowlistBlocksAll) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(
          R"({"allowlist": []})");

  EXPECT_FALSE(IsNavigationAllowed(manager, GURL("https://a.com")));
  EXPECT_FALSE(IsNavigationAllowed(manager, GURL("https://b.com")));
}

TEST_F(DevToolsNavigationGatingRuleManagerTest, AllowlistWildcardDomain) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(
          R"({"allowlist": ["https://[*.]example.com"]})");

  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://example.com")));
  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://sub.example.com")));
  EXPECT_TRUE(
      IsNavigationAllowed(manager, GURL("https://deep.sub.example.com/foo")));

  EXPECT_FALSE(IsNavigationAllowed(manager, GURL("http://example.com")));
  EXPECT_FALSE(IsNavigationAllowed(manager, GURL("https://notexample.com")));
}

TEST_F(DevToolsNavigationGatingRuleManagerTest, BlocklistRestrictsNavigations) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(
          R"({"blocklist": ["https://blockedsite.com"]})");

  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://allowedsite.com")));
  EXPECT_FALSE(IsNavigationAllowed(manager, GURL("https://blockedsite.com")));
}

TEST_F(DevToolsNavigationGatingRuleManagerTest,
       SpecificityBlocklistOverridesAllowlist) {
  // blocklist pattern is more specific
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(R"({
        "allowlist": ["https://[*.]blockedsite.com", "https://allowedsite.com"],
        "blocklist": ["https://sub.blockedsite.com"]
      })");

  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://allowedsite.com")));
  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://blockedsite.com")));
  EXPECT_TRUE(
      IsNavigationAllowed(manager, GURL("https://another.blockedsite.com")));
  EXPECT_FALSE(
      IsNavigationAllowed(manager, GURL("https://sub.blockedsite.com")));
}

TEST_F(DevToolsNavigationGatingRuleManagerTest,
       SpecificityAllowlistOverridesBlocklist) {
  // allowlist pattern is more specific
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(R"({
        "allowlist": ["https://allow.blockedsite.com"],
        "blocklist": ["https://[*.]blockedsite.com"]
      })");

  EXPECT_FALSE(IsNavigationAllowed(manager, GURL("https://blockedsite.com")));
  EXPECT_FALSE(
      IsNavigationAllowed(manager, GURL("https://sub.blockedsite.com")));
  EXPECT_TRUE(
      IsNavigationAllowed(manager, GURL("https://allow.blockedsite.com")));
}

TEST_F(DevToolsNavigationGatingRuleManagerTest,
       MalformedJsonRulesFallbackToEmpty) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting("invalid-json");
  EXPECT_TRUE(IsNavigationAllowed(manager, GURL("https://any.com")));
}

}  // namespace
