// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/auto_connect_notifier.h"

#include <memory>

#include "ash/shell.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/timer/mock_timer.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/auto_connect_handler.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

constexpr char kTestServicePath[] = "testServicePath";
constexpr char kTestServiceGuid[] = "testServiceGuid";
constexpr char kTestServiceName[] = "testServiceName";

}  // namespace

class AutoConnectNotifierTest : public AshTestBase {
 protected:
  AutoConnectNotifierTest() = default;
  ~AutoConnectNotifierTest() override = default;

  void SetUp() override {
    chromeos::NetworkCertLoader::Initialize();
    chromeos::NetworkCertLoader::ForceHardwareBackedForTesting();
    chromeos::shill_clients::InitializeFakes();
    chromeos::NetworkHandler::Initialize();
    CHECK(chromeos::NetworkHandler::Get()->auto_connect_handler());
    network_config_helper_ = std::make_unique<
        chromeos::network_config::CrosNetworkConfigTestHelper>();

    AshTestBase::SetUp();

    mock_notification_timer_ = new base::MockOneShotTimer();
    Shell::Get()
        ->system_notification_controller()
        ->auto_connect_->set_timer_for_testing(
            base::WrapUnique(mock_notification_timer_));

    chromeos::ShillServiceClient::Get()->GetTestInterface()->AddService(
        kTestServicePath, kTestServiceGuid, kTestServiceName, shill::kTypeWifi,
        shill::kStateIdle, true /* visible*/);
    // Ensure fake DBus service initialization completes.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    network_config_helper_.reset();
    chromeos::NetworkHandler::Shutdown();
    chromeos::shill_clients::Shutdown();
    chromeos::NetworkCertLoader::Shutdown();
  }

  void NotifyConnectToNetworkRequested() {
    Shell::Get()
        ->system_notification_controller()
        ->auto_connect_->ConnectToNetworkRequested(kTestServicePath);
    base::RunLoop().RunUntilIdle();
  }

  void SuccessfullyJoinWifiNetwork() {
    chromeos::ShillServiceClient::Get()->Connect(
        dbus::ObjectPath(kTestServicePath), base::BindRepeating([]() {}),
        chromeos::ShillServiceClient::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  std::string GetNotificationId() {
    return AutoConnectNotifier::kAutoConnectNotificationId;
  }

  // Ownership passed to Shell owned AutoConnectNotifier instance.
  base::MockOneShotTimer* mock_notification_timer_;

 private:
  std::unique_ptr<chromeos::network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;

  DISALLOW_COPY_AND_ASSIGN(AutoConnectNotifierTest);
};

TEST_F(AutoConnectNotifierTest, NoExplicitConnectionRequested) {
  chromeos::NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          chromeos::AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);
  SuccessfullyJoinWifiNetwork();

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          GetNotificationId());
  EXPECT_FALSE(notification);
}

TEST_F(AutoConnectNotifierTest, AutoConnectDueToLoginOnly) {
  NotifyConnectToNetworkRequested();
  chromeos::NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          chromeos::AutoConnectHandler::AUTO_CONNECT_REASON_LOGGED_IN);
  SuccessfullyJoinWifiNetwork();

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          GetNotificationId());
  EXPECT_FALSE(notification);
}

TEST_F(AutoConnectNotifierTest, NoConnectionBeforeTimerExpires) {
  NotifyConnectToNetworkRequested();
  chromeos::NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          chromeos::AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);

  // No connection occurs.
  ASSERT_TRUE(mock_notification_timer_->IsRunning());
  mock_notification_timer_->Fire();

  // Connect after the timer fires; since the connection did not occur before
  // the timeout, no notification should be displayed.
  SuccessfullyJoinWifiNetwork();

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          GetNotificationId());
  EXPECT_FALSE(notification);
}

TEST_F(AutoConnectNotifierTest, ConnectToConnectedNetwork) {
  SuccessfullyJoinWifiNetwork();

  NotifyConnectToNetworkRequested();
  chromeos::NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          chromeos::AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);
  SuccessfullyJoinWifiNetwork();

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          GetNotificationId());
  ASSERT_FALSE(notification);
}

TEST_F(AutoConnectNotifierTest, NotificationDisplayed) {
  NotifyConnectToNetworkRequested();
  chromeos::NetworkHandler::Get()
      ->auto_connect_handler()
      ->NotifyAutoConnectInitiatedForTest(
          chromeos::AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED);
  SuccessfullyJoinWifiNetwork();

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          GetNotificationId());
  ASSERT_TRUE(notification);
  EXPECT_EQ(GetNotificationId(), notification->id());
}

}  // namespace ash
