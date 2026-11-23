// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/security_settings_bundle_toast_helper.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/test/mock_toast_controller.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SecuritySettingsBundleToastHelperTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    helper_ = SecuritySettingsBundleToastHelper::GetForProfile(profile_.get());
    ON_CALL(mock_browser_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    EXPECT_CALL(mock_browser_, RegisterBrowserDidClose(testing::_))
        .WillOnce([this](BrowserWindowInterface::BrowserDidCloseCallback cb) {
          close_callback_ = std::move(cb);
          return base::CallbackListSubscription();
        });
    BrowserManagerServiceFactory::GetForProfile(profile_.get())
        ->AddBrowserForTesting(&mock_browser_);
  }

  void TearDown() override {
    if (close_callback_) {
      close_callback_.Run(&mock_browser_);
      close_callback_.Reset();
    }
    helper_ = nullptr;
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<SecuritySettingsBundleToastHelper> helper_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_;
  BrowserWindowInterface::BrowserDidCloseCallback close_callback_;
  testing::NiceMock<MockToastController> toast_controller_{&mock_browser_};
  base::HistogramTester histogram_tester_;
};

TEST_F(SecuritySettingsBundleToastHelperTest, NoToastWithoutBrowser) {
  close_callback_.Run(&mock_browser_);
  close_callback_.Reset();
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetInteger(
      prefs::kSecuritySettingsBundleMigrationToastState,
      static_cast<int>(SecuritySettingsBundleToastState::kPending));
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::ENHANCED);

  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);
  helper_->TriggerIfNeeded();
  task_environment_.FastForwardBy(
      SecuritySettingsBundleToastHelper::kRetryDelay *
      SecuritySettingsBundleToastHelper::kMaxRetries);
  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kPending),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));
}

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
      .WillOnce(testing::DoAll(
          []() {
            base::UmaHistogramEnumeration(
                "Toast.TriggeredToShow",
                ToastId::kEnhancedBundledSecuritySettings);
          },
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
      .WillOnce(testing::DoAll(
          []() {
            base::UmaHistogramEnumeration(
                "Toast.FailedToShow",
                ToastId::kEnhancedBundledSecuritySettings);
          },
          testing::Return(false)))
      .WillOnce(testing::DoAll(
          []() {
            base::UmaHistogramEnumeration(
                "Toast.TriggeredToShow",
                ToastId::kEnhancedBundledSecuritySettings);
          },
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
      .WillRepeatedly(testing::DoAll(
          []() {
            base::UmaHistogramEnumeration(
                "Toast.FailedToShow",
                ToastId::kEnhancedBundledSecuritySettings);
          },
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
