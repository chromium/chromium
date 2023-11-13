// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_url_matcher.h"

#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

class LegacyURLMatcherTest : public ::testing::Test {
 public:
  LegacyURLMatcherTest() = default;
  ~LegacyURLMatcherTest() override = default;

  void SetPolicy(const std::vector<std::string>& urls) {
    base::Value::List policy;
    for (const auto& url : urls) {
      policy.Append(base::Value(url));
    }
    profile_.GetTestingPrefService()->SetManagedPref(
        kCloudLegacyTechReportAllowlist,
        std::make_unique<base::Value>(std::move(policy)));
  }

  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(LegacyURLMatcherTest, Match) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"www.example.com"});
  EXPECT_EQ("www.example.com",
            *matcher.GetMatchedURL(GURL("https://www.example.com")));
  EXPECT_EQ("www.example.com",
            *matcher.GetMatchedURL(GURL("https://www.example.com/")));
  EXPECT_EQ("www.example.com",
            *matcher.GetMatchedURL(GURL("http://www.example.com/path")));
  EXPECT_EQ("www.example.com",
            *matcher.GetMatchedURL(GURL("https://www.example.com/path")));
  EXPECT_EQ("www.example.com",
            *matcher.GetMatchedURL(GURL("https://www.example.com:8088/path")));
  EXPECT_EQ(
      "www.example.com",
      *matcher.GetMatchedURL(GURL("https://www.example.com/path?query=text")));
}

TEST_F(LegacyURLMatcherTest, NotMatch) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"www.example.com"});
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://www.example2.com")));
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://example.com")));
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://chat.example2.com")));
}

TEST_F(LegacyURLMatcherTest, SubDomain) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"www.example.com", "example2.com", ".example3.com"});

  // Only subdomain www is matched
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://example.com")));
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://chat.example.com")));
  EXPECT_TRUE(matcher.GetMatchedURL(GURL("https://www.example.com")));

  // All subdomains are matched.
  EXPECT_TRUE(matcher.GetMatchedURL(GURL("https://example2.com")));
  EXPECT_TRUE(matcher.GetMatchedURL(GURL("https://chat.example2.com")));
  EXPECT_TRUE(matcher.GetMatchedURL(GURL("https://www.example2.com")));
  EXPECT_TRUE(matcher.GetMatchedURL(GURL("https://s1.s2.example2.com")));

  // Subdomain wildcard matching is disabled with prefix dot.
  EXPECT_TRUE(matcher.GetMatchedURL(GURL("https://example3.com")));
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://chat.example3.com")));
}

TEST_F(LegacyURLMatcherTest, PathPrecedence) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"www.example.com", "www.example.com/p1", "www.example.com/p1/p2"});
  EXPECT_EQ("www.example.com",
            *matcher.GetMatchedURL(GURL("https://www.example.com/p2")));
  EXPECT_EQ("www.example.com/p1",
            *matcher.GetMatchedURL(GURL("https://www.example.com/p1")));
  EXPECT_EQ("www.example.com/p1",
            *matcher.GetMatchedURL(GURL("https://www.example.com/p1/p3")));
  EXPECT_EQ("www.example.com/p1/p2",
            *matcher.GetMatchedURL(GURL("https://www.example.com/p1/p2/p3")));
}

TEST_F(LegacyURLMatcherTest, IgnoredPart) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"http://www.example.com:8088/path?query=text#abc",
             "https://www.example2.com/"});
  EXPECT_EQ("http://www.example.com:8088/path?query=text#abc",
            *matcher.GetMatchedURL(GURL("https://www.example.com/path")));
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://www.example.com/p2")));

  EXPECT_EQ("https://www.example2.com/",
            *matcher.GetMatchedURL(GURL("http://www.example2.com")));
}

TEST_F(LegacyURLMatcherTest, Update) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"www.example.com/p1"});
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://www.example2.com/p2")));
  SetPolicy({"www.example.com/p2", "www.example.com/p1"});
  EXPECT_EQ("www.example.com/p2",
            *matcher.GetMatchedURL(GURL("https://www.example.com/p2")));
}

TEST_F(LegacyURLMatcherTest, Localhost) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"localhost", "localhost/path"});
  EXPECT_EQ("localhost/path",
            *matcher.GetMatchedURL(GURL("https://localhost/path2")));
}

TEST_F(LegacyURLMatcherTest, IP) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"192.168.1.1", "192.168.1./path"});

  EXPECT_EQ("192.168.1.1",
            *matcher.GetMatchedURL(GURL("https://192.168.1.1/path2")));
  EXPECT_FALSE(matcher.GetMatchedURL(GURL("https://192.168.1.2/path")));
}

TEST_F(LegacyURLMatcherTest, File) {
  LegacyTechURLMatcher matcher{profile()};

#if BUILDFLAG(IS_WIN)
  std::string path = "file://c:\\\\path";
#else
  std::string path = "file:///home/path";
#endif

  SetPolicy({path});
  EXPECT_EQ(path, *matcher.GetMatchedURL(GURL(path)));
}

TEST_F(LegacyURLMatcherTest, Chrome) {
  LegacyTechURLMatcher matcher{profile()};
  SetPolicy({"chrome://policy"});
  EXPECT_EQ("chrome://policy",
            *matcher.GetMatchedURL(GURL("chrome://policy/log")));
}

}  // namespace enterprise_reporting
