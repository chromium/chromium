// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_disk_space_monitor.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcDiskSpaceMonitorTest : public testing::Test {
 public:
  ArcDiskSpaceMonitorTest() = default;
  ~ArcDiskSpaceMonitorTest() override = default;

  ArcDiskSpaceMonitorTest(const ArcDiskSpaceMonitorTest&) = delete;
  ArcDiskSpaceMonitorTest& operator=(const ArcDiskSpaceMonitorTest&) = delete;

  void SetUp() override {
    // Initialize fake clients.
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::SpacedClient::InitializeFake();

    // Set --arc-availability=officially-supported.
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    // Make the session manager skip creating UI.
    ArcSessionManager::SetUiEnabledForTesting(/*enabled=*/false);

    // Enable the ArcEnableVirtioBlkForData feature by default. This can be
    // overridden with another ScopedFeatureList in each test case.
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>(
        kEnableVirtioBlkForData);

    // Initialize a testing profile and a fake user manager.
    // (Required for testing ARC.)
    testing_profile_ = std::make_unique<TestingProfile>();
    const AccountId account_id(
        AccountId::FromUserEmail(testing_profile_->GetProfileUserName()));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        testing_profile_.get());

    // Initialize a session manager with a fake ARC session.
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    arc_session_manager_->SetProfile(testing_profile_.get());
    arc_session_manager_->Initialize();
    arc_session_manager_->RequestEnable();

    // ArcDiskSpaceMonitor should be initialized after the session manager is
    // created.
    arc_disk_space_monitor_ = std::make_unique<ArcDiskSpaceMonitor>();
  }

  void TearDown() override {
    arc_disk_space_monitor_.reset();
    arc_session_manager_.reset();
    notification_tester_.reset();
    testing_profile_.reset();
    scoped_feature_list_.reset();
    ash::SpacedClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    fake_user_manager_.Reset();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  NotificationDisplayServiceTester* notification_tester() const {
    return notification_tester_.get();
  }

  ArcSessionManager* arc_session_manager() const {
    return arc_session_manager_.get();
  }

  ArcDiskSpaceMonitor* arc_disk_space_monitor() const {
    return arc_disk_space_monitor_.get();
  }

  TestingProfile* testing_profile() const { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  session_manager::SessionManager session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcDiskSpaceMonitor> arc_disk_space_monitor_;
};

TEST_F(ArcDiskSpaceMonitorTest, GetFreeDiskSpaceFailed) {
  // spaced::GetFreeDiskSpace fails.
  ash::FakeSpacedClient::Get()->set_free_disk_space(std::nullopt);

  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Wait until ArcDiskSpaceMonitor::CheckDiskSpace() runs.
  base::RunLoop().RunUntilIdle();

  // ARC should keep running but the timer should be stopped.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_disk_space_monitor()->IsTimerRunningForTesting());

  // No notification should be shown.
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePostStopNotificationId));
}

TEST_F(ArcDiskSpaceMonitorTest, FreeSpaceIsHigherThanPreStopNotification) {
  // ThresholdForStoppingArc < ThresholdForPreStopNotification < free_disk_space
  ash::FakeSpacedClient::Get()->set_free_disk_space(
      std::make_optional(kDiskSpaceThresholdForPreStopNotification + 1));

  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Wait until ArcDiskSpaceMonitor::CheckDiskSpace() runs.
  base::RunLoop().RunUntilIdle();

  // ARC should still be active.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // The timer should be running with the long check interval.
  EXPECT_TRUE(arc_disk_space_monitor()->IsTimerRunningForTesting());
  EXPECT_EQ(kDiskSpaceCheckIntervalLong,
            arc_disk_space_monitor()->GetTimerCurrentDelayForTesting());

  // No notification should be shown.
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePostStopNotificationId));
}

