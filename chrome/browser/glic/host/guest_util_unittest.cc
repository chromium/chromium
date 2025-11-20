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
class GuestUtilMultiInstanceTest : public testing::TestWithParam<bool> {
 public:
  GuestUtilMultiInstanceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    if (IsMultiInstanceEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {
              {features::kGlicMultiInstance, {}},
              {features::kGlicMultitabUnderlines, {}},
              {mojom::features::kGlicMultiTab, {}},
              {features::kGlicURLConfig,
               {{features::kGlicGuestURL.name,
                 "https://www.example.com/glic"}}},
          },
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {
              {features::kGlicURLConfig,
               {{features::kGlicGuestURL.name,
                 "https://www.example.org/glic"}}},
          },
          /*disabled_features=*/{});
    }
  }

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfile* CreateTestingProfile() {
    return profile_manager_.CreateTestingProfile("test_profile");
  }

 protected:
  bool IsMultiInstanceEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

TEST_P(GuestUtilMultiInstanceTest,
       MaybeAddMultiInstanceParameterAddsParameter) {
  if (IsMultiInstanceEnabled()) {
    EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com")),
              GURL("https://www.google.com?mode=mi"));
    EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/")),
              GURL("https://www.google.com/?mode=mi"));
    EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?")),
              GURL("https://www.google.com/?mode=mi"));
    EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?&")),
              GURL("https://www.google.com/?mode=mi"));
    EXPECT_EQ(
        MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?a=b")),
        GURL("https://www.google.com/?a=b&mode=mi"));
    EXPECT_EQ(
        MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?a=b&")),
        GURL("https://www.google.com/?a=b&mode=mi"));
    EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?c")),
              GURL("https://www.google.com/?c&mode=mi"));
  } else {
    EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com")),
              GURL("https://www.google.com"));
    EXPECT_EQ(
        MaybeAddMultiInstanceParameter(GURL("https://www.google.com?a=b")),
        GURL("https://www.google.com?a=b"));
    EXPECT_EQ(MaybeAddMultiInstanceParameter(GURL("https://www.google.com/")),
              GURL("https://www.google.com/"));
  }
}

TEST_P(GuestUtilMultiInstanceTest,
       MaybeAddMultiInstanceParameterReplacesParameter) {
  if (IsMultiInstanceEnabled()) {
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
  } else {
    EXPECT_EQ(
        MaybeAddMultiInstanceParameter(GURL("https://www.google.com/?mode=si")),
        GURL("https://www.google.com/?mode=si"));
  }
}

TEST_P(GuestUtilMultiInstanceTest, GetGlicGuestURLs) {
  if (IsMultiInstanceEnabled()) {
    EXPECT_EQ(GURL("https://www.example.com/glic?mode=mi&hl=en"),
              GetGuestURL());
    EXPECT_TRUE(net::GetValueForKeyInQuery(GetFreURL(CreateTestingProfile()),
                                           "mode", nullptr));
  } else {
    EXPECT_EQ(GURL("https://www.example.org/glic?hl=en"), GetGuestURL());
    EXPECT_FALSE(net::GetValueForKeyInQuery(GetFreURL(CreateTestingProfile()),
                                            "mode", nullptr));
  }
}

TEST_P(GuestUtilMultiInstanceTest, MaybeAddMultiInstanceParameterDisabled) {
  // Test that disabling the feature does not add any multi-instance params.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kGlicGuestUrlMultiInstanceParam);
  if (IsMultiInstanceEnabled()) {
    EXPECT_EQ(GURL("https://www.example.com/glic?hl=en"), GetGuestURL());
  } else {
    EXPECT_EQ(GURL("https://www.example.org/glic?hl=en"), GetGuestURL());
  }
}

INSTANTIATE_TEST_SUITE_P(All, GuestUtilMultiInstanceTest, testing::Bool());

}  // namespace

}  // namespace glic
