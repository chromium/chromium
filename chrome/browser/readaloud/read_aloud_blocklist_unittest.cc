// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_blocklist.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace readaloud {
namespace {

class ReadAloudBlocklistBlockedDomainsTest
    : public testing::TestWithParam<std::string_view> {};

TEST_P(ReadAloudBlocklistBlockedDomainsTest,
       BlocksDefaultDomainsAndStrippedPrefixes) {
  ReadAloudBlocklist blocklist;
  EXPECT_TRUE(blocklist.IsBlocked(GURL(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ReadAloudBlocklistBlockedDomainsTest,
    testing::Values(
        // 1P Google web apps and privacy-critical domains.
        "https://drive.google.com/corp/drive/mobile/my-drive?pli=1",
        "https://docs.google.com/document/d/123/edit",
        "https://mail.google.com/mail/u/0/#inbox",
        "https://accounts.google.com/signin",
        "https://gemini.google.com/app",
        // Blocked 3P domains and www./m. prefixes stripped for exact match.
        "https://chatgpt.com/c/12345",
        "https://teams.microsoft.com/v2/",
        "https://x.com/home",
        "https://m.youtube.com/watch?v=123",
        "https://www.youtube.com/watch?v=1",
        "https://m.twitter.com/home",
        "https://amazon.com/dp/B000",
        "https://www.amazon.com/dp/B000",
        // http://, uppercase, trailing-dot, and blob: URLs unwrapped via
        // url::Origin::Create().
        "http://youtube.com/watch?v=1",
        "https://DRIVE.GOOGLE.COM/my-drive",
        "https://drive.google.com./my-drive",
        "blob:https://drive.google.com/1234-5678"));

TEST(ReadAloudBlocklistTest, AllowsNonBlockedAndInvalidUrls) {
  ReadAloudBlocklist blocklist;

  EXPECT_FALSE(blocklist.IsBlocked(GURL("https://blog.google/technology/ai/")));
  EXPECT_FALSE(blocklist.IsBlocked(GURL("https://developers.google.com/")));
  // Non-www/m subdomains and suffix-only lookalikes must not match.
  EXPECT_FALSE(blocklist.IsBlocked(GURL("https://aws.amazon.com/blogs/")));
  EXPECT_FALSE(blocklist.IsBlocked(GURL("https://whatsapp.com/")));
  EXPECT_FALSE(blocklist.IsBlocked(GURL("https://en.wikipedia.org/wiki/Cat")));
  EXPECT_FALSE(blocklist.IsBlocked(GURL("https://box.com/")));
  EXPECT_FALSE(blocklist.IsBlocked(GURL("https://notyoutube.com/")));
  EXPECT_FALSE(blocklist.IsBlocked(GURL("not-a-valid-url")));
  EXPECT_FALSE(blocklist.IsBlocked(GURL()));
}

TEST(ReadAloudBlocklistTest, BypassedWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kReadAloudBlockedDomains);

  ReadAloudBlocklist blocklist;
  EXPECT_FALSE(blocklist.IsBlocked(GURL("https://drive.google.com/my-drive")));
}

TEST(ReadAloudBlocklistTest, SingletonReturnsSameInstance) {
  EXPECT_EQ(&ReadAloudBlocklist::GetInstance(),
            &ReadAloudBlocklist::GetInstance());
  EXPECT_TRUE(ReadAloudBlocklist::GetInstance().IsBlocked(
      GURL("https://mail.google.com/")));
}

}  // namespace
}  // namespace readaloud
