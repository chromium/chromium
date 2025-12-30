// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/security_settings_bundle_toast_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class MockToastController : public ToastController {
 public:
  MockToastController() : ToastController(nullptr, nullptr) {}
  MOCK_METHOD(bool, MaybeShowToast, (ToastParams params), (override));
};

}  // namespace

class SecuritySettingsBundleToastHelperTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    helper_ = SecuritySettingsBundleToastHelper::GetForProfile(profile_.get());
    helper_->SetToastControllerForTesting(&toast_controller_);
  }

  void TearDown() override {
    helper_ = nullptr;
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<SecuritySettingsBundleToastHelper> helper_;
  testing::NiceMock<MockToastController> toast_controller_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SecuritySettingsBundleToastHelperTest, ToastShownIfPending) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(
      prefs::kSecuritySettingsBundleMigrationToastState,
      static_cast<int>(SecuritySettingsBundleToastState::kPending));
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::ENHANCED);

  EXPECT_CALL(
      toast_controller_,
      MaybeShowToast(testing::Field(&ToastParams::toast_id,
                                    ToastId::kEnhancedBundledSecuritySettings)))
      .Times(1)
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs([]() {
                                 base::UmaHistogramEnumeration(
                                     "Toast.TriggeredToShow",
                                     ToastId::kEnhancedBundledSecuritySettings);
                               }),
                               testing::Return(true)));

  helper_->TriggerIfNeeded();

  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kShown),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));
  histogram_tester_.ExpectUniqueSample(
      "Toast.TriggeredToShow", ToastId::kEnhancedBundledSecuritySettings, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.SecuritySettingsBundle."
      "BundleStateBeforeShowingEnhancedToast",
      SecuritySettingsBundleSetting::ENHANCED, 1);
}

TEST_F(SecuritySettingsBundleToastHelperTest, ToastNotShownIfNone) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(prefs::kSecuritySettingsBundleMigrationToastState,
                    static_cast<int>(SecuritySettingsBundleToastState::kNone));
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::ENHANCED);

  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  helper_->TriggerIfNeeded();

  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kNone),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));
}

TEST_F(SecuritySettingsBundleToastHelperTest, ToastNotShownIfAlreadyShown) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(prefs::kSecuritySettingsBundleMigrationToastState,
                    static_cast<int>(SecuritySettingsBundleToastState::kShown));
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::ENHANCED);

  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  helper_->TriggerIfNeeded();

  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kShown),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));
}

TEST_F(SecuritySettingsBundleToastHelperTest, RetryTriggeredIfToastShownFails) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(
      prefs::kSecuritySettingsBundleMigrationToastState,
      static_cast<int>(SecuritySettingsBundleToastState::kPending));
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::ENHANCED);

  EXPECT_CALL(
      toast_controller_,
      MaybeShowToast(testing::Field(&ToastParams::toast_id,
                                    ToastId::kEnhancedBundledSecuritySettings)))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs([]() {
                                 base::UmaHistogramEnumeration(
                                     "Toast.FailedToShow",
                                     ToastId::kEnhancedBundledSecuritySettings);
                               }),
                               testing::Return(false)))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs([]() {
                                 base::UmaHistogramEnumeration(
                                     "Toast.TriggeredToShow",
                                     ToastId::kEnhancedBundledSecuritySettings);
                               }),
                               testing::Return(true)));

  helper_->TriggerIfNeeded();

  // Initially failed, so state should still be PENDING.
  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kPending),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));

  // Advance time to trigger retry.
  task_environment_.FastForwardBy(
      SecuritySettingsBundleToastHelper::kRetryDelay);
  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kShown),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));
  histogram_tester_.ExpectUniqueSample(
      "Toast.TriggeredToShow", ToastId::kEnhancedBundledSecuritySettings, 1);
  histogram_tester_.ExpectUniqueSample(
      "Toast.FailedToShow", ToastId::kEnhancedBundledSecuritySettings, 1);
}

TEST_F(SecuritySettingsBundleToastHelperTest, RetryStopsAfterMaxRetries) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(
      prefs::kSecuritySettingsBundleMigrationToastState,
      static_cast<int>(SecuritySettingsBundleToastState::kPending));
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::ENHANCED);

  // Expect kMaxRetries + 1 calls: 1 initial + kMaxRetries retries.
  EXPECT_CALL(
      toast_controller_,
      MaybeShowToast(testing::Field(&ToastParams::toast_id,
                                    ToastId::kEnhancedBundledSecuritySettings)))
      .Times(SecuritySettingsBundleToastHelper::kMaxRetries + 1)
      .WillRepeatedly(
          testing::DoAll(testing::InvokeWithoutArgs([]() {
                           base::UmaHistogramEnumeration(
                               "Toast.FailedToShow",
                               ToastId::kEnhancedBundledSecuritySettings);
                         }),
                         testing::Return(false)));

  helper_->TriggerIfNeeded();

  for (int i = 0; i < SecuritySettingsBundleToastHelper::kMaxRetries; ++i) {
    task_environment_.FastForwardBy(
        SecuritySettingsBundleToastHelper::kRetryDelay);
  }
  // Verify that even after more time, no more retries occur.
  task_environment_.FastForwardBy(
      SecuritySettingsBundleToastHelper::kRetryDelay);

  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kPending),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));
  histogram_tester_.ExpectUniqueSample(
      "Toast.FailedToShow", ToastId::kEnhancedBundledSecuritySettings,
      SecuritySettingsBundleToastHelper::kMaxRetries + 1);
}

TEST_F(SecuritySettingsBundleToastHelperTest, ToastNotShownIfNotEnhanced) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(
      prefs::kSecuritySettingsBundleMigrationToastState,
      static_cast<int>(SecuritySettingsBundleToastState::kPending));
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::STANDARD);

  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  helper_->TriggerIfNeeded();

  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kPending),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.SecuritySettingsBundle."
      "BundleStateBeforeShowingEnhancedToast",
      SecuritySettingsBundleSetting::STANDARD, 1);
}

}  // namespace safe_browsing
