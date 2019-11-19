// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_config.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

class LoadingPredictorConfigTest : public testing::Test {
 public:
  void SetPreference(chrome_browser_net::NetworkPredictionOptions value) {
    profile_.GetPrefs()->SetInteger(prefs::kNetworkPredictionOptions, value);
  }

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(LoadingPredictorConfigTest, FeatureAndPrefEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(chrome_browser_net::NETWORK_PREDICTION_ALWAYS);

  EXPECT_TRUE(IsPreconnectFeatureEnabled());
  EXPECT_TRUE(IsLoadingPredictorEnabled(profile()));
  EXPECT_TRUE(IsPreconnectAllowed(profile()));
}

TEST_F(LoadingPredictorConfigTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(chrome_browser_net::NETWORK_PREDICTION_ALWAYS);

  EXPECT_FALSE(IsPreconnectFeatureEnabled());
  EXPECT_FALSE(IsLoadingPredictorEnabled(profile()));
  EXPECT_FALSE(IsPreconnectAllowed(profile()));
}

TEST_F(LoadingPredictorConfigTest, FeatureEnabledAndPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(chrome_browser_net::NETWORK_PREDICTION_NEVER);

  EXPECT_TRUE(IsPreconnectFeatureEnabled());
  EXPECT_TRUE(IsLoadingPredictorEnabled(profile()));
  EXPECT_FALSE(IsPreconnectAllowed(profile()));
}

TEST_F(LoadingPredictorConfigTest, OffTheRecordProfile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(predictors::kSpeculativePreconnectFeature);
  SetPreference(chrome_browser_net::NETWORK_PREDICTION_ALWAYS);
  Profile* incognito = profile()->GetOffTheRecordProfile();

  EXPECT_TRUE(IsPreconnectFeatureEnabled());
  EXPECT_FALSE(IsLoadingPredictorEnabled(incognito));
  EXPECT_TRUE(IsPreconnectAllowed(incognito));
}

}  // namespace predictors
