// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
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

class PrivacyHubGeolocationControllerTest : public AshTestBase {
 public:
  PrivacyHubGeolocationControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
  }

  ~PrivacyHubGeolocationControllerTest() override = default;

  // AshTest:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = GeolocationPrivacySwitchController::Get();
  }

  void SetUserPref(bool allowed) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kUserGeolocationAllowed, allowed);
  }

  bool GetUserPref() const {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetBoolean(prefs::kUserGeolocationAllowed);
  }

  raw_ptr<GeolocationPrivacySwitchController,
          DanglingUntriaged | ExperimentalAsh>
      controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  const base::HistogramTester histogram_tester_;
};

TEST_F(PrivacyHubGeolocationControllerTest, GetActiveAppsTest) {
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

TEST_F(PrivacyHubGeolocationControllerTest, NotificationOnActivityChangeTest) {
  const std::string app_name = "app";
  SetUserPref(false);
  EXPECT_FALSE(FindNotification());
  controller_->TrackGeolocationAttempted(app_name);
  EXPECT_TRUE(FindNotification());
  controller_->TrackGeolocationRelinquished(app_name);
  EXPECT_FALSE(FindNotification());
}

TEST_F(PrivacyHubGeolocationControllerTest,
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

TEST_F(PrivacyHubGeolocationControllerTest, ClickOnNotificationTest) {
  const std::string app_name = "app";
  SetUserPref(false);
  controller_->TrackGeolocationAttempted(app_name);
  // We didn't log any notification clicks so far.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubGeolocationEnabledFromNotificationHistogram,
                true),
            0);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubGeolocationEnabledFromNotificationHistogram,
                false),
            0);
  EXPECT_TRUE(FindNotification());
  EXPECT_FALSE(GetUserPref());

  // Click on the notification button.
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      PrivacyHubNotificationController::kGeolocationSwitchNotificationId, 0);
  // This must change the user pref.
  EXPECT_TRUE(GetUserPref());
  // The notification should be cleared after it has been clicked on.
  EXPECT_FALSE(FindNotification());

  // The histograms were updated.
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubGeolocationEnabledFromNotificationHistogram,
                true),
            1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                privacy_hub_metrics::
                    kPrivacyHubGeolocationEnabledFromNotificationHistogram,
                false),
            0);
}

}  // namespace ash
