// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {

namespace {

class GuestUtilTest : public testing::Test {};

// Test fixture for multi-instance feature.
class GuestUtilMultiInstanceTest : public testing::Test {
 public:
  GuestUtilMultiInstanceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(
        {features::kGlicURLConfig,
         {{features::kGlicGuestURL.name, "https://www.example.com/glic"}}});

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfile* CreateTestingProfile() {
    return profile_manager_.CreateTestingProfile("test_profile");
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST(GuestUtilTest, GetLocalizedGuestURLAddsLanguageParameter) {
  EXPECT_EQ(GURL("https://www.google.com?hl=en"),
            GetLocalizedGuestURL(GURL("https://www.google.com")));
}

TEST(GuestUtilTest, GetLocalizedGuestURLDoesNotChangeLanguageParameter) {
  EXPECT_EQ(GURL("https://www.google.com?hl=es"),
            GetLocalizedGuestURL(GURL("https://www.google.com?hl=es")));
}

TEST_F(GuestUtilMultiInstanceTest,
       MaybeAddMultiInstanceParameterAddsParameter) {
  EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com")),
            GURL("https://www.google.com?mode=mi"));
  EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/")),
            GURL("https://www.google.com/?mode=mi"));
  EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?")),
            GURL("https://www.google.com/?mode=mi"));
  EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?&")),
            GURL("https://www.google.com/?mode=mi"));
  EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?a=b")),
            GURL("https://www.google.com/?a=b&mode=mi"));
  EXPECT_EQ(
      MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?a=b&")),
      GURL("https://www.google.com/?a=b&mode=mi"));
  EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?c")),
            GURL("https://www.google.com/?c&mode=mi"));
}

TEST_F(GuestUtilMultiInstanceTest,
       MaybeAddMultiInstanceParameterReplacesParameter) {
  EXPECT_EQ(
      MaybeAddMultiInstanceParameter(GURL("https://www.google.com?mode=si")),
      GURL("https://www.google.com?mode=mi"));
  EXPECT_EQ(
      MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?mode=si")),
      GURL("https://www.google.com/?mode=mi"));
  EXPECT_EQ(MaybeAddMultiInstanceParameter(
                GURL("https://www.google.com/?a=b&mode=si")),
            GURL("https://www.google.com/?a=b&mode=mi"));
  EXPECT_EQ(MaybeAddMultiInstanceParameter(
                GURL("https://www.google.com/?mode=si&a=b")),
            GURL("https://www.google.com/?mode=mi&a=b"));
}

TEST_F(GuestUtilMultiInstanceTest, GetGlicGuestURLs) {
  TestingProfile* profile = CreateTestingProfile();
    EXPECT_EQ(GURL("https://www.example.com/glic?mode=mi&hl=en"),
              GetGuestURL());
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(GetFreURL(profile), "mode", nullptr));
}

TEST_F(GuestUtilMultiInstanceTest, MaybeAddMultiInstanceParameterDisabled) {
  // Test that disabling the feature does not add any multi-instance params.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kGlicGuestUrlMultiInstanceParam);
  EXPECT_EQ(GURL("https://www.example.com/glic?hl=en"), GetGuestURL());
}

}  // namespace

}  // namespace glic
