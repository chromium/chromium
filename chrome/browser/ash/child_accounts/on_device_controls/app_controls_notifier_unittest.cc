// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_notifier.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace {
constexpr char kEligibleDeviceRegionKey[] = "gp";
constexpr char kShowNotificationId[] = "show_app_controls_notification";
constexpr int kOpenSettingsButtonIndex = 0;

constexpr char kNotificationClickedActionName[] =
    "OnDeviceControls_NotificationClicked";

constexpr char kNotificationShownActionName[] =
    "OnDeviceControls_NotificationShown";
}  // namespace

namespace ash::on_device_controls {

class AppControlsNotifierTest : public BrowserWithTestWindowTest {
 public:
  AppControlsNotifierTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kOnDeviceAppControls);

    statistics_provider_.SetMachineStatistic(ash::system::kRegionKey,
                                             kEligibleDeviceRegionKey);
  }
  AppControlsNotifierTest(const AppControlsNotifierTest&) = delete;
  AppControlsNotifierTest& operator=(const AppControlsNotifierTest&) = delete;
  ~AppControlsNotifierTest() override = default;

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(profile());
    app_controls_notifier_ = std::make_unique<AppControlsNotifier>(profile());
  }

  void TearDown() override {
    app_controls_notifier_.reset();
    tester_.reset();
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(nullptr);
    BrowserWithTestWindowTest::TearDown();
  }

  AppControlsNotifier* app_controls_notifier() {
    return app_controls_notifier_.get();
  }

 protected:
  void ClickOpenSettingsButton() {
    app_controls_notifier_->HandleClick(kOpenSettingsButtonIndex);
  }

  bool IsAppControlsNotificationPresent() const {
    return tester_->GetNotification(kShowNotificationId).has_value();
  }
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<AppControlsNotifier> app_controls_notifier_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
};

TEST_F(AppControlsNotifierTest, ShowAppControlsNotification) {
  base::UserActionTester user_action_tester;
  app_controls_notifier()->MaybeShowAppControlsNotification();

  EXPECT_TRUE(IsAppControlsNotificationPresent());

  EXPECT_EQ(1, user_action_tester.GetActionCount(kNotificationShownActionName));
}

TEST_F(AppControlsNotifierTest, ClickAppControlsNotification) {
  base::UserActionTester user_action_tester;
  app_controls_notifier()->MaybeShowAppControlsNotification();

  EXPECT_TRUE(IsAppControlsNotificationPresent());

  ClickOpenSettingsButton();

  EXPECT_EQ(1,
            user_action_tester.GetActionCount(kNotificationClickedActionName));
  // Notification should be removed on `Open Settings` click.
  EXPECT_FALSE(IsAppControlsNotificationPresent());
}

TEST_F(AppControlsNotifierTest,
       AppControlsNotificationDoesNotShowAgainAfterBeingClicked) {
  base::UserActionTester user_action_tester;
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kOnDeviceAppControlsNotificationShown));

  app_controls_notifier()->MaybeShowAppControlsNotification();
  EXPECT_TRUE(IsAppControlsNotificationPresent());

  ClickOpenSettingsButton();

  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kOnDeviceAppControlsNotificationShown));

  app_controls_notifier()->MaybeShowAppControlsNotification();

  // Notification should have only been shown once.
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNotificationShownActionName));
}

class AppControlsNotifierDisabledTest : public AppControlsNotifierTest {
 public:
  AppControlsNotifierDisabledTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(features::kOnDeviceAppControls);
  }
  AppControlsNotifierDisabledTest(const AppControlsNotifierDisabledTest&) =
      delete;
  AppControlsNotifierDisabledTest& operator=(
      const AppControlsNotifierDisabledTest&) = delete;
  ~AppControlsNotifierDisabledTest() override = default;
};

TEST_F(AppControlsNotifierDisabledTest,
       AppControlsNotificationDoesNotShowWhenFeatureDisabled) {
  app_controls_notifier()->MaybeShowAppControlsNotification();

  EXPECT_FALSE(IsAppControlsNotificationPresent());
}
}  // namespace ash::on_device_controls
