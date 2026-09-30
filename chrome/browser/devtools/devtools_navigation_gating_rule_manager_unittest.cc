// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_gating_rule_manager.h"

#include <memory>
#include <string_view>

#include "components/origin_gating/core/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

bool IsNavigationAllowed(const DevToolsNavigationGatingRuleManager& manager,
                         const GURL& url) {
  return manager.EvaluateRules(
             origin_gating::GateableEvent(origin_gating::NavigationRequestEvent{
                 .source = GURL(), .destination = url})) ==
         origin_gating::Decision::kAllowed;
}

TEST(DevToolsNavigationGatingRuleManagerTest, EmptyRulesAllowsAll) {
  auto manager = DevToolsNavigationGatingRuleManager::CreateForTesting("{}");
  EXPECT_FALSE(manager->MayBlockNavigation());
  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("http://a.com")));
  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://b.com/foo")));
}

TEST(DevToolsNavigationGatingRuleManagerTest, AllowlistRestrictsNavigations) {
  auto manager = DevToolsNavigationGatingRuleManager::CreateForTesting(
      R"({"allowlist": ["https://a.com", "http://b.com"]})");
  EXPECT_TRUE(manager->MayBlockNavigation());

  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://a.com")));
  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://a.com/foo")));
  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("http://b.com:80/bar")));

  EXPECT_FALSE(
      IsNavigationAllowed(*manager, GURL("http://a.com")));  // Scheme mismatch
  EXPECT_FALSE(
      IsNavigationAllowed(*manager, GURL("https://c.com")));  // Host mismatch
}

TEST(DevToolsNavigationGatingRuleManagerTest, EmptyAllowlistBlocksAll) {
  auto manager = DevToolsNavigationGatingRuleManager::CreateForTesting(
      R"({"allowlist": []})");
  EXPECT_TRUE(manager->MayBlockNavigation());

  EXPECT_FALSE(IsNavigationAllowed(*manager, GURL("https://a.com")));
  EXPECT_FALSE(IsNavigationAllowed(*manager, GURL("https://b.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest, AllowlistWildcardDomain) {
  auto manager = DevToolsNavigationGatingRuleManager::CreateForTesting(
      R"({"allowlist": ["https://[*.]example.com"]})");

  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://example.com")));
  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://sub.example.com")));
  EXPECT_TRUE(
      IsNavigationAllowed(*manager, GURL("https://deep.sub.example.com/foo")));

  EXPECT_FALSE(IsNavigationAllowed(*manager, GURL("http://example.com")));
  EXPECT_FALSE(IsNavigationAllowed(*manager, GURL("https://notexample.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest, BlocklistRestrictsNavigations) {
  auto manager = DevToolsNavigationGatingRuleManager::CreateForTesting(
      R"({"blocklist": ["https://blockedsite.com"]})");

  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://allowedsite.com")));
  EXPECT_FALSE(IsNavigationAllowed(*manager, GURL("https://blockedsite.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest,
     SpecificityBlocklistOverridesAllowlist) {
  // blocklist pattern is more specific
  auto manager = DevToolsNavigationGatingRuleManager::CreateForTesting(R"({
        "allowlist": ["https://[*.]blockedsite.com", "https://allowedsite.com"],
        "blocklist": ["https://sub.blockedsite.com"]
      })");

  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://allowedsite.com")));
  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://blockedsite.com")));
  EXPECT_TRUE(
      IsNavigationAllowed(*manager, GURL("https://another.blockedsite.com")));
  EXPECT_FALSE(
      IsNavigationAllowed(*manager, GURL("https://sub.blockedsite.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest,
     SpecificityAllowlistOverridesBlocklist) {
  // allowlist pattern is more specific
  auto manager = DevToolsNavigationGatingRuleManager::CreateForTesting(R"({
        "allowlist": ["https://allow.blockedsite.com"],
        "blocklist": ["https://[*.]blockedsite.com"]
      })");

  EXPECT_FALSE(IsNavigationAllowed(*manager, GURL("https://blockedsite.com")));
  EXPECT_FALSE(
      IsNavigationAllowed(*manager, GURL("https://sub.blockedsite.com")));
  EXPECT_TRUE(
      IsNavigationAllowed(*manager, GURL("https://allow.blockedsite.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest,
     MalformedJsonRulesFallbackToEmpty) {
  auto manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting("invalid-json");
  EXPECT_FALSE(manager->MayBlockNavigation());
  EXPECT_TRUE(IsNavigationAllowed(*manager, GURL("https://any.com")));
}

}  // namespace
