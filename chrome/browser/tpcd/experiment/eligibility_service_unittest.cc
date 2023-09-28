// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/eligibility_service.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/tpcd/experiment/experiment_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace tpcd::experiment {

namespace {

using ::testing::_;
using ::testing::Return;

class MockExperimentManager : public ExperimentManager {
 public:
  MockExperimentManager() = default;
  ~MockExperimentManager() override = default;

  MOCK_METHOD(void,
              SetClientEligibility,
              (bool, EligibilityDecisionCallback),
              (override));
  MOCK_METHOD(absl::optional<bool>, IsClientEligible, (), (const, override));
};

}  // namespace

class EligibilityServiceTest : public testing::Test {
 public:
  EligibilityServiceTest() {
    feature_list_.InitAndEnableFeature(
        features::kCookieDeprecationFacilitatedTesting);
  }

  void SetUp() override {
    experiment_manager_ = std::make_unique<MockExperimentManager>();

    auto* privacy_sandbox_settings =
        PrivacySandboxSettingsFactory::GetForProfile(&profile_);
    auto privacy_sandbox_delegate = std::make_unique<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>();
    privacy_sandbox_delegate_ = privacy_sandbox_delegate.get();
    privacy_sandbox_settings->SetDelegateForTesting(
        std::move(privacy_sandbox_delegate));
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  TestingProfile profile_;
  std::unique_ptr<MockExperimentManager> experiment_manager_;
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
      privacy_sandbox_delegate_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(EligibilityServiceTest, ClientEligibilityKnown_ClientEligibilityNotSet) {
  EXPECT_CALL(*experiment_manager_, IsClientEligible).WillOnce(Return(false));
  EXPECT_CALL(*experiment_manager_, SetClientEligibility).Times(0);

  EligibilityService eligibility_service(&profile_, experiment_manager_.get());
}

TEST_F(EligibilityServiceTest,
       ClientEligibilityUnknownProfileIneligible_ClientEligibilitySet) {
  EXPECT_CALL(*experiment_manager_, IsClientEligible)
      .WillOnce(Return(absl::nullopt));

  EXPECT_CALL(*privacy_sandbox_delegate_,
              IsCookieDeprecationExperimentCurrentlyEligible)
      .WillOnce(Return(false));

  EXPECT_CALL(*experiment_manager_, SetClientEligibility(false, _));

  EligibilityService eligibility_service(&profile_, experiment_manager_.get());
}

TEST_F(EligibilityServiceTest,
       ClientEligibilityUnknownProfileEligible_ClientEligibilitySet) {
  EXPECT_CALL(*experiment_manager_, IsClientEligible)
      .WillOnce(Return(absl::nullopt));

  EXPECT_CALL(*privacy_sandbox_delegate_,
              IsCookieDeprecationExperimentCurrentlyEligible)
      .WillOnce(Return(true));

  EXPECT_CALL(*experiment_manager_, SetClientEligibility(true, _));

  EligibilityService eligibility_service(&profile_, experiment_manager_.get());
}

}  // namespace tpcd::experiment
