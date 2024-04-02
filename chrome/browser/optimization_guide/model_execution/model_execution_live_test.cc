// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/e2e_tests/account_capabilities_observer.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace optimization_guide {

// Manual live tests that sign-in and check model execution state.
//
// Developers can run the tests as follows:
// 1. Enable the tests by changing the "#if 0" below to "#if 1"
// 2. browser_tests --gtest_filter=ModelExecutionLiveTest.* --run-live-tests
//
// The whole code is disabled because the tests cannot be tagged as MANUAL_
// tests, since they are also using the PRE_ browser test feature. The browser
// tests currently do not support both the wasy - PRE_ and MANUAL_. So the tests
// are just written to use PRE_, and commented out.
#if 0
class ModelExecutionLiveTest : public signin::test::LiveTest {
 public:
  ModelExecutionLiveTest() = default;
  ~ModelExecutionLiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationGuideModelExecution,
         features::internal::kTabOrganizationSettingsVisibility},
        {});
    LiveTest::SetUp();
    // Always disable animation for stability.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  OptimizationGuideKeyedService* GetOptGuideKeyedService() {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile());
  }

  bool IsSettingVisible(optimization_guide::UserVisibleFeatureKey feature) {
    return GetOptGuideKeyedService()->IsSettingVisible(feature);
  }

  syncer::SyncService* sync_service() {
    return signin::test::sync_service(browser());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  signin::test::SignInFunctions sign_in_functions =
      signin::test::SignInFunctions(
          base::BindLambdaForTesting(
              [this]() -> Browser* { return this->browser(); }),
          base::BindLambdaForTesting(
              [this](int index,
                     const GURL& url,
                     ui::PageTransition transition) -> bool {
                return this->AddTabAtIndex(index, url, transition);
              }));
};

IN_PROC_BROWSER_TEST_F(ModelExecutionLiveTest, PRE_SimpleSyncFlow) {
  signin::test::TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", ta));
  sign_in_functions.TurnOnSync(ta, 0);

  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(IsSettingVisible(
      UserVisibleFeatureKey::kTabOrganization));
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.SettingsVisibilityResult."
      "TabOrganization",
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFieldTrialEnabled,
      1);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionLiveTest, SimpleSyncFlow) {
  signin::test::TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_1", ta));

  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(IsSettingVisible(
      UserVisibleFeatureKey::kTabOrganization));
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.SettingsVisibilityResult."
      "TabOrganization",
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFieldTrialEnabled,
      1);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionLiveTest,
                       PRE_SimpleSyncFlowForMinorAccount) {
  signin::test::TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_MINOR", ta));
  sign_in_functions.TurnOnSync(ta, 0);

  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(IsSettingVisible(
      UserVisibleFeatureKey::kTabOrganization));
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.SettingsVisibilityResult."
      "TabOrganization",
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kNotVisibleModelExecutionCapability,
      1);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionLiveTest,
                       SimpleSyncFlowForMinorAccount) {
  signin::test::TestAccount ta;
  CHECK(GetTestAccountsUtil()->GetAccount("TEST_ACCOUNT_MINOR", ta));

  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(IsSettingVisible(
      UserVisibleFeatureKey::kTabOrganization));
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.SettingsVisibilityResult."
      "TabOrganization",
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kNotVisibleModelExecutionCapability,
      1);
}

#endif

}  // namespace optimization_guide
