// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/features/google_lens_tip.h"

#include <map>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_service_test_base.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tips {

class GoogleLensTipTest : public TipsServiceTestBase {
 public:
  GoogleLensTipTest() = default;
  ~GoogleLensTipTest() override = default;

 protected:
  GoogleLensTip tip_;
};

TEST_F(GoogleLensTipTest, GetRank) {
  EXPECT_EQ(tip_.GetRank(), TipFeatureRank::kGoogleLens);
}

TEST_F(GoogleLensTipTest, GetFeatureType) {
  EXPECT_EQ(tip_.GetFeatureType(), TipsNotificationsFeatureType::kGoogleLens);
}

TEST_F(GoogleLensTipTest, GetRequiredSignals) {
  auto signals = tip_.GetRequiredSignals();
  ASSERT_EQ(signals.size(), 4u);

  EXPECT_EQ(signals[0].name, GoogleLensTip::kNewTabPageSearchBoxLensUserAction);
  EXPECT_EQ(signals[0].type, SignalType::kUserAction);
  EXPECT_EQ(signals[0].days, GoogleLensTip::kLookbackDays);

  EXPECT_EQ(signals[1].name, GoogleLensTip::kMobileOmniboxLensUserAction);
  EXPECT_EQ(signals[1].type, SignalType::kUserAction);
  EXPECT_EQ(signals[1].days, GoogleLensTip::kLookbackDays);

  EXPECT_EQ(signals[2].name, GoogleLensTip::kTasksSurfaceFakeBoxLensUserAction);
  EXPECT_EQ(signals[2].type, SignalType::kUserAction);
  EXPECT_EQ(signals[2].days, GoogleLensTip::kLookbackDays);

  EXPECT_EQ(signals[3].name, GoogleLensTip::kNotificationsTipsLensUserAction);
  EXPECT_EQ(signals[3].type, SignalType::kUserAction);
  EXPECT_EQ(signals[3].days, GoogleLensTip::kLookbackDays);
}

TEST_F(GoogleLensTipTest, GetNotificationData) {
  notifications::NotificationData data = tip_.GetNotificationData();
  EXPECT_EQ(data.custom_data[notifications::kTipsNotificationsFeatureType],
            base::NumberToString(
                static_cast<int>(TipsNotificationsFeatureType::kGoogleLens)));
  EXPECT_FALSE(data.title.empty());
  EXPECT_FALSE(data.message.empty());
  ASSERT_EQ(data.buttons.size(), 1u);
  EXPECT_EQ(data.buttons[0].type, notifications::ActionButtonType::kHelpful);
}

TEST_F(GoogleLensTipTest, IsEligible_Eligible) {
  std::map<std::string, float> signals;
  EXPECT_TRUE(tip_.IsEligible(signals, pref_service_));

  signals[GoogleLensTip::kNewTabPageSearchBoxLensUserAction] = 0.0f;
  signals[GoogleLensTip::kMobileOmniboxLensUserAction] = 0.0f;
  signals[GoogleLensTip::kTasksSurfaceFakeBoxLensUserAction] = 0.0f;
  signals[GoogleLensTip::kNotificationsTipsLensUserAction] = 0.0f;
  EXPECT_TRUE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(GoogleLensTipTest, IsEligible_NewTabPageAction) {
  std::map<std::string, float> signals;
  signals[GoogleLensTip::kNewTabPageSearchBoxLensUserAction] = 1.0f;

  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(GoogleLensTipTest, IsEligible_MobileOmniboxAction) {
  std::map<std::string, float> signals;
  signals[GoogleLensTip::kMobileOmniboxLensUserAction] = 1.0f;

  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(GoogleLensTipTest, IsEligible_TasksSurfaceAction) {
  std::map<std::string, float> signals;
  signals[GoogleLensTip::kTasksSurfaceFakeBoxLensUserAction] = 1.0f;

  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(GoogleLensTipTest, IsEligible_NotificationsTipsAction) {
  std::map<std::string, float> signals;
  signals[GoogleLensTip::kNotificationsTipsLensUserAction] = 1.0f;

  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(GoogleLensTipTest, IsEligible_TipAlreadyShown) {
  pref_service_.SetBoolean(prefs::kAndroidTipNotificationShownLens, true);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(GoogleLensTipTest, IsEligible_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      segmentation_platform::features::kAndroidTipsNotifications);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(GoogleLensTipTest, IsEligible_FeatureParamDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kAndroidTipsNotifications,
      {{"enable_google_lens_tip", "false"}});

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

// End-to-end integration test verifying that DetermineBestTip suggests
// GoogleLens as the best tip when evaluated against segmentation platform.
TEST_F(GoogleLensTipTest, DetermineBestTip_GoogleLens) {
  service_->RegisterFeature(std::make_unique<GoogleLensTip>());

  EXPECT_EQ(DetermineBestTipSync(), TipsNotificationsFeatureType::kGoogleLens);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) after recording the NTP searchbox lens user action.
TEST_F(GoogleLensTipTest, DetermineBestTip_GoogleLens_NoShowOnNtpAction) {
  service_->RegisterFeature(std::make_unique<GoogleLensTip>());

  RecordUserAction(GoogleLensTip::kNewTabPageSearchBoxLensUserAction);

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) after recording the mobile omnibox lens user action.
TEST_F(GoogleLensTipTest, DetermineBestTip_GoogleLens_NoShowOnOmniboxAction) {
  service_->RegisterFeature(std::make_unique<GoogleLensTip>());

  RecordUserAction(GoogleLensTip::kMobileOmniboxLensUserAction);

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) after recording the tasks surface lens user action.
TEST_F(GoogleLensTipTest,
       DetermineBestTip_GoogleLens_NoShowOnTasksSurfaceAction) {
  service_->RegisterFeature(std::make_unique<GoogleLensTip>());

  RecordUserAction(GoogleLensTip::kTasksSurfaceFakeBoxLensUserAction);

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) after recording the tips notifications lens user action.
TEST_F(GoogleLensTipTest,
       DetermineBestTip_GoogleLens_NoShowOnTipsNotifsAction) {
  service_->RegisterFeature(std::make_unique<GoogleLensTip>());

  RecordUserAction(GoogleLensTip::kNotificationsTipsLensUserAction);

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) when Google Lens tip has already been shown.
TEST_F(GoogleLensTipTest, DetermineBestTip_GoogleLens_NoShowOnTipShown) {
  service_->RegisterFeature(std::make_unique<GoogleLensTip>());

  pref_service_.SetBoolean(prefs::kAndroidTipNotificationShownLens, true);

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

}  // namespace tips
