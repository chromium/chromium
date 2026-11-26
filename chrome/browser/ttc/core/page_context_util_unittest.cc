// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/page_context_util.h"

#include <optional>
#include <string_view>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "components/actor/core/safety_list_manager.h"
#include "components/ttc/app/public/page_context_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {
namespace {

class PageContextUtilTest : public testing::Test {
 protected:
  void TearDown() override {
    // SafetyListManager is a process-wide singleton; clear any lists a test
    // set.
    actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    "{}");
  }

  std::optional<PageRestrictionReason> GetRestriction(
      const GURL& url,
      const actor::EnterprisePolicyChecker* enterprise_policy_checker =
          nullptr) {
    base::test::TestFuture<std::optional<PageRestrictionReason>> restriction;
    GetPageRestrictionForUrl(url, enterprise_policy_checker,
                             restriction.GetCallback());
    return restriction.Get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// HTTP(S) pages and the New Tab Page are supported.
class SupportedUrlTest : public PageContextUtilTest,
                         public testing::WithParamInterface<std::string_view> {
};

TEST_P(SupportedUrlTest, IsSupported) {
  const GURL url(GetParam());
  EXPECT_EQ(GetPageRestrictionForUrlSync(url, nullptr), std::nullopt);
  EXPECT_EQ(GetRestriction(url), std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SupportedUrlTest,
                         testing::Values("http://example.com/",
                                         "https://example.com/path?q=1#ref",
                                         "http://127.0.0.1:8080/",
                                         "chrome://newtab/",
                                         "chrome://new-tab-page/",
                                         "chrome://new-tab-page-third-party/"));

// All other schemes, including non-NTP chrome:// pages, are unsupported.
class UnsupportedUrlTest
    : public PageContextUtilTest,
      public testing::WithParamInterface<std::string_view> {};

TEST_P(UnsupportedUrlTest, IsNotSupported) {
  const GURL url(GetParam());
  EXPECT_EQ(GetPageRestrictionForUrlSync(url, nullptr),
            PageRestrictionReason::kUnsupportedScheme);
  EXPECT_EQ(GetRestriction(url), PageRestrictionReason::kUnsupportedScheme);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UnsupportedUrlTest,
    testing::Values(
        "chrome://settings/",
        "chrome://version/",
        "chrome://newtab-not-really/",
        "chrome-untrusted://new-tab-page/",
        "file:///tmp/index.html",
        "javascript:alert(1)",
        "about:blank",
        "data:text/html,hello",
        "blob:https://example.com/0b1c2d3e-4f50-6172-8394-a5b6c7d8e9f0",
        "chrome-extension://abcdefghijklmnopabcdefghijklmnop/page.html",
        "ftp://example.com/",
        "",
        "not a url"));

// HTTP(S) URLs for which the actor safety lists block a navigation from the URL
// to itself are unsupported.
TEST_F(PageContextUtilTest, SafetyLists) {
  actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                  R"json(
    {
      "navigation_allowed": [
        { "from": "*", "to": "[*.]allowed.blocked.com" }
      ],
      "navigation_blocked": [
        { "from": "*", "to": "[*.]blocked.com" },
        { "from": "[*.]other.com", "to": "[*.]cross-origin-only.com" }
      ]
    }
  )json");

  EXPECT_EQ(
      GetPageRestrictionForUrlSync(GURL("https://blocked.com/path"), nullptr),
      std::nullopt);
  EXPECT_EQ(GetRestriction(GURL("https://blocked.com/path")),
            PageRestrictionReason::kSafetyPolicy);
  EXPECT_EQ(GetRestriction(GURL("https://allowed.blocked.com/path")),
            std::nullopt);
  EXPECT_EQ(GetRestriction(GURL("https://cross-origin-only.com/path")),
            std::nullopt);
  EXPECT_EQ(GetRestriction(GURL("https://unlisted.com/path")), std::nullopt);
}

// URLs for which enterprise policy explicitly blocks actuation are unsupported.
TEST_F(PageContextUtilTest, EnterprisePolicy) {
  const GURL allowed_url("https://allowed.example.com/path");
  const GURL blocked_url("https://blocked.example.com/path");

  actor::MockPolicyChecker policy_checker(
      actor::EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked);

  EXPECT_EQ(GetPageRestrictionForUrlSync(blocked_url, &policy_checker),
            PageRestrictionReason::kEnterprisePolicy);
  EXPECT_EQ(GetRestriction(blocked_url, &policy_checker),
            PageRestrictionReason::kEnterprisePolicy);

  policy_checker.set_reason(
      actor::EnterprisePolicyChecker::UrlBlockReason::kExplicitlyAllowed);
  EXPECT_EQ(GetPageRestrictionForUrlSync(allowed_url, &policy_checker),
            std::nullopt);
  EXPECT_EQ(GetRestriction(allowed_url, &policy_checker), std::nullopt);

  policy_checker.set_reason(
      actor::EnterprisePolicyChecker::UrlBlockReason::kNotBlocked);
  EXPECT_EQ(GetRestriction(allowed_url, &policy_checker), std::nullopt);

  // Unsupported schemes remain unsupported even if enterprise policy allows
  // them.
  EXPECT_EQ(GetRestriction(GURL("chrome://settings/"), &policy_checker),
            PageRestrictionReason::kUnsupportedScheme);
}

// Enterprise policy takes precedence over safety lists.
TEST_F(PageContextUtilTest, EnterprisePolicyAndSafetyListInteraction) {
  actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                  R"json(
    {
      "navigation_blocked": [
        { "from": "*", "to": "[*.]safetylist-blocked.com" }
      ]
    }
  )json");

  const GURL safety_blocked_url("https://safetylist-blocked.com/path");
  const GURL safety_allowed_url("https://safetylist-allowed.com/path");

  actor::MockPolicyChecker policy_checker(
      actor::EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked);

  // Enterprise policy block overrides safety list allow.
  EXPECT_EQ(GetRestriction(safety_allowed_url, &policy_checker),
            PageRestrictionReason::kEnterprisePolicy);

  // Enterprise policy block also blocks safety list blocked URLs.
  EXPECT_EQ(GetRestriction(safety_blocked_url, &policy_checker),
            PageRestrictionReason::kEnterprisePolicy);

  // When enterprise policy does not block, safety list blocks still apply.
  policy_checker.set_reason(
      actor::EnterprisePolicyChecker::UrlBlockReason::kNotBlocked);
  EXPECT_EQ(GetRestriction(safety_blocked_url, &policy_checker),
            PageRestrictionReason::kSafetyPolicy);
  EXPECT_EQ(GetRestriction(safety_allowed_url, &policy_checker), std::nullopt);
  EXPECT_EQ(GetRestriction(GURL("chrome://settings/"), &policy_checker),
            PageRestrictionReason::kUnsupportedScheme);
}

}  // namespace
}  // namespace ttc
