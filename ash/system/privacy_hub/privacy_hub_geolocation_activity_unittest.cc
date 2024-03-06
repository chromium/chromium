// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using testing::_;

namespace ash {

namespace {

message_center::Notification* FindNotification() {
  return message_center::MessageCenter::Get()->FindNotificationById(
      PrivacyHubNotificationController::kGeolocationSwitchNotificationId);
}

}  // namespace

class PrivacyHubGeolocationTestBase : public AshTestBase {
public:
  PrivacyHubGeolocationTestBase()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures(
        { features::kCrosPrivacyHub}, {});
  }

  ~PrivacyHubGeolocationTestBase() override = default;

  // AshTest:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = GeolocationPrivacySwitchController::Get();
  }

  void SetAccessLevel(GeolocationAccessLevel access_level) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
        prefs::kUserGeolocationAccessLevel, static_cast<int>(access_level));
  }

  GeolocationAccessLevel GetAccessLevel() {
    return static_cast<GeolocationAccessLevel>(
        Shell::Get()->session_controller()->GetActivePrefService()->GetInteger(
            prefs::kUserGeolocationAccessLevel));
  }

 protected:
  raw_ptr<GeolocationPrivacySwitchController, DanglingUntriaged> controller_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class PrivacyHubGeolocationActivityTest : public PrivacyHubGeolocationTestBase,
      public testing::WithParamInterface<GeolocationAccessLevel> {
 public:
  // TODO(b/323169598): Review all uses of this function and rewrite the tests
  // to check all access levels using GetAccessLevel().
  void SetUserPref(bool allowed) {
    GeolocationAccessLevel access_level;
    if (allowed) {
      access_level = GeolocationAccessLevel::kAllowed;
    } else {
      access_level = static_cast<GeolocationAccessLevel>(GetParam());
      ASSERT_NE(access_level, GeolocationAccessLevel::kAllowed);
    }
    SetAccessLevel(access_level);
  }

  const base::HistogramTester histogram_tester_;
};

TEST_P(PrivacyHubGeolocationActivityTest, GetActiveAppsTest) {
  EXPECT_TRUE(features::IsCrosPrivacyHubLocationEnabled());
  const std::vector<std::string> app_names{"App1", "App2", "App3"};
  const std::vector<std::u16string> app_names_u16{u"App1", u"App2", u"App3"};
  EXPECT_EQ(controller_->GetActiveApps(3), (std::vector<std::u16string>{}));
  controller_->TrackGeolocationAttempted(app_names[0]);
  EXPECT_EQ(controller_->GetActiveApps(3),
            (std::vector<std::u16string>{app_names_u16[0]}));
  controller_->TrackGeolocationAttempted(app_names[1]);
  EXPECT_EQ(controller_->GetActiveApps(3),
            (std::vector<std::u16string>{app_names_u16[0], app_names_u16[1]}));
  controller_->TrackGeolocationAttempted(app_names[1]);
  EXPECT_EQ(controller_->GetActiveApps(3),
            (std::vector<std::u16string>{app_names_u16[0], app_names_u16[1]}));
  controller_->TrackGeolocationAttempted(app_names[2]);
  EXPECT_EQ(controller_->GetActiveApps(3), app_names_u16);
  controller_->TrackGeolocationRelinquished(app_names[2]);
  EXPECT_EQ(controller_->GetActiveApps(3),
            (std::vector<std::u16string>{app_names_u16[0], app_names_u16[1]}));
  controller_->TrackGeolocationRelinquished(app_names[1]);
  EXPECT_EQ(controller_->GetActiveApps(3),
            (std::vector<std::u16string>{app_names_u16[0], app_names_u16[1]}));
  controller_->TrackGeolocationRelinquished(app_names[1]);
  EXPECT_EQ(controller_->GetActiveApps(3),
            (std::vector<std::u16string>{app_names_u16[0]}));
  controller_->TrackGeolocationRelinquished(app_names[0]);
  EXPECT_EQ(controller_->GetActiveApps(3), (std::vector<std::u16string>{}));
}

TEST_P(PrivacyHubGeolocationActivityTest, NotificationOnActivityChangeTest) {
  const std::string app_name = "app";
  SetUserPref(false);
  EXPECT_FALSE(FindNotification());
  controller_->TrackGeolocationAttempted(app_name);
  EXPECT_TRUE(FindNotification());
  controller_->TrackGeolocationRelinquished(app_name);
  EXPECT_FALSE(FindNotification());
}

TEST_P(PrivacyHubGeolocationActivityTest,
       NotificationOnPreferenceChangeTest) {
  const std::string app_name = "app";
  SetUserPref(true);
  controller_->TrackGeolocationAttempted(app_name);
  EXPECT_FALSE(FindNotification());
  SetUserPref(false);
  EXPECT_TRUE(FindNotification());
  SetUserPref(true);
  EXPECT_FALSE(FindNotification());
}

