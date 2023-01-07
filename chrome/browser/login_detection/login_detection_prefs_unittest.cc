// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_prefs.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace login_detection {

class LoginDetectionPrefsTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kLoginDetection);
    prefs::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(LoginDetectionPrefsTest, SiteSavedToOAuthSignedInList) {
  GURL test_url("https://foo.com");
  EXPECT_FALSE(prefs::IsSiteInOAuthSignedInList(&pref_service_, test_url));
  prefs::SaveSiteToOAuthSignedInList(&pref_service_, test_url);
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, test_url));
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_,
                                               GURL("https://m.foo.com")));
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_,
                                               GURL("https://www.foo.com")));
}

// Verify that the effective TLD+1 domains of the OAuth url are also treated as
// signed in.
TEST_F(LoginDetectionPrefsTest, OAuthSignedInListRespectsTLD) {
  GURL oauth_url("https://auth.foo.com");
  GURL test_urls[] = {
      GURL("https://auth.foo.com"), GURL("https://m.foo.com"),
      GURL("https://www.foo.com"),  GURL("https://images.foo.com"),
      GURL("https://a.b.foo.com"),  GURL("https://foo.com"),
  };
  for (const auto& test_url : test_urls) {
    EXPECT_FALSE(prefs::IsSiteInOAuthSignedInList(&pref_service_, test_url));
  }
  prefs::SaveSiteToOAuthSignedInList(&pref_service_, oauth_url);
  for (const auto& test_url : test_urls) {
    EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, test_url));
  }
}

TEST_F(LoginDetectionPrefsTest, OAuthSignedInListCleared) {
  GURL test_url("https://foo.com");
  prefs::SaveSiteToOAuthSignedInList(&pref_service_, test_url);
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, test_url));
  prefs::RemoveLoginDetectionData(&pref_service_);
  EXPECT_FALSE(prefs::IsSiteInOAuthSignedInList(&pref_service_, test_url));
}

TEST_F(LoginDetectionPrefsTest, OAuthSignedInListCappedToMaxSize) {
  // Set up the max limit to 3
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kLoginDetection, {{"oauth_loggedin_sites_max_size", "3"}});

  GURL foo_url("https://foo.com");
  GURL bar_url("https://bar.com");
  GURL baz_url("https://baz.com");
  GURL qux_url("https://qux.com");

  // Save 3 sites to OAuth list.
  prefs::SaveSiteToOAuthSignedInList(&pref_service_, foo_url);
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, foo_url));
  prefs::SaveSiteToOAuthSignedInList(&pref_service_, bar_url);
  prefs::SaveSiteToOAuthSignedInList(&pref_service_, baz_url);
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, foo_url));
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, bar_url));
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, baz_url));

  // The fourth site should remove the first site.
  prefs::SaveSiteToOAuthSignedInList(&pref_service_, qux_url);
  EXPECT_FALSE(prefs::IsSiteInOAuthSignedInList(&pref_service_, foo_url));
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, bar_url));
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, baz_url));
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_, qux_url));
}

}  // namespace login_detection
