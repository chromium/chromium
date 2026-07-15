// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/bluetooth_log_controller.h"

#include "base/test/task_environment.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class BluetoothLogControllerTest : public testing::Test {
 public:
  BluetoothLogControllerTest() = default;
  ~BluetoothLogControllerTest() override = default;

  void SetUp() override {
    UpstartClient::InitializeFake();
    user_session_manager_ = std::make_unique<ash::test::TestUserSessionManager>(
        TestingBrowserProcess::GetGlobal()->local_state());
    controller_ = std::make_unique<BluetoothLogController>(
        user_manager::UserManager::Get());
  }

  void TearDown() override {
    controller_.reset();
    user_session_manager_.reset();
    UpstartClient::Shutdown();
  }

  ash::test::TestUserSessionManager& user_session_manager() {
    return *user_session_manager_;
  }
  BluetoothLogController& controller() { return *controller_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ash::test::TestUserSessionManager> user_session_manager_;
  std::unique_ptr<BluetoothLogController> controller_;
};

TEST_F(BluetoothLogControllerTest, GoogleInternalUser) {
  auto* upstart_client = FakeUpstartClient::Get();
  upstart_client->StartRecordingUpstartOperations();

  auto* user = user_session_manager().AddRegularUser(
      AccountId::FromUserEmailGaiaId("test@google.com", GaiaId("fakegaia")));
  ASSERT_TRUE(user);
  user_session_manager().LogIn(user->GetAccountId());

  auto upstart_operations =
      upstart_client->GetRecordedUpstartOperationsForJob("bluetoothlog");
  ASSERT_EQ(1u, upstart_operations.size());
  EXPECT_EQ(FakeUpstartClient::UpstartOperationType::START,
            upstart_operations[0].type);
}

TEST_F(BluetoothLogControllerTest, NonGoogleInternalUser) {
  auto* upstart_client = FakeUpstartClient::Get();
  upstart_client->StartRecordingUpstartOperations();

  auto* user = user_session_manager().AddRegularUser(
      AccountId::FromUserEmailGaiaId("test@test.org", GaiaId("fakegaia")));
  ASSERT_TRUE(user);
  user_session_manager().LogIn(user->GetAccountId());

  auto upstart_operations =
      upstart_client->GetRecordedUpstartOperationsForJob("bluetoothlog");
  EXPECT_EQ(0u, upstart_operations.size());
}

TEST_F(BluetoothLogControllerTest, NonRegularUser) {
  auto* upstart_client = FakeUpstartClient::Get();
  upstart_client->StartRecordingUpstartOperations();

  auto* user = user_session_manager().AddKioskChromeAppUser(
      "test@kiosk-apps.device-local.localhost");
  ASSERT_TRUE(user);
  user_session_manager().LogIn(user->GetAccountId());

  auto upstart_operations =
      upstart_client->GetRecordedUpstartOperationsForJob("bluetoothlog");
  EXPECT_EQ(0u, upstart_operations.size());
}

}  // namespace ash
