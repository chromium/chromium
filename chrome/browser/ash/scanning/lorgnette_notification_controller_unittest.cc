// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_notification_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/constants/lorgnette_dlc.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

using ::dlcservice::DlcState;
using ::testing::_;

namespace ash {
namespace {
using ::message_center::Notification;
}  // namespace

class LorgnetteNotificationControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    DlcserviceClient::InitializeFake();
    notification_controller_ =
        std::make_unique<LorgnetteNotificationController>(&profile_);
  }

  void TearDown() override {
    notification_controller_.reset();
    DlcserviceClient::Shutdown();
  }

  std::optional<Notification> Notification() {
    return NotificationDisplayServiceTester::Get()->GetNotification(
        "scanning_dlc_notification");
  }

  dlcservice::DlcState CreateDlcState(dlcservice::DlcState_State state) {
    DlcState output;
    output.set_state(state);
    output.set_id(lorgnette::kSaneBackendsPfuDlcId);
    return output;
  }

  dlcservice::DlcState CreateInstalledState() {
    return CreateDlcState(dlcservice::DlcState_State_INSTALLED);
  }

  dlcservice::DlcState CreateInstallingState() {
    return CreateDlcState(dlcservice::DlcState_State_INSTALLING);
  }

  dlcservice::DlcState CreateInstallErrorState() {
    return CreateDlcState(dlcservice::DlcState_State_NOT_INSTALLED);
  }

  dlcservice::DlcState CreateInstalledStateWrongId() {
    auto output = CreateDlcState(dlcservice::DlcState_State_INSTALLED);
    output.set_id("incorrect-id");
    return output;
  }

  void InstallDlcWithState(dlcservice::DlcState state) {
    fake_dlcservice_client()->NotifyObserversForTest(state);
  }

  FakeDlcserviceClient* fake_dlcservice_client() {
    return static_cast<FakeDlcserviceClient*>(DlcserviceClient::Get());
  }

  std::unique_ptr<LorgnetteNotificationController> notification_controller_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  NotificationDisplayServiceTester display_service_tester_{&profile_};
};

TEST_F(LorgnetteNotificationControllerTest, TestDlcSuccessfullyInstalled) {
  InstallDlcWithState(CreateInstalledState());

  ASSERT_TRUE(notification_controller_
                  ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                  .has_value());
  EXPECT_EQ(notification_controller_
                ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                .value(),
            LorgnetteNotificationController::DlcState::kIdle);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(LorgnetteNotificationControllerTest, TestDlcInstalling) {
  InstallDlcWithState(CreateInstallingState());

  ASSERT_TRUE(notification_controller_
                  ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                  .has_value());
  EXPECT_EQ(notification_controller_
                ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                .value(),
            LorgnetteNotificationController::DlcState::kInstalling);
  ASSERT_TRUE(Notification().has_value());
  EXPECT_EQ(u"Installing scanner software", Notification()->title());
  EXPECT_EQ(u"", Notification()->message());
  EXPECT_EQ(cros_tokens::kCrosSysPrimary, Notification()->accent_color_id());
  EXPECT_EQ(&kNotificationPrintingIcon, &Notification()->vector_small_image());
}

TEST_F(LorgnetteNotificationControllerTest, TestDlcInstallFailed) {
  InstallDlcWithState(CreateInstallErrorState());

  ASSERT_TRUE(notification_controller_
                  ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                  .has_value());
  EXPECT_EQ(notification_controller_
                ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                .value(),
            LorgnetteNotificationController::DlcState::kInstallError);
  ASSERT_TRUE(Notification().has_value());
  EXPECT_EQ(u"Can't install scanner software", Notification()->title());
  EXPECT_EQ(u"Unplug the scanner's USB cable and re-plug it to retry",
            Notification()->message());
  EXPECT_EQ(cros_tokens::kCrosSysError, Notification()->accent_color_id());
  EXPECT_EQ(&kNotificationPrintingWarningIcon,
            &Notification()->vector_small_image());
}

TEST_F(LorgnetteNotificationControllerTest, TestWrongIdIntalled) {
  InstallDlcWithState(CreateInstalledStateWrongId());

  ASSERT_TRUE(notification_controller_
                  ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                  .has_value());
  EXPECT_EQ(notification_controller_
                ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                .value(),
            LorgnetteNotificationController::DlcState::kIdle);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(LorgnetteNotificationControllerTest, TestRealDlcFlow) {
  InstallDlcWithState(CreateInstallingState());
  ASSERT_TRUE(notification_controller_
                  ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                  .has_value());
  EXPECT_EQ(notification_controller_
                ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                .value(),
            LorgnetteNotificationController::DlcState::kInstalling);
  ASSERT_TRUE(Notification().has_value());
  EXPECT_EQ(u"Installing scanner software", Notification()->title());
  EXPECT_EQ(u"", Notification()->message());
  EXPECT_EQ(cros_tokens::kCrosSysPrimary, Notification()->accent_color_id());
  EXPECT_EQ(&kNotificationPrintingIcon, &Notification()->vector_small_image());

  InstallDlcWithState(CreateInstalledState());
  ASSERT_TRUE(notification_controller_
                  ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                  .has_value());
  EXPECT_EQ(notification_controller_
                ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                .value(),
            LorgnetteNotificationController::DlcState::kInstalledSuccessfully);
  ASSERT_TRUE(Notification().has_value());
  EXPECT_EQ(u"Scanner software installed", Notification()->title());
  EXPECT_EQ(u"", Notification()->message());
  EXPECT_EQ(cros_tokens::kCrosSysPrimary, Notification()->accent_color_id());
  EXPECT_EQ(&kNotificationPrintingIcon, &Notification()->vector_small_image());

  // If Install Called again, change back to Idle state and remove notification
  InstallDlcWithState(CreateInstalledState());
  ASSERT_TRUE(notification_controller_
                  ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                  .has_value());
  EXPECT_EQ(notification_controller_
                ->current_state_for_testing(lorgnette::kSaneBackendsPfuDlcId)
                .value(),
            LorgnetteNotificationController::DlcState::kIdle);
  EXPECT_FALSE(Notification().has_value());
}

}  // namespace ash
