// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_allowlist_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

class AwSafeBrowsingAllowlistManagerTest : public testing::Test {
 protected:
  class TestAwSafeBrowsingAllowlistSetObserver
      : public AwSafeBrowsingAllowlistSetObserver {
   public:
    explicit TestAwSafeBrowsingAllowlistSetObserver(
        AwSafeBrowsingAllowlistManager* manager)
        : AwSafeBrowsingAllowlistSetObserver(manager) {}

    void OnSafeBrowsingAllowListSet() override { observer_triggered_ = true; }

    bool observer_triggered() { return observer_triggered_; }

   private:
    bool observer_triggered_ = false;
  };

  AwSafeBrowsingAllowlistManagerTest() = default;

  void SetUp() override {
    wm_ = std::make_unique<AwSafeBrowsingAllowlistManager>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  void TearDown() override { wm_.reset(); }

  void SetAllowlist(std::vector<std::string>&& allowlist, bool expected);

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<AwSafeBrowsingAllowlistManager> wm_;
};

void VerifyAllowlistCallback(bool expected, bool success) {
  EXPECT_EQ(expected, success);
}

void AwSafeBrowsingAllowlistManagerTest::SetAllowlist(
    std::vector<std::string>&& allowlist,
    bool expected) {
  wm_->SetAllowlistOnUIThread(
      std::move(allowlist), base::BindOnce(&VerifyAllowlistCallback, expected));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, WsSchemeCanBeAllowlisted) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("ws://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("wss://google.com")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, HttpSchemeCanBeAllowlisted) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("https://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com:80")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com:123")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com:443")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, WsSchemeCanBeAllowlistedExactMatch) {
  std::vector<std::string> allowlist;
  allowlist.push_back(".google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("ws://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("wss://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("ws://google.com:80")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("ws://google.com:123")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("ws://google.com:443")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, ExactMatchWorks) {
  std::vector<std::string> allowlist;
  allowlist.push_back(".google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("https://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("ws://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("wss://google.com")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("ws://a.google.com")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("wss://a.google.com")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("ws://com")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("wss://oogle.com")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, SchemeInAllowlistIsInvalid) {
  std::vector<std::string> allowlist;
  allowlist.push_back("http://google.com");
  SetAllowlist(std::move(allowlist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://google.com")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       NonStandardSchemeInAllowlistIsInvalid) {
  std::vector<std::string> allowlist;
  allowlist.push_back("data:google.com");
  allowlist.push_back("mailto:google.com");
  SetAllowlist(std::move(allowlist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://google.com")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, PortInAllowlistIsInvalid) {
  std::vector<std::string> allowlist;
  allowlist.push_back("www.google.com:123");
  SetAllowlist(std::move(allowlist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://www.google.com")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://www.google.com:123")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, PathInAllowlistIsInvalid) {
  std::vector<std::string> allowlist;
  allowlist.push_back("www.google.com/123");
  SetAllowlist(std::move(allowlist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://www.google.com/123")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://www.google.com")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, PathQueryAndReferenceWorks) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/a")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/a/b")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com?test=1")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/a#a100")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, TrailingDotInRuleWorks) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com.");
  allowlist.push_back("example.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://example.com.")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, DomainNameEmbeddedInPathIsIgnored) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://example.com/google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, URLsWithEmbeddedUserNamePassword) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://user1:pass@google.com")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, AllowlistsWithPunycodeWorks) {
  std::vector<std::string> allowlist;
  allowlist.push_back("㯙㯜㯙㯟.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://xn--domain.com")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       PathQueryAndReferenceWorksWithLeadingDot) {
  std::vector<std::string> allowlist;
  allowlist.push_back(".google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/a")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/a/b")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com?test=1")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/a#a100")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       SubdomainsAreAllowedWhenNoLeadingDots) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://b.a.google.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       SubdomainsAreNotAllowedWhenLeadingDots) {
  std::vector<std::string> allowlist;
  allowlist.push_back(".google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://b.a.google.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       MatchSubdomainsInMultipleAllowlists) {
  std::vector<std::string> allowlist;
  allowlist.push_back("a.google.com");
  allowlist.push_back(".google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://b.a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://b.google.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, TestLeadingDotInGURL) {
  std::vector<std::string> allowlist;
  allowlist.push_back("a.google.com");
  allowlist.push_back(".google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://.a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://.b.a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://.google.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://.b.google.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyTLDsAreNotSpecial) {
  std::vector<std::string> allowlist;
  allowlist.push_back(".com");
  allowlist.push_back("co");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://b.a.google.co/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://com/")));
}

// It seems GURL is happy to accept "*" in hostname literal. Since we rely
// on GURL host validation, just be consistent on that but make sure
// that does not wildcard all the domains.
TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyStarDoesNotWildcardDomains) {
  std::vector<std::string> allowlist;
  allowlist.push_back("*.com");
  allowlist.push_back("*co");
  allowlist.push_back("b.a.*.co");
  allowlist.push_back("b.*.*.co");
  allowlist.push_back("b.*");
  allowlist.push_back("c*");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://b.a.google.co/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://com/")));

  allowlist.clear();
  allowlist.push_back("*");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://*/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyPrefixOrSuffixOfDomains) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://ogle.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://agoogle.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyIPV4CanBeAllowlisted) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  allowlist.push_back("192.168.1.1");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://192.168.1.1/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyIPV4IsNotSegmented) {
  std::vector<std::string> allowlist;
  allowlist.push_back("192.168.1.1");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://192.168.1.1/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://1.192.168.1.1/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://192.168.1.0/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyLeadingDotInIPV4IsNotValid) {
  std::vector<std::string> allowlist;
  allowlist.push_back(".192.168.1.1");
  SetAllowlist(std::move(allowlist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://192.168.1.1/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://1.192.168.1.1/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyMultipleIPV4Works) {
  std::vector<std::string> allowlist;
  allowlist.push_back("192.168.1.1");
  allowlist.push_back("192.168.1.2");
  allowlist.push_back("194.168.1.1");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://192.168.1.1/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://192.168.1.2/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://194.168.1.1/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("https://194.168.1.1/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://194.168.1.1:443/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyIPV6CanBeAllowlisted) {
  std::vector<std::string> allowlist;
  allowlist.push_back("[10:20:30:40:50:60:70:80]");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://[10:20:30:40:50:60:70:80]")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyIPV6CannotBeAllowlistedIfBroken) {
  std::vector<std::string> allowlist;
  allowlist.push_back("[10:20:30:40:50:60:]");
  SetAllowlist(std::move(allowlist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://[10:20:30:40:50:60:70:80]")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyIPV6WithZerosCanBeAllowlisted) {
  std::vector<std::string> allowlist;
  allowlist.push_back("[20:0:0:0:0:0:0:0]");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://[20:0:0:0:0:0:0:0]")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyCapitalizationDoesNotMatter) {
  std::vector<std::string> allowlist;
  allowlist.push_back("A.goOGle.Com");
  allowlist.push_back(".GOOGLE.COM");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://b.a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://b.google.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyAllowlistingWorksWhenDomainSuffixesMatch1) {
  std::vector<std::string> allowlist;
  allowlist.push_back("com");
  allowlist.push_back("example.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://com/")));
}

// Same as verifyAllowlistingWorksWhenDomainSuffixesMatch1 but order reversed.
TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyAllowlistingWorksWhenDomainSuffixesMatch2) {
  std::vector<std::string> allowlist;
  allowlist.push_back("example.com");
  allowlist.push_back("com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyAllowlistingWorksWhenDomainSuffixesMatchWithLeadingDots1) {
  std::vector<std::string> allowlist;
  allowlist.push_back(".com");
  allowlist.push_back("example.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://com/")));
}

// Same as VerifyAllowlistingWorksWhenDomainSuffixesMatchWithLeadingDots2
// but order reversed.
TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyAllowlistingWorksWhenDomainSuffixesMatchWithLeadingDots2) {
  std::vector<std::string> allowlist;
  allowlist.push_back("example.com");
  allowlist.push_back(".com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://com/")));
}

// Verify that a more general rule won't be rendered useless by a rule that
// more closely matches. For example if "com" is a rule to allowlist all com
// subdomains, a later rule for .example.com should not make a.example.com a
// no match.
TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyAnExactMatchRuleCanBeOverwrittenByAMoreGeneralNonExactMatchRule) {
  std::vector<std::string> allowlist;
  allowlist.push_back("com");
  allowlist.push_back(".example.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifySubdomainMatchWinsIfRuleIsEnteredWithAndWithoutSubdomainMatch) {
  std::vector<std::string> allowlist;
  allowlist.push_back("example.com");
  allowlist.push_back(".example.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));

  allowlist = std::vector<std::string>();
  allowlist.push_back(".example.com");
  allowlist.push_back("example.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyOrderOfRuleEntryDoesNotChangeExpectations) {
  std::vector<std::string> allowlist;
  allowlist.push_back("b.example.com");
  allowlist.push_back(".example.com");
  allowlist.push_back("example.com");
  allowlist.push_back("a.example.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://b.example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://example.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://com/")));

  allowlist = std::vector<std::string>();
  allowlist.push_back("a.example.com");
  allowlist.push_back("example.com");
  allowlist.push_back(".example.com");
  allowlist.push_back("b.example.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://b.example.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://example.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("http://com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyInvalidUrlsAreNotAllowlisted) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();

  GURL url = GURL("");
  EXPECT_FALSE(url.is_valid());
  EXPECT_FALSE(wm_->IsUrlAllowed(url));

  url = GURL("http;??www.google.com");
  EXPECT_FALSE(url.is_valid());
  EXPECT_FALSE(wm_->IsUrlAllowed(url));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest,
       VerifyUrlsWithoutHostAreNotAllowlisted) {
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("file:///google.com/test")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("mailto:google.com/")));
  EXPECT_FALSE(wm_->IsUrlAllowed(GURL("data:google.com/")));

  // However FTP, HTTP and WS should work
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("ftp://google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("http://google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("ws://google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("https://google.com/")));
  EXPECT_TRUE(wm_->IsUrlAllowed(GURL("wss://google.com/")));
}

TEST_F(AwSafeBrowsingAllowlistManagerTest, VerifyAllowListSetObserverCalled) {
  TestAwSafeBrowsingAllowlistSetObserver observer(wm_.get());
  std::vector<std::string> allowlist;
  allowlist.push_back("google.com");
  SetAllowlist(std::move(allowlist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.observer_triggered());
}

}  // namespace android_webview
