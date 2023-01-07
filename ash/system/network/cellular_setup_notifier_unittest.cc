// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/cellular_setup_notifier.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

const char kShillManagerClientStubCellularDevice[] =
    "/device/stub_cellular_device";
const char kShillManagerClientStubCellularDeviceName[] = "stub_cellular_device";

}  // namespace

class CellularSetupNotifierTest : public NoSessionAshTestBase {
 protected:
  CellularSetupNotifierTest() = default;
  CellularSetupNotifierTest(const CellularSetupNotifierTest&) = delete;
  CellularSetupNotifierTest& operator=(const CellularSetupNotifierTest&) =
      delete;
  ~CellularSetupNotifierTest() override = default;

  void SetUp() override {
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();
    NetworkHandler::Initialize();
    network_config_helper_ = std::make_unique<
        chromeos::network_config::CrosNetworkConfigTestHelper>();

    AshTestBase::SetUp();

    auto mock_notification_timer = std::make_unique<base::MockOneShotTimer>();
    mock_notification_timer_ = mock_notification_timer.get();
    Shell::Get()
        ->system_notification_controller()
        ->cellular_setup_notifier_->SetTimerForTesting(
            std::move(mock_notification_timer));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    network_config_helper_.reset();
    NetworkHandler::Shutdown();
    hermes_clients::Shutdown();
    shill_clients::Shutdown();
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
  }

  // Returns the cellular setup notification if it is shown, and null if it is
  // not shown.
  message_center::Notification* GetCellularSetupNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        CellularSetupNotifier::kCellularSetupNotificationId);
  }

  void LogIn() { SimulateUserLogin("user1@test.com"); }

  void LogOut() { ClearLogin(); }

  void LogInAndFireTimer() {
    LogIn();
    EXPECT_TRUE(GetCanCellularSetupNotificationBeShown());

    ASSERT_TRUE(mock_notification_timer_->IsRunning());
    mock_notification_timer_->Fire();
    // Wait for the async network calls to complete.
    base::RunLoop().RunUntilIdle();
  }

  bool GetCanCellularSetupNotificationBeShown() {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    return prefs->GetBoolean(prefs::kCanCellularSetupNotificationBeShown);
  }

  void SetCanCellularSetupNotificationBeShown(bool value) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    prefs->SetBoolean(prefs::kCanCellularSetupNotificationBeShown, value);
  }

  // Ownership passed to Shell owned CellularSetupNotifier instance.
  base::MockOneShotTimer* mock_notification_timer_;

  std::unique_ptr<chromeos::network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
};

TEST_F(CellularSetupNotifierTest, DontShowNotificationUnfinishedOOBE) {
  ASSERT_FALSE(mock_notification_timer_->IsRunning());

  message_center::Notification* notification = GetCellularSetupNotification();
  EXPECT_FALSE(notification);
}

TEST_F(CellularSetupNotifierTest, ShowNotificationUnactivatedNetwork) {
  network_config_helper_->network_state_helper().AddDevice(
      kShillManagerClientStubCellularDevice, shill::kTypeCellular,
      kShillManagerClientStubCellularDeviceName);

  LogInAndFireTimer();

  message_center::Notification* notification = GetCellularSetupNotification();
  EXPECT_TRUE(notification);
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, DontShowNotificationActivatedNetwork) {
  network_config_helper_->network_state_helper().AddDevice(
      kShillManagerClientStubCellularDevice, shill::kTypeCellular,
      kShillManagerClientStubCellularDeviceName);
  const std::string& cellular_path_ =
      network_config_helper_->network_state_helper().ConfigureService(
          R"({"GUID": "cellular_guid", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})");
  network_config_helper_->network_state_helper().SetServiceProperty(
      cellular_path_, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));

  LogInAndFireTimer();

  message_center::Notification* notification = GetCellularSetupNotification();
  EXPECT_FALSE(notification);
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, ShowNotificationMultipleUnactivatedNetworks) {
  network_config_helper_->network_state_helper().AddDevice(
      kShillManagerClientStubCellularDevice, shill::kTypeCellular,
      kShillManagerClientStubCellularDeviceName);
  network_config_helper_->network_state_helper().ConfigureService(
      R"({"GUID": "cellular_guid", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})");
  network_config_helper_->network_state_helper().ConfigureService(
      R"({"GUID": "cellular_guid1", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})");

  LogInAndFireTimer();

  message_center::Notification* notification = GetCellularSetupNotification();
  EXPECT_TRUE(notification);
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, LogOutBeforeNotificationShowsLogInAgain) {
  network_config_helper_->network_state_helper().AddDevice(
      kShillManagerClientStubCellularDevice, shill::kTypeCellular,
      kShillManagerClientStubCellularDeviceName);

  LogIn();
  ASSERT_TRUE(mock_notification_timer_->IsRunning());

  LogOut();
  ASSERT_FALSE(mock_notification_timer_->IsRunning());

  LogInAndFireTimer();

  message_center::Notification* notification = GetCellularSetupNotification();
  EXPECT_TRUE(notification);
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, LogInAgainAfterShowingNotification) {
  network_config_helper_->network_state_helper().AddDevice(
      kShillManagerClientStubCellularDevice, shill::kTypeCellular,
      kShillManagerClientStubCellularDeviceName);

  LogInAndFireTimer();

  message_center::Notification* notification = GetCellularSetupNotification();
  EXPECT_TRUE(notification);
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());

  message_center::MessageCenter::Get()->RemoveNotification(
      CellularSetupNotifier::kCellularSetupNotificationId, false);
  LogOut();
  LogIn();

  ASSERT_FALSE(mock_notification_timer_->IsRunning());
}

TEST_F(CellularSetupNotifierTest, LogInAgainAfterCheckingNonCellularDevice) {
  LogInAndFireTimer();

  message_center::Notification* notification = GetCellularSetupNotification();
  EXPECT_FALSE(notification);
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());

  LogOut();
  LogIn();

  ASSERT_FALSE(mock_notification_timer_->IsRunning());
}

TEST_F(CellularSetupNotifierTest, RemoveNotificationAfterAddingNetwork) {
  network_config_helper_->network_state_helper().AddDevice(
      kShillManagerClientStubCellularDevice, shill::kTypeCellular,
      kShillManagerClientStubCellularDeviceName);

  LogInAndFireTimer();

  message_center::Notification* notification = GetCellularSetupNotification();
  EXPECT_TRUE(notification);
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());

  const std::string& cellular_path_ =
      network_config_helper_->network_state_helper().ConfigureService(
          R"({"GUID": "cellular_guid", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})");

  base::RunLoop().RunUntilIdle();

  // Notification is not removed after adding unactivated network.
  notification = GetCellularSetupNotification();
  EXPECT_TRUE(notification);

  network_config_helper_->network_state_helper().SetServiceProperty(
      cellular_path_, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));

  base::RunLoop().RunUntilIdle();

  notification = GetCellularSetupNotification();
  EXPECT_FALSE(notification);
  ASSERT_FALSE(mock_notification_timer_->IsRunning());
}

}  // namespace ash
