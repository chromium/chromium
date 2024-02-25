// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/auto_connect_notifier.h"

#include <memory>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/system_notification_controller.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/auto_connect_handler.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kTestServicePath[] = "testServicePath";
constexpr char kTestServiceGuid[] = "testServiceGuid";
constexpr char kTestServiceName[] = "testServiceName";

}  // namespace

class AutoConnectNotifierTest : public AshTestBase {
 public:
  AutoConnectNotifierTest(const AutoConnectNotifierTest&) = delete;
  AutoConnectNotifierTest& operator=(const AutoConnectNotifierTest&) = delete;

 protected:
  AutoConnectNotifierTest() = default;
  ~AutoConnectNotifierTest() override = default;

  void SetUp() override {
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    NetworkCertLoader::ForceAvailableForNetworkAuthForTesting();
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    CHECK(NetworkHandler::Get()->auto_connect_handler());
    network_config_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();

    AshTestBase::SetUp();

    toast_manager_ = Shell::Get()->toast_manager();

    mock_notification_timer_ = new base::MockOneShotTimer();
    Shell::Get()
        ->system_notification_controller()
        ->auto_connect_->set_timer_for_testing(
            base::WrapUnique(mock_notification_timer_.get()));

    ShillServiceClient::Get()->GetTestInterface()->AddService(
        kTestServicePath, kTestServiceGuid, kTestServiceName, shill::kTypeWifi,
        shill::kStateIdle, true /* visible*/);
    // Ensure fake DBus service initialization completes.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    network_config_helper_.reset();
    network_handler_test_helper_.reset();
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
  }

  void NotifyConnectToNetworkRequested() {
    Shell::Get()
        ->system_notification_controller()
        ->auto_connect_->ConnectToNetworkRequested(kTestServicePath);
    base::RunLoop().RunUntilIdle();
  }

  void SuccessfullyJoinWifiNetwork() {
    ShillServiceClient::Get()->Connect(dbus::ObjectPath(kTestServicePath),
                                       base::BindOnce([]() {}),
                                       ShillServiceClient::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  ToastOverlay* GetCurrentOverlay() {
    return toast_manager_->GetCurrentOverlayForTesting();
  }

  void VerifyAutoConnectToastVisibility(bool visible) {
    if (visible) {
      ToastOverlay* overlay = GetCurrentOverlay();
      ASSERT_NE(nullptr, overlay);
      EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_NETWORK_AUTOCONNECT),
                overlay->GetText());
    } else {
      EXPECT_EQ(nullptr, GetCurrentOverlay());
    }
  }

  // Ownership passed to Shell owned AutoConnectNotifier instance.
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_notification_timer_;

 private:
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
  raw_ptr<ToastManagerImpl, DanglingUntriaged> toast_manager_ = nullptr;
};

TEST_F(AutoConnectNotifierTest, NoExplicitConnectionRequested) {
  NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);
  SuccessfullyJoinWifiNetwork();

  // Toast should not be displayed.
  VerifyAutoConnectToastVisibility(/*visible=*/false);
}

TEST_F(AutoConnectNotifierTest, AutoConnectDueToLoginOnly) {
  NotifyConnectToNetworkRequested();
  NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN);
  SuccessfullyJoinWifiNetwork();

  // Toast should not be displayed.
  VerifyAutoConnectToastVisibility(/*visible=*/false);
}

TEST_F(AutoConnectNotifierTest, NoConnectionBeforeTimerExpires) {
  NotifyConnectToNetworkRequested();
  NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);

  // No connection occurs.
  ASSERT_TRUE(mock_notification_timer_->IsRunning());
  mock_notification_timer_->Fire();

  // Connect after the timer fires; since the connection did not occur before
  // the timeout, no notification should be displayed.
  SuccessfullyJoinWifiNetwork();

  // Toast should not be displayed.
  VerifyAutoConnectToastVisibility(/*visible=*/false);
}

TEST_F(AutoConnectNotifierTest, ConnectToConnectedNetwork) {
  SuccessfullyJoinWifiNetwork();

  NotifyConnectToNetworkRequested();
  NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);
  SuccessfullyJoinWifiNetwork();

  // Toast should not be displayed.
  VerifyAutoConnectToastVisibility(/*visible=*/false);
}

TEST_F(AutoConnectNotifierTest, ToastDisplayed) {
  NotifyConnectToNetworkRequested();
  NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);
  SuccessfullyJoinWifiNetwork();

  VerifyAutoConnectToastVisibility(/*visible=*/true);
}

}  // namespace ash