TEST_P(PrivacyHubGeolocationActivityTest, ClickOnNotificationTest) {
  const std::string app_name = "app";
  SetUserPref(false);
  EXPECT_TRUE(features::IsCrosPrivacyHubLocationEnabled());
  EXPECT_TRUE(controller_);
  controller_->TrackGeolocationAttempted(app_name);

  const auto kGeolocationAccessLevels = {
      GeolocationAccessLevel::kDisallowed, GeolocationAccessLevel::kAllowed,
      GeolocationAccessLevel::kOnlyAllowedForSystem};
  ASSERT_EQ(static_cast<unsigned long>(GeolocationAccessLevel::kMaxValue) + 1,
            kGeolocationAccessLevels.size());

  // We didn't log any notification clicks so far.
  for (auto access_level : kGeolocationAccessLevels) {
    EXPECT_EQ(0,
              histogram_tester_.GetBucketCount(
                  privacy_hub_metrics::
                      kPrivacyHubGeolocationAccessLevelChangedFromNotification,
                  access_level));
  }
  EXPECT_TRUE(FindNotification());
  EXPECT_NE(GetAccessLevel(), GeolocationAccessLevel::kAllowed);

  // Click on the notification button.
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      PrivacyHubNotificationController::kGeolocationSwitchNotificationId, 0);
  // This must change the user pref.
  EXPECT_EQ(GetAccessLevel(), GeolocationAccessLevel::kAllowed);
  // The notification should be cleared after it has been clicked on.
  EXPECT_FALSE(FindNotification());

  // The histograms were updated.
  EXPECT_EQ(1, histogram_tester_.GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubGeolocationAccessLevelChangedFromNotification,
                   GeolocationAccessLevel::kAllowed));
  EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubGeolocationAccessLevelChangedFromNotification,
                   GeolocationAccessLevel::kDisallowed));
  EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                   privacy_hub_metrics::
                       kPrivacyHubGeolocationAccessLevelChangedFromNotification,
                   GeolocationAccessLevel::kOnlyAllowedForSystem));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrivacyHubGeolocationActivityTest,
                         testing::Values( GeolocationAccessLevel::kDisallowed, GeolocationAccessLevel::kOnlyAllowedForSystem));

using BooleanSyncTransitionTableRow = std::tuple<GeolocationAccessLevel,
                                                 GeolocationAccessLevel,
                                                 bool,
                                                 GeolocationAccessLevel,
                                                 GeolocationAccessLevel>;

class PrivacyHubGeolocationApplyArcLocationUpdatesTest
    : public PrivacyHubGeolocationTestBase,
      public testing::WithParamInterface<BooleanSyncTransitionTableRow> {
 public:
  GeolocationAccessLevel PreviousAccessLevel() const {
    return std::get<0>(GetParam());
  }
  GeolocationAccessLevel CurrentAccessLevel() const {
    return std::get<1>(GetParam());
  }
  bool IncomingValueToBeSynced() const { return std::get<2>(GetParam()); }
  GeolocationAccessLevel ExpectedNewPreviousAccessLevel() const {
    return std::get<3>(GetParam());
  }
  GeolocationAccessLevel ExpectedNewAccessLevel() const {
    return std::get<4>(GetParam());
  }
};

TEST_P(PrivacyHubGeolocationApplyArcLocationUpdatesTest, UpdateTest) {
  ASSERT_NE(CurrentAccessLevel(), PreviousAccessLevel());
  // Test initial values
  EXPECT_EQ(GeolocationAccessLevel::kAllowed, controller_->AccessLevel());
  EXPECT_EQ(GeolocationAccessLevel::kDisallowed,
            controller_->PreviousAccessLevel());

  SetAccessLevel(PreviousAccessLevel());
  SetAccessLevel(CurrentAccessLevel());
  EXPECT_EQ(CurrentAccessLevel(), controller_->AccessLevel());
  EXPECT_EQ(PreviousAccessLevel(), controller_->PreviousAccessLevel());

  controller_->ApplyArcLocationUpdate(IncomingValueToBeSynced());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ExpectedNewAccessLevel(), controller_->AccessLevel());
  EXPECT_EQ(ExpectedNewPreviousAccessLevel(),
            controller_->PreviousAccessLevel());
}

using BooleanSyncTransitionTable = std::vector<BooleanSyncTransitionTableRow>;
// Utility function to generate all possible perameter combinations for the
// PrivacyHubGeolocationApplyArcLocationUpdatesTest test suite.
BooleanSyncTransitionTable GenerateTransitionTable() {
  BooleanSyncTransitionTable table;
  const std::array<GeolocationAccessLevel, 3> all_levels{
      GeolocationAccessLevel::kAllowed, GeolocationAccessLevel::kDisallowed,
      GeolocationAccessLevel::kOnlyAllowedForSystem};
  const auto is_blocking = [](GeolocationAccessLevel level) {
    return (level != GeolocationAccessLevel::kAllowed);
  };
  for (const GeolocationAccessLevel previous_level : all_levels) {
    for (const GeolocationAccessLevel current_level : all_levels) {
      if (previous_level == current_level) {
        // The GeolocationPrivacySwitch ensures that the previous and current
        // levels are different
        continue;
      }
      for (const bool incoming_enable : {true, false}) {
        const bool is_noop = (incoming_enable && !is_blocking(current_level)) ||
                             (!incoming_enable && is_blocking(current_level));
        const GeolocationAccessLevel expected_new_current_level =
            is_noop ? current_level
                    : (incoming_enable ? GeolocationAccessLevel::kAllowed
                                       : previous_level);
        const GeolocationAccessLevel expected_new_previous_level =
            is_noop ? previous_level : current_level;
        table.push_back(std::make_tuple(
            previous_level, current_level, incoming_enable,
            expected_new_previous_level, expected_new_current_level));
      }
    }
  }
  return table;
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         PrivacyHubGeolocationApplyArcLocationUpdatesTest,
                         testing::ValuesIn(GenerateTransitionTable()));

}  // namespace ash
