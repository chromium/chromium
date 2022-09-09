// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_config.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

class LoadingPredictorConfigTest : public testing::Test {
 public:
  void SetPreference(prefetch::PreloadPagesState value) {
    prefetch::SetPreloadPagesState(profile_.GetPrefs(), value);
  }

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(LoadingPredictorConfigTest, FeatureAndPrefEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(prefetch::PreloadPagesState::kStandardPreloading);

  EXPECT_TRUE(IsPreconnectFeatureEnabled());
  EXPECT_TRUE(IsLoadingPredictorEnabled(profile()));
  EXPECT_TRUE(IsPreconnectAllowed(profile()));
}

TEST_F(LoadingPredictorConfigTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(prefetch::PreloadPagesState::kStandardPreloading);

  EXPECT_FALSE(IsPreconnectFeatureEnabled());
  EXPECT_FALSE(IsLoadingPredictorEnabled(profile()));
  EXPECT_FALSE(IsPreconnectAllowed(profile()));
}

TEST_F(LoadingPredictorConfigTest, FeatureEnabledAndPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(prefetch::PreloadPagesState::kNoPreloading);

  EXPECT_TRUE(IsPreconnectFeatureEnabled());
  EXPECT_TRUE(IsLoadingPredictorEnabled(profile()));
  EXPECT_FALSE(IsPreconnectAllowed(profile()));
}

TEST_F(LoadingPredictorConfigTest, IncognitoProfile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(prefetch::PreloadPagesState::kStandardPreloading);
  Profile* incognito =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_TRUE(IsPreconnectFeatureEnabled());
  EXPECT_FALSE(IsLoadingPredictorEnabled(incognito));
  EXPECT_TRUE(IsPreconnectAllowed(incognito));
}

TEST_F(LoadingPredictorConfigTest, NonPrimaryOffTheRecordProfile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(prefetch::PreloadPagesState::kStandardPreloading);
  Profile* otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  EXPECT_TRUE(IsPreconnectFeatureEnabled());
  EXPECT_FALSE(IsLoadingPredictorEnabled(otr_profile));
  EXPECT_TRUE(IsPreconnectAllowed(otr_profile));
}

}  // namespace predictors
