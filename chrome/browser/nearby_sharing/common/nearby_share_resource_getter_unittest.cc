// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class NearbyShareResourceGetterTest : public ::testing::Test {
 public:
  NearbyShareResourceGetterTest() {}

  ~NearbyShareResourceGetterTest() override = default;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(NearbyShareResourceGetterTest,
       GetStringWithFeatureNameWorksWithPlaceholderOfficialBuild) {
  base::test::ScopedFeatureList feature_list{features::kIsNameEnabled};

  // Just enforce non empty string for official branded builds..
  EXPECT_NE(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_SHARE_FEATURE_NAME_PH),
            u"");
}
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(NearbyShareResourceGetterTest,
       GetStringWithFeatureNameWorksWithPlaceholderUnofficialBuild) {
  base::test::ScopedFeatureList feature_list{features::kIsNameEnabled};

  // Expect the feature name to be inserted into the string.
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_SHARE_FEATURE_NAME_PH),
            u"Nearby Share");
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(NearbyShareResourceGetterTest, GetFeatureNameOfficialBuild) {
  base::test::ScopedFeatureList feature_list{features::kIsNameEnabled};

  // Just enforce non empty string for official branded builds..
  EXPECT_NE(NearbyShareResourceGetter::GetInstance()->GetFeatureName(), u"");
}
#else   // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(NearbyShareResourceGetterTest, GetFeatureNameWorksUnofficialBuild) {
  base::test::ScopedFeatureList feature_list{features::kIsNameEnabled};

  // Expect the feature name to be inserted into the string.
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetFeatureName(),
            u"Nearby Share");
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
