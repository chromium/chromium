// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/features/enhanced_safe_browsing_tip.h"

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
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tips {

class EnhancedSafeBrowsingTipTest : public TipsServiceTestBase {
 public:
  EnhancedSafeBrowsingTipTest() = default;
  ~EnhancedSafeBrowsingTipTest() override = default;

  void SetUp() override {
    TipsServiceTestBase::SetUp();
    safe_browsing::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  EnhancedSafeBrowsingTip tip_;
};

TEST_F(EnhancedSafeBrowsingTipTest, GetRank) {
  EXPECT_EQ(tip_.GetRank(), TipFeatureRank::kEnhancedSafeBrowsing);
}

TEST_F(EnhancedSafeBrowsingTipTest, GetFeatureType) {
  EXPECT_EQ(tip_.GetFeatureType(),
            TipsNotificationsFeatureType::kEnhancedSafeBrowsing);
}

TEST_F(EnhancedSafeBrowsingTipTest, GetRequiredSignals) {
  auto signals = tip_.GetRequiredSignals();
  ASSERT_EQ(signals.size(), 1u);
  EXPECT_EQ(signals[0].name,
            EnhancedSafeBrowsingTip::kEnhancedProtectionClickedUserAction);
  EXPECT_EQ(signals[0].type, SignalType::kUserAction);
  EXPECT_EQ(signals[0].days, EnhancedSafeBrowsingTip::kLookbackDays);
}

TEST_F(EnhancedSafeBrowsingTipTest, GetNotificationData) {
  notifications::NotificationData data = tip_.GetNotificationData();
  EXPECT_EQ(data.custom_data[notifications::kTipsNotificationsFeatureType],
            base::NumberToString(static_cast<int>(
                TipsNotificationsFeatureType::kEnhancedSafeBrowsing)));
  EXPECT_FALSE(data.title.empty());
  EXPECT_FALSE(data.message.empty());
  ASSERT_EQ(data.buttons.size(), 1u);
  EXPECT_EQ(data.buttons[0].type, notifications::ActionButtonType::kHelpful);
}

TEST_F(EnhancedSafeBrowsingTipTest, IsEligible_Eligible) {
  std::map<std::string, float> signals;
  EXPECT_TRUE(tip_.IsEligible(signals, pref_service_));

  signals[EnhancedSafeBrowsingTip::kEnhancedProtectionClickedUserAction] = 0.0f;
  EXPECT_TRUE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(EnhancedSafeBrowsingTipTest, IsEligible_AlreadyEnabled) {
  safe_browsing::SetSafeBrowsingState(
      &pref_service_, safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(EnhancedSafeBrowsingTipTest, IsEligible_RecentUserAction) {
  std::map<std::string, float> signals;
  signals[EnhancedSafeBrowsingTip::kEnhancedProtectionClickedUserAction] = 1.0f;

  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(EnhancedSafeBrowsingTipTest, IsEligible_TipAlreadyShown) {
  pref_service_.SetBoolean(prefs::kAndroidTipNotificationShownESB, true);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(EnhancedSafeBrowsingTipTest, IsEligible_PolicyManaged) {
  pref_service_.SetManagedPref(::prefs::kSafeBrowsingEnabled,
                               base::Value(true));

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(EnhancedSafeBrowsingTipTest, IsEligible_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      segmentation_platform::features::kAndroidTipsNotifications);

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

TEST_F(EnhancedSafeBrowsingTipTest, IsEligible_FeatureParamDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kAndroidTipsNotifications,
      {{"enable_enhanced_safe_browsing_tip", "false"}});

  std::map<std::string, float> signals;
  EXPECT_FALSE(tip_.IsEligible(signals, pref_service_));
}

// End-to-end integration test verifying that DetermineBestTip suggests ESB as
// the best tip when evaluated against real segmentation platform database
// calls.
TEST_F(EnhancedSafeBrowsingTipTest, DetermineBestTip_EnhancedSafeBrowsing) {
  // Register EnhancedSafeBrowsingTip.
  service_->RegisterFeature(std::make_unique<EnhancedSafeBrowsingTip>());

  // Initially, no user action has been emitted to the database. The real
  // segmentation platform SQLite query returns count = 0 for
  // "SafeBrowsing.Settings.EnhancedProtectionClicked".
  // ESB tip should be eligible and scheduled as the best tip.
  EXPECT_EQ(DetermineBestTipSync(),
            TipsNotificationsFeatureType::kEnhancedSafeBrowsing);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// (no show) after recording the user action via real database calls.
TEST_F(EnhancedSafeBrowsingTipTest,
       DetermineBestTip_EnhancedSafeBrowsing_NoShow) {
  // Register EnhancedSafeBrowsingTip.
  service_->RegisterFeature(std::make_unique<EnhancedSafeBrowsingTip>());

  // Emit the user action via standard base::RecordAction. Because this signal
  // is registered in TipsNotificationsRanker, UserActionSignalHandler observes
  // and stores it in the SQLite database.
  RecordUserAction(
      EnhancedSafeBrowsingTip::kEnhancedProtectionClickedUserAction);

  // With the metric recorded in the database, the real database query returns
  // count = 1 (> 0). ESB tip becomes ineligible and no tip is scheduled.
  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

// End-to-end integration test verifying that DetermineBestTip returns nullopt
// when Safe Browsing is managed by enterprise policy.
TEST_F(EnhancedSafeBrowsingTipTest,
       DetermineBestTip_EnhancedSafeBrowsing_PolicyManaged) {
  // Register EnhancedSafeBrowsingTip.
  service_->RegisterFeature(std::make_unique<EnhancedSafeBrowsingTip>());

  pref_service_.SetManagedPref(::prefs::kSafeBrowsingEnabled,
                               base::Value(true));

  EXPECT_EQ(DetermineBestTipSync(), std::nullopt);
}

}  // namespace tips
