// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/component_updater/pref_names.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"

namespace contextual_cueing {

class FakeOptimizationGuideKeyedService
    : public testing::NiceMock<MockOptimizationGuideKeyedService> {
 public:
  optimization_guide::ModelExecutionFeaturesController*
  GetModelExecutionFeaturesController() override {
    return fake_features_controller_.get();
  }

  void SetFakeFeaturesController(
      std::unique_ptr<optimization_guide::ModelExecutionFeaturesController>
          fake_features_controller) {
    fake_features_controller_ = std::move(fake_features_controller);
  }

 private:
  std::unique_ptr<optimization_guide::ModelExecutionFeaturesController>
      fake_features_controller_;
};

std::unique_ptr<KeyedService> CreateOptimizationGuideKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<FakeOptimizationGuideKeyedService>();
}

class ContextualCueingHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  ContextualCueingHelperTest() {
    scoped_feature_list_.InitAndEnableFeature(
        contextual_cueing::kContextualCueing);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        pref_service_->registry());
    local_state_->registry()->RegisterBooleanPref(
        ::prefs::kComponentUpdatesEnabled, true, PrefRegistry::LOSSY_PREF);

    ((FakeOptimizationGuideKeyedService*)
         OptimizationGuideKeyedServiceFactory::GetForProfile(profile()))
        ->SetFakeFeaturesController(
            std::make_unique<
                optimization_guide::ModelExecutionFeaturesController>(
                pref_service_.get(), identity_test_env_.identity_manager(),
                local_state_.get(),
                optimization_guide::ModelExecutionFeaturesController::
                    DogfoodStatus::NON_DOGFOOD));
  }

  void TearDown() override {
    contextual_cueing_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void EnableSignIn() {
    auto account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
  }

  void EnableSignInWithoutCapability() {
    auto account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(false);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        OptimizationGuideKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&CreateOptimizationGuideKeyedService)}};
  }

 protected:
  std::unique_ptr<ContextualCueingHelper> contextual_cueing_helper_;

 private:
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContextualCueingHelperTest, NullTabHelperWithoutSignin) {
  contextual_cueing_helper_ =
      ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  EXPECT_FALSE(contextual_cueing_helper_);
}

TEST_F(ContextualCueingHelperTest, NullTabHelperIfWithoutCapability) {
  EnableSignInWithoutCapability();
  contextual_cueing_helper_ =
      ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  EXPECT_FALSE(contextual_cueing_helper_);
}

TEST_F(ContextualCueingHelperTest, TabHelperStartsUp) {
  EnableSignIn();
  contextual_cueing_helper_ =
      ContextualCueingHelper::MaybeCreateForWebContents(web_contents());
  EXPECT_TRUE(contextual_cueing_helper_);
}

}  // namespace contextual_cueing
