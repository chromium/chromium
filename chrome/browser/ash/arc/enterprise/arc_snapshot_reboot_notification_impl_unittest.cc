// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/arc_snapshot_reboot_notification_impl.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace data_snapshotd {

class ArcSnapshotRebootNotificationTest : public testing::Test {
 public:
  ArcSnapshotRebootNotificationTest() = default;
  ArcSnapshotRebootNotificationTest(const ArcSnapshotRebootNotificationTest&) =
      delete;
  ArcSnapshotRebootNotificationTest& operator=(
      const ArcSnapshotRebootNotificationTest&) = delete;

  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(nullptr);
    tester_->SetNotificationAddedClosure(base::BindRepeating(
        &ArcSnapshotRebootNotificationTest::OnNotificationAdded,
        base::Unretained(this)));
    tester_->SetNotificationClosedClosure(base::BindRepeating(
        &ArcSnapshotRebootNotificationTest::OnNotificationClosed,
        base::Unretained(this)));
  }

  void TearDown() override {
    EXPECT_FALSE(IsNotificationShown());
    tester_.reset();
  }

  void ClickOnNotification() {
    tester_->SimulateClick(
        NotificationHandler::Type::TRANSIENT,
        ArcSnapshotRebootNotificationImpl::get_notification_id_for_testing(),
        absl::nullopt, absl::nullopt);
  }

  void ClickOnRestartButton() {
    tester_->SimulateClick(
        NotificationHandler::Type::TRANSIENT,
        ArcSnapshotRebootNotificationImpl::get_notification_id_for_testing(),
        ArcSnapshotRebootNotificationImpl::get_restart_button_id_for_testing(),
        absl::nullopt);
  }

  void OnNotificationAdded() { is_notification_shown_ = true; }

  void OnNotificationClosed() { is_notification_shown_ = false; }

  bool IsNotificationShown() { return is_notification_shown_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  bool is_notification_shown_ = false;

  std::unique_ptr<NotificationDisplayServiceTester> tester_;
};

TEST_F(ArcSnapshotRebootNotificationTest, Basic) {
  ArcSnapshotRebootNotificationImpl notification;
  EXPECT_FALSE(IsNotificationShown());

  notification.Show();
  EXPECT_TRUE(IsNotificationShown());

  notification.Hide();
  EXPECT_FALSE(IsNotificationShown());
}

TEST_F(ArcSnapshotRebootNotificationTest, ClickOnRestartButton) {
  ArcSnapshotRebootNotificationImpl notification;
  notification.Show();
  EXPECT_TRUE(IsNotificationShown());

  base::RunLoop run_loop;
  notification.SetUserConsentCallback(run_loop.QuitClosure());

  ClickOnRestartButton();
  run_loop.Run();
  EXPECT_FALSE(IsNotificationShown());
}

TEST_F(ArcSnapshotRebootNotificationTest, ClickOnNotification) {
  ArcSnapshotRebootNotificationImpl notification;
  notification.Show();
  EXPECT_TRUE(IsNotificationShown());
  notification.SetUserConsentCallback(base::BindLambdaForTesting(
      []() { NOTREACHED() << "Unexpected user consent registered"; }));
  ClickOnNotification();
  EXPECT_TRUE(IsNotificationShown());
}

TEST_F(ArcSnapshotRebootNotificationTest, DoubleShow) {
  ArcSnapshotRebootNotificationImpl notification;
  EXPECT_FALSE(IsNotificationShown());

  notification.Show();
  notification.Show();
  EXPECT_TRUE(IsNotificationShown());
}

TEST_F(ArcSnapshotRebootNotificationTest, DoubleHide) {
  ArcSnapshotRebootNotificationImpl notification;
  EXPECT_FALSE(IsNotificationShown());

  notification.Show();
  EXPECT_TRUE(IsNotificationShown());

  notification.Hide();
  EXPECT_FALSE(IsNotificationShown());
  notification.Hide();
}

}  // namespace data_snapshotd
}  // namespace arc