TEST_F(ArcDiskSpaceMonitorTest,
       FreeSpaceIsLowerThanThresholdForPreStopNotification) {
  // ThresholdForStoppingArc < free_disk_space < ThresholdForPreStopNotification
  ash::FakeSpacedClient::Get()->set_free_disk_space(
      std::make_optional(kDiskSpaceThresholdForPreStopNotification - 1));

  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Wait until ArcDiskSpaceMonitor::CheckDiskSpace() runs.
  base::RunLoop().RunUntilIdle();

  // ARC should still be active.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_disk_space_monitor()->IsTimerRunningForTesting());

  // The timer should be running with the short check interval.
  EXPECT_EQ(kDiskSpaceCheckIntervalShort,
            arc_disk_space_monitor()->GetTimerCurrentDelayForTesting());

  // A pre-stop warning notification should be shown.
  EXPECT_TRUE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePostStopNotificationId));

  // Remove the notification.
  notification_tester()->RemoveAllNotifications(
      NotificationHandler::Type::TRANSIENT, /*by_user=*/false);

  // Ensure that the warning notification is reshown only after
  // kPreStopNotificationReshowInterval elapses.
  FastForwardBy(kPreStopNotificationReshowInterval - base::Seconds(1));
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
  FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
}

TEST_F(ArcDiskSpaceMonitorTest, FreeSpaceIsLowerThanThresholdForStoppingArc) {
  // free_disk_space < ThresholdForStoppingArc < ThresholdForPreStopNotification
  ash::FakeSpacedClient::Get()->set_free_disk_space(
      std::make_optional(kDiskSpaceThresholdForStoppingArc - 1));

  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Wait until ArcDiskSpaceMonitor::CheckDiskSpace() runs.
  base::RunLoop().RunUntilIdle();

  // Both ARC and the timer should be stopped.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());
  EXPECT_FALSE(arc_disk_space_monitor()->IsTimerRunningForTesting());

  // A post-stop warning notification should be shown.
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
  EXPECT_TRUE(notification_tester()->GetNotification(
      kLowDiskSpacePostStopNotificationId));
}

TEST_F(ArcDiskSpaceMonitorTest, VirtioBlkNotEnabled) {
  // ThresholdForStoppingArc < ThresholdForPreStopNotification < free_disk_space
  ash::FakeSpacedClient::Get()->set_free_disk_space(
      std::make_optional(kDiskSpaceThresholdForPreStopNotification + 1));

  base::test::ScopedFeatureList override_scoped_feature_list;
  override_scoped_feature_list.InitAndDisableFeature(kEnableVirtioBlkForData);

  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  base::RunLoop().RunUntilIdle();

  // ARC should keep running but the timer should be stopped.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_disk_space_monitor()->IsTimerRunningForTesting());

  // No notification should be shown.
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePostStopNotificationId));
}

TEST_F(ArcDiskSpaceMonitorTest, ArcVmDataMigrationNotFinished) {
  // ThresholdForStoppingArc < ThresholdForPreStopNotification < free_disk_space
  ash::FakeSpacedClient::Get()->set_free_disk_space(
      std::make_optional(kDiskSpaceThresholdForPreStopNotification + 1));

  base::test::ScopedFeatureList override_scoped_feature_list;
  override_scoped_feature_list.InitWithFeatures({kEnableArcVmDataMigration},
                                                {kEnableVirtioBlkForData});

  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  base::RunLoop().RunUntilIdle();

  // ARC should keep running but the timer should be stopped.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_disk_space_monitor()->IsTimerRunningForTesting());

  // No notification should be shown.
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePostStopNotificationId));
}

TEST_F(ArcDiskSpaceMonitorTest, ArcVmDataMigrationFinished) {
  // ThresholdForStoppingArc < ThresholdForPreStopNotification < free_disk_space
  ash::FakeSpacedClient::Get()->set_free_disk_space(
      std::make_optional(kDiskSpaceThresholdForPreStopNotification + 1));

  base::test::ScopedFeatureList override_scoped_feature_list;
  override_scoped_feature_list.InitWithFeatures({kEnableArcVmDataMigration},
                                                {kEnableVirtioBlkForData});
  SetArcVmDataMigrationStatus(testing_profile()->GetPrefs(),
                              ArcVmDataMigrationStatus::kFinished);

  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  base::RunLoop().RunUntilIdle();

  // Both ARC and the timer should be running.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_disk_space_monitor()->IsTimerRunningForTesting());

  // No notification should be shown.
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePreStopNotificationId));
  EXPECT_FALSE(notification_tester()->GetNotification(
      kLowDiskSpacePostStopNotificationId));
}

}  // namespace
}  // namespace arc
