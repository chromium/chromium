// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/features/quick_delete_tip.h"

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
#include "components/browsing_data/core/pref_names.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tips {

class QuickDeleteTipTest : public TipsServiceTestBase {
 public:
  QuickDeleteTipTest() = default;
  ~QuickDeleteTipTest() override = default;

  void SetUp() override {
    TipsServiceTestBase::SetUp();
    browsing_data::prefs::RegisterBrowserUserPrefs(pref_service_.registry());
  }

 protected:
  QuickDeleteTip tip_;
};

TEST_F(QuickDeleteTipTest, GetRank) {
  EXPECT_EQ(tip_.GetRank(), TipFeatureRank::kQuickDelete);
}

TEST_F(QuickDeleteTipTest, GetFeatureType) {
  EXPECT_EQ(tip_.GetFeatureType(), TipsNotificationsFeatureType::kQuickDelete);
}

TEST_F(QuickDeleteTipTest, GetRequiredSignals) {
  auto signals = tip_.GetRequiredSignals();
  ASSERT_EQ(signals.size(), 1u);
  EXPECT_EQ(signals[0].name, QuickDeleteTip::kMagicStackShownHistogram);
  EXPECT_EQ(signals[0].type, SignalType::kHistogramEnum);
  EXPECT_EQ(signals[0].days, QuickDeleteTip::kLookbackDays);
  ASSERT_EQ(signals[0].enum_values.size(), 1u);
  EXPECT_EQ(signals[0].enum_values[0],
            QuickDeleteTip::kQuickDeleteMagicStackImpressionEnumValue);
}

TEST_F(QuickDeleteTipTest, GetNotificationData) {
  notifications::NotificationData data = tip_.GetNotificationData();
  EXPECT_EQ(data.custom_data[notifications::kTipsNotificationsFeatureType],
            base::NumberToString(
                static_cast<int>(TipsNotificationsFeatureType::kQuickDelete)));
  EXPECT_FALSE(data.title.empty());
  EXPECT_FALSE(data.message.empty());
  ASSERT_EQ(data.buttons.size(), 1u);
  EXPECT_EQ(data.buttons[0].type, notifications::ActionButtonType::kHelpful);
}

TEST_F(QuickDeleteTipTest, IsEligible_Eligible) {
  std::map<std::string, float> signals;
  EXPECT_TRUE(tip_.IsEligible(signals, pref_service_));

  signals[QuickDeleteTip::kMagicStackShownHistogram] = 0.0f;
  EXPECT_TRUE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(QuickDeleteTipTest, IsEligible_QuickDeleteEverUsed) {
  pref_service_.SetBoolean(browsing_data::prefs::kQuickDeleteEverUsed, true);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(QuickDeleteTipTest, IsEligible_MagicStackImpression) {
  std::map<std::string, float> signals;
  signals[QuickDeleteTip::kMagicStackShownHistogram] = 1.0f;

  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(QuickDeleteTipTest, IsEligible_TipAlreadyShown) {
  pref_service_.SetBoolean(prefs::kAndroidTipNotificationShownQuickDelete,
                           true);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(QuickDeleteTipTest, IsEligible_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      segmentation_platform::features::kAndroidTipsNotifications);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(QuickDeleteTipTest, IsEligible_FeatureParamDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kAndroidTipsNotifications,
      {{"enable_quick_delete_tip", "false"}});

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

// End-to-end integration test verifying that DetermineBestTip suggests
// QuickDelete as the best tip when evaluated against segmentation platform.
TEST_F(QuickDeleteTipTest, DetermineBestTip_QuickDelete) {
  service_->RegisterFeature(std::make_unique<QuickDeleteTip>());

  EXPECT_EQ(DetermineBestTipSync(), TipsNotificationsFeatureType::kQuickDelete);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) when Quick Delete was ever used.
TEST_F(QuickDeleteTipTest, DetermineBestTip_QuickDelete_NoShowOnEverUsed) {
  service_->RegisterFeature(std::make_unique<QuickDeleteTip>());

  pref_service_.SetBoolean(browsing_data::prefs::kQuickDeleteEverUsed, true);

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) after recording the Magic Stack impression histogram sample.
TEST_F(QuickDeleteTipTest,
       DetermineBestTip_QuickDelete_NoShowOnMagicStackImpression) {
  service_->RegisterFeature(std::make_unique<QuickDeleteTip>());

  // Emit the histogram sample (enum value 9 = QuickDelete).
  RecordHistogramEnum(
      QuickDeleteTip::kMagicStackShownHistogram,
      QuickDeleteTip::kQuickDeleteMagicStackImpressionEnumValue);

  // With the metric recorded in the database, the real database query returns
  // count = 1 (> 0). QuickDelete tip becomes ineligible and no tip is
  // scheduled.
  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

}  // namespace tips
