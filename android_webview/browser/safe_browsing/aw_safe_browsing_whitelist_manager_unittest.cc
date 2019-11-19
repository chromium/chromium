// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_whitelist_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

class AwSafeBrowsingWhitelistManagerTest : public testing::Test {
 protected:
  AwSafeBrowsingWhitelistManagerTest() {}

  void SetUp() override {
    wm_.reset(new AwSafeBrowsingWhitelistManager(
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get()));
  }

  void TearDown() override { wm_.reset(); }

  void SetWhitelist(std::vector<std::string>&& whitelist, bool expected);

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<AwSafeBrowsingWhitelistManager> wm_;
};

void VerifyWhitelistCallback(bool expected, bool success) {
  EXPECT_EQ(expected, success);
}

void AwSafeBrowsingWhitelistManagerTest::SetWhitelist(
    std::vector<std::string>&& whitelist,
    bool expected) {
  wm_->SetWhitelistOnUIThread(
      std::move(whitelist), base::BindOnce(&VerifyWhitelistCallback, expected));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, WsSchemeCanBeWhitelisted) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("ws://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("wss://google.com")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, HttpSchemeCanBeWhitelisted) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("https://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com:80")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com:123")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com:443")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, WsSchemeCanBeWhitelistedExactMatch) {
  std::vector<std::string> whitelist;
  whitelist.push_back(".google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("ws://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("wss://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("ws://google.com:80")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("ws://google.com:123")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("ws://google.com:443")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, ExactMatchWorks) {
  std::vector<std::string> whitelist;
  whitelist.push_back(".google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("https://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("ws://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("wss://google.com")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("ws://a.google.com")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("wss://a.google.com")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("ws://com")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("wss://oogle.com")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, SchemeInWhitelistIsInvalid) {
  std::vector<std::string> whitelist;
  whitelist.push_back("http://google.com");
  SetWhitelist(std::move(whitelist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://google.com")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       NonStandardSchemeInWhitelistIsInvalid) {
  std::vector<std::string> whitelist;
  whitelist.push_back("data:google.com");
  whitelist.push_back("mailto:google.com");
  SetWhitelist(std::move(whitelist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://google.com")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, PortInWhitelistIsInvalid) {
  std::vector<std::string> whitelist;
  whitelist.push_back("www.google.com:123");
  SetWhitelist(std::move(whitelist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://www.google.com")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://www.google.com:123")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, PathInWhitelistIsInvalid) {
  std::vector<std::string> whitelist;
  whitelist.push_back("www.google.com/123");
  SetWhitelist(std::move(whitelist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://www.google.com/123")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://www.google.com")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, PathQueryAndReferenceWorks) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/a")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/a/b")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com?test=1")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/a#a100")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, TrailingDotInRuleWorks) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com.");
  whitelist.push_back("example.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://example.com.")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, DomainNameEmbeddedInPathIsIgnored) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://example.com/google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, URLsWithEmbeddedUserNamePassword) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://user1:pass@google.com")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, WhitelistsWithPunycodeWorks) {
  std::vector<std::string> whitelist;
  whitelist.push_back("㯙㯜㯙㯟.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://xn--domain.com")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       PathQueryAndReferenceWorksWithLeadingDot) {
  std::vector<std::string> whitelist;
  whitelist.push_back(".google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/a")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/a/b")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com?test=1")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/a#a100")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       SubdomainsAreAllowedWhenNoLeadingDots) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://b.a.google.com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       SubdomainsAreNotAllowedWhenLeadingDots) {
  std::vector<std::string> whitelist;
  whitelist.push_back(".google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://b.a.google.com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       MatchSubdomainsInMultipleWhitelists) {
  std::vector<std::string> whitelist;
  whitelist.push_back("a.google.com");
  whitelist.push_back(".google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://b.a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://b.google.com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, TestLeadingDotInGURL) {
  std::vector<std::string> whitelist;
  whitelist.push_back("a.google.com");
  whitelist.push_back(".google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://.a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://.b.a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://.google.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://.b.google.com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyTLDsAreNotSpecial) {
  std::vector<std::string> whitelist;
  whitelist.push_back(".com");
  whitelist.push_back("co");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://b.a.google.co/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://com/")));
}

// It seems GURL is happy to accept "*" in hostname literal. Since we rely
// on GURL host validation, just be consistent on that but make sure
// that does not wildcard all the domains.
TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyStarDoesNotWildcardDomains) {
  std::vector<std::string> whitelist;
  whitelist.push_back("*.com");
  whitelist.push_back("*co");
  whitelist.push_back("b.a.*.co");
  whitelist.push_back("b.*.*.co");
  whitelist.push_back("b.*");
  whitelist.push_back("c*");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://b.a.google.co/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://com/")));

  whitelist.clear();
  whitelist.push_back("*");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://*/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyPrefixOrSuffixOfDomains) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://ogle.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://agoogle.com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyIPV4CanBeWhitelisted) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  whitelist.push_back("192.168.1.1");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://192.168.1.1/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyIPV4IsNotSegmented) {
  std::vector<std::string> whitelist;
  whitelist.push_back("192.168.1.1");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://192.168.1.1/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://1.192.168.1.1/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://192.168.1.0/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyLeadingDotInIPV4IsNotValid) {
  std::vector<std::string> whitelist;
  whitelist.push_back(".192.168.1.1");
  SetWhitelist(std::move(whitelist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://192.168.1.1/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://1.192.168.1.1/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyMultipleIPV4Works) {
  std::vector<std::string> whitelist;
  whitelist.push_back("192.168.1.1");
  whitelist.push_back("192.168.1.2");
  whitelist.push_back("194.168.1.1");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://192.168.1.1/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://192.168.1.2/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://194.168.1.1/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("https://194.168.1.1/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://194.168.1.1:443/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyIPV6CanBeWhitelisted) {
  std::vector<std::string> whitelist;
  whitelist.push_back("[10:20:30:40:50:60:70:80]");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://[10:20:30:40:50:60:70:80]")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyIPV6CannotBeWhitelistedIfBroken) {
  std::vector<std::string> whitelist;
  whitelist.push_back("[10:20:30:40:50:60:]");
  SetWhitelist(std::move(whitelist), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://[10:20:30:40:50:60:70:80]")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyIPV6WithZerosCanBeWhitelisted) {
  std::vector<std::string> whitelist;
  whitelist.push_back("[20:0:0:0:0:0:0:0]");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://[20:0:0:0:0:0:0:0]")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyCapitalizationDoesNotMatter) {
  std::vector<std::string> whitelist;
  whitelist.push_back("A.goOGle.Com");
  whitelist.push_back(".GOOGLE.COM");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://b.a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://b.google.com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyWhitelistingWorksWhenDomainSuffixesMatch1) {
  std::vector<std::string> whitelist;
  whitelist.push_back("com");
  whitelist.push_back("example.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://com/")));
}

// Same as verifyWhitelistingWorksWhenDomainSuffixesMatch1 but order reversed.
TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyWhitelistingWorksWhenDomainSuffixesMatch2) {
  std::vector<std::string> whitelist;
  whitelist.push_back("example.com");
  whitelist.push_back("com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyWhitelistingWorksWhenDomainSuffixesMatchWithLeadingDots1) {
  std::vector<std::string> whitelist;
  whitelist.push_back(".com");
  whitelist.push_back("example.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://com/")));
}

// Same as VerifyWhitelistingWorksWhenDomainSuffixesMatchWithLeadingDots2
// but order reversed.
TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyWhitelistingWorksWhenDomainSuffixesMatchWithLeadingDots2) {
  std::vector<std::string> whitelist;
  whitelist.push_back("example.com");
  whitelist.push_back(".com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://a.google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://com/")));
}

// Verify that a more general rule won't be rendered useless by a rule that
// more closely matches. For example if "com" is a rule to whitelist all com
// subdomains, a later rule for .example.com should not make a.example.com a
// no match.
TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyAnExactMatchRuleCanBeOverwrittenByAMoreGeneralNonExactMatchRule) {
  std::vector<std::string> whitelist;
  whitelist.push_back("com");
  whitelist.push_back(".example.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifySubdomainMatchWinsIfRuleIsEnteredWithAndWithoutSubdomainMatch) {
  std::vector<std::string> whitelist;
  whitelist.push_back("example.com");
  whitelist.push_back(".example.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));

  whitelist = std::vector<std::string>();
  whitelist.push_back(".example.com");
  whitelist.push_back("example.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyOrderOfRuleEntryDoesNotChangeExpectations) {
  std::vector<std::string> whitelist;
  whitelist.push_back("b.example.com");
  whitelist.push_back(".example.com");
  whitelist.push_back("example.com");
  whitelist.push_back("a.example.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://b.example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://example.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://com/")));

  whitelist = std::vector<std::string>();
  whitelist.push_back("a.example.com");
  whitelist.push_back("example.com");
  whitelist.push_back(".example.com");
  whitelist.push_back("b.example.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://a.example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://b.example.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://example.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("http://com/")));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest, VerifyInvalidUrlsAreNotWhitelisted) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();

  GURL url = GURL("");
  EXPECT_FALSE(url.is_valid());
  EXPECT_FALSE(wm_->IsURLWhitelisted(url));

  url = GURL("http;??www.google.com");
  EXPECT_FALSE(url.is_valid());
  EXPECT_FALSE(wm_->IsURLWhitelisted(url));
}

TEST_F(AwSafeBrowsingWhitelistManagerTest,
       VerifyUrlsWithoutHostAreNotWhitelisted) {
  std::vector<std::string> whitelist;
  whitelist.push_back("google.com");
  SetWhitelist(std::move(whitelist), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("file:///google.com/test")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("mailto:google.com/")));
  EXPECT_FALSE(wm_->IsURLWhitelisted(GURL("data:google.com/")));

  // However FTP, HTTP and WS should work
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("ftp://google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("http://google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("ws://google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("https://google.com/")));
  EXPECT_TRUE(wm_->IsURLWhitelisted(GURL("wss://google.com/")));
}

}  // namespace android_webview
