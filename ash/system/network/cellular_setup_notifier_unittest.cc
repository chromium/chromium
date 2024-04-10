// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/cellular_setup_notifier.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

constexpr char kShillManagerClientStubCellularDevice[] =
    "/device/stub_cellular_device";
constexpr char kShillManagerClientStubCellularDeviceName[] =
    "stub_cellular_device";
constexpr char kCellularNetworkWithActivationState[] = R"(
{
  "GUID": "cellular_guid",
  "Type": "cellular",
  "Technology": "LTE",
  "State": "idle",
  "Cellular.ActivationState": "%s"
})";

// Delay after OOBE when the notification is expected to be shown.
constexpr base::TimeDelta kNotificationDelay = base::Minutes(15);

}  // namespace

class CellularSetupNotifierTest : public NoSessionAshTestBase {
 protected:
  CellularSetupNotifierTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
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
    network_config_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();

    AshTestBase::SetUp();
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

  void AddCellularDevice() {
    network_config_helper_->network_state_helper().AddDevice(
        kShillManagerClientStubCellularDevice, shill::kTypeCellular,
        kShillManagerClientStubCellularDeviceName);
    base::RunLoop().RunUntilIdle();
  }

  std::string ConfigureCellularService(bool activated) {
    const std::string path =
        network_config_helper_->network_state_helper().ConfigureService(
            base::StringPrintf(kCellularNetworkWithActivationState,
                               activated
                                   ? shill::kActivationStateActivated
                                   : shill::kActivationStateNotActivated));
    base::RunLoop().RunUntilIdle();
    return path;
  }

  void ActivateCellularService(const std::string& path) {
    network_config_helper_->network_state_helper().SetServiceProperty(
        path, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateActivated));
    base::RunLoop().RunUntilIdle();
  }

  // Returns whether the cellular setup notification is shown.
  bool IsNotificationShown() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        CellularSetupNotifier::kCellularSetupNotificationId);
  }

  void LogIn() {
    SimulateUserLogin("user1@test.com");
    base::RunLoop().RunUntilIdle();
  }

  void LogOut() {
    // Remove the notification if it is shown.
    message_center::MessageCenter::Get()->RemoveNotification(
        CellularSetupNotifier::kCellularSetupNotificationId, false);
    ClearLogin();
    base::RunLoop().RunUntilIdle();
  }

  void FastForwardNotificationDelay() {
    task_environment()->FastForwardBy(kNotificationDelay);
    base::RunLoop().RunUntilIdle();
  }

  // Returns the pref that indicates whether the notification is able to be
  // shown; the value will be |false| if the notification has already been
  // shown, or if we should otherwise not show the notification.
  bool GetCanCellularSetupNotificationBeShown() {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    if (!prefs) {
      return false;
    }
    return prefs->GetBoolean(prefs::kCanCellularSetupNotificationBeShown);
  }

  void SetCanCellularSetupNotificationBeShown(bool value) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    prefs->SetBoolean(prefs::kCanCellularSetupNotificationBeShown, value);
  }

 private:
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
};

TEST_F(CellularSetupNotifierTest, DontShowNotificationUnfinishedOOBE) {
  FastForwardNotificationDelay();
  EXPECT_FALSE(IsNotificationShown());
}

TEST_F(CellularSetupNotifierTest, ShowNotificationZeroUnactivatedNetworks) {
  AddCellularDevice();

  LogIn();
  FastForwardNotificationDelay();

  EXPECT_TRUE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, ShowNotificationOneUnactivatedNetwork) {
  AddCellularDevice();
  ConfigureCellularService(/*activated=*/false);

  LogIn();
  FastForwardNotificationDelay();

  EXPECT_TRUE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, ShowNotificationTwoUnactivatedNetworks) {
  AddCellularDevice();
  ConfigureCellularService(/*activated=*/false);
  ConfigureCellularService(/*activated=*/false);

  LogIn();
  FastForwardNotificationDelay();

  EXPECT_TRUE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, DontShowNotificationActivatedNetwork) {
  AddCellularDevice();
  const std::string& cellular_path =
      ConfigureCellularService(/*activated=*/true);

  LogIn();
  FastForwardNotificationDelay();

  EXPECT_FALSE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, LogOutBeforeNotificationShowsThenLogInAgain) {
  AddCellularDevice();

  LogIn();
  task_environment()->FastForwardBy(kNotificationDelay - base::Minutes(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetCanCellularSetupNotificationBeShown());

  LogOut();
  FastForwardNotificationDelay();
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());

  LogIn();
  FastForwardNotificationDelay();

  EXPECT_TRUE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest, LogInAgainAfterShowingNotification) {
  AddCellularDevice();

  LogIn();
  FastForwardNotificationDelay();

  EXPECT_TRUE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());

  LogOut();
  LogIn();

  // Check that even without a delay we correctly identify that a notification
  // was already shown.
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
  FastForwardNotificationDelay();
  EXPECT_FALSE(IsNotificationShown());
}

TEST_F(CellularSetupNotifierTest, LogInAgainAfterCheckingNonCellularDevice) {
  // Perform the logic twice to check that even after logging out and back in
  // the notification is not shown if there is no cellular device.
  for (int i = 0; i < 2; ++i) {
    LogIn();
    FastForwardNotificationDelay();

    EXPECT_FALSE(IsNotificationShown());
    EXPECT_TRUE(GetCanCellularSetupNotificationBeShown());

    LogOut();
  }
}

TEST_F(CellularSetupNotifierTest,
       NotificationStillShownAfterLatentCellularDevice) {
  LogIn();
  FastForwardNotificationDelay();

  EXPECT_FALSE(IsNotificationShown());
  EXPECT_TRUE(GetCanCellularSetupNotificationBeShown());

  AddCellularDevice();
  FastForwardNotificationDelay();

  EXPECT_TRUE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());
}

TEST_F(CellularSetupNotifierTest,
       RemoveNotificationAfterAddingActivatedNetwork) {
  AddCellularDevice();

  LogIn();
  FastForwardNotificationDelay();

  EXPECT_TRUE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());

  const std::string& cellular_path =
      ConfigureCellularService(/*activated=*/true);

  EXPECT_FALSE(IsNotificationShown());
}

TEST_F(CellularSetupNotifierTest,
       RemoveNotificationAfterExistingNetworkBecomesActivated) {
  AddCellularDevice();

  LogIn();
  FastForwardNotificationDelay();

  EXPECT_TRUE(IsNotificationShown());
  EXPECT_FALSE(GetCanCellularSetupNotificationBeShown());

  const std::string& cellular_path =
      ConfigureCellularService(/*activated=*/false);

  // Notification is not removed after adding unactivated network.
  EXPECT_TRUE(IsNotificationShown());

  ActivateCellularService(cellular_path);
  EXPECT_FALSE(IsNotificationShown());
}

}  // namespace ash
