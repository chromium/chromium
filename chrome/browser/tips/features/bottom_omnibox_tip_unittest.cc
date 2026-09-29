// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/features/bottom_omnibox_tip.h"

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
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tips {

class BottomOmniboxTipTest : public TipsServiceTestBase {
 public:
  BottomOmniboxTipTest() = default;
  ~BottomOmniboxTipTest() override = default;

  void SetUp() override {
    TipsServiceTestBase::SetUp();
    omnibox::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  BottomOmniboxTip tip_;
};

TEST_F(BottomOmniboxTipTest, GetRank) {
  EXPECT_EQ(tip_.GetRank(), TipFeatureRank::kBottomOmnibox);
}

TEST_F(BottomOmniboxTipTest, GetFeatureType) {
  EXPECT_EQ(tip_.GetFeatureType(),
            TipsNotificationsFeatureType::kBottomOmnibox);
}

TEST_F(BottomOmniboxTipTest, GetRequiredSignals) {
  auto signals = tip_.GetRequiredSignals();
  EXPECT_TRUE(signals.empty());
}

TEST_F(BottomOmniboxTipTest, GetNotificationData) {
  notifications::NotificationData data = tip_.GetNotificationData();
  EXPECT_EQ(data.custom_data[notifications::kTipsNotificationsFeatureType],
            base::NumberToString(static_cast<int>(
                TipsNotificationsFeatureType::kBottomOmnibox)));
  EXPECT_FALSE(data.title.empty());
  EXPECT_FALSE(data.message.empty());
  ASSERT_EQ(data.buttons.size(), 1u);
  EXPECT_EQ(data.buttons[0].type, notifications::ActionButtonType::kHelpful);
}

TEST_F(BottomOmniboxTipTest, IsEligible_Eligible) {
  std::map<std::string, float> signals;
  EXPECT_TRUE(tip_.IsEligible(signals, pref_service_));

  signals[segmentation_platform::kBottomOmniboxStatus] = 0.0f;
  EXPECT_TRUE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(BottomOmniboxTipTest, IsEligible_BottomOmniboxActive) {
  std::map<std::string, float> signals;
  signals[segmentation_platform::kBottomOmniboxStatus] = 1.0f;

  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(BottomOmniboxTipTest, IsEligible_EverUsed) {
  pref_service_.SetBoolean(omnibox::kBottomOmniboxEverUsed, true);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(BottomOmniboxTipTest, IsEligible_TipAlreadyShown) {
  pref_service_.SetBoolean(prefs::kAndroidTipNotificationShownBottomOmnibox,
                           true);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(BottomOmniboxTipTest, IsEligible_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      segmentation_platform::features::kAndroidTipsNotifications);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(BottomOmniboxTipTest, IsEligible_FeatureParamDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kAndroidTipsNotifications,
      {{"enable_bottom_omnibox_tip", "false"}});

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

// End-to-end integration test verifying that DetermineBestTip suggests
// BottomOmnibox as the best tip when evaluated against segmentation platform.
TEST_F(BottomOmniboxTipTest, DetermineBestTip_BottomOmnibox) {
  service_->RegisterFeature(std::make_unique<BottomOmniboxTip>());

  EXPECT_EQ(DetermineBestTipSync(),
            TipsNotificationsFeatureType::kBottomOmnibox);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) when Bottom Omnibox is currently active via custom runtime signal.
TEST_F(BottomOmniboxTipTest, DetermineBestTip_BottomOmnibox_NoShowWhenActive) {
  service_->RegisterFeature(std::make_unique<BottomOmniboxTip>());

  std::map<std::string, float> custom_signals = {
      {segmentation_platform::kBottomOmniboxStatus, 1.0f},
  };

  EXPECT_EQ(DetermineBestTipSync(custom_signals), std::nullopt);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) when Bottom Omnibox was ever used.
TEST_F(BottomOmniboxTipTest, DetermineBestTip_BottomOmnibox_NoShowOnEverUsed) {
  service_->RegisterFeature(std::make_unique<BottomOmniboxTip>());

  pref_service_.SetBoolean(omnibox::kBottomOmniboxEverUsed, true);

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) when Bottom Omnibox tip has already been shown.
TEST_F(BottomOmniboxTipTest, DetermineBestTip_BottomOmnibox_NoShowOnTipShown) {
  service_->RegisterFeature(std::make_unique<BottomOmniboxTip>());

  pref_service_.SetBoolean(prefs::kAndroidTipNotificationShownBottomOmnibox,
                           true);

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

}  // namespace tips
