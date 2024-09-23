// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/managed_sim_lock_notifier.h"

#include "ash/constants/ash_features.h"
#include "ash/system/system_notification_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

const char kTestCellularServicePath[] = "cellular_service_path";
const char kTestCellularDevicePath[] = "cellular_path";
const char kTestCellularDevicePathName[] = "stub_cellular_device";
const char kTestIccid[] = "1234567890123456789";
const char kTestCellularGuid[] = "cellular_guid";
const char kTestCellularName[] = "cellular_name";
const char kTestEid[] = "123456789012345678901234567890123";

}  // namespace

class ManagedSimLockNotifierTest : public NoSessionAshTestBase {
 protected:
  ManagedSimLockNotifierTest() = default;
  ManagedSimLockNotifierTest(const ManagedSimLockNotifierTest&) = delete;
  ManagedSimLockNotifierTest& operator=(const ManagedSimLockNotifierTest&) =
      delete;
  ~ManagedSimLockNotifierTest() override = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();

    network_config_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();
    AshTestBase::SetUp();
    base::RunLoop().RunUntilIdle();

    // User must be logged in for notification to be visible.
    LogIn();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    network_config_helper_.reset();
    network_handler_test_helper_.reset();
  }

  void LogIn() { SimulateUserLogin("user1@test.com"); }

  void LogOut() { ClearLogin(); }

  ManagedNetworkConfigurationHandler* managed_network_configuration_handler() {
    return NetworkHandler::Get()->managed_network_configuration_handler();
  }

  void SetCellularEnabled(bool enabled) {
    NetworkHandler::Get()
        ->technology_state_controller()
        ->SetTechnologiesEnabled(NetworkTypePattern::Cellular(), enabled,
                                 network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularSimLockEnabled(
      bool enable,
      const std::optional<std::string>& lock_type = std::nullopt) {
    // Simulate a locked SIM.
    base::Value::Dict sim_lock_status;
    sim_lock_status.Set(shill::kSIMLockEnabledProperty, enable);
    if (lock_type.has_value())
      sim_lock_status.Set(shill::kSIMLockTypeProperty, *lock_type);
    network_config_helper_->network_state_helper()
        .device_test()
        ->SetDeviceProperty(
            kTestCellularDevicePath, shill::kSIMLockStatusProperty,
            base::Value(std::move(sim_lock_status)), /*notify_changed=*/true);

    // Set the cellular service to be the active profile.
    base::Value::List sim_slot_infos;
    base::Value::Dict slot_info_item;
    slot_info_item.Set(shill::kSIMSlotInfoICCID, kTestIccid);
    slot_info_item.Set(shill::kSIMSlotInfoPrimary, true);
    sim_slot_infos.Append(std::move(slot_info_item));
    network_config_helper_->network_state_helper()
        .device_test()
        ->SetDeviceProperty(
            kTestCellularDevicePath, shill::kSIMSlotInfoProperty,
            base::Value(std::move(sim_slot_infos)), /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  void SetAllowCellularSimLock(bool allow_cellular_sim_lock) {
    base::Value::Dict global_config;
    global_config.Set(::onc::global_network_config::kAllowCellularSimLock,
                      allow_cellular_sim_lock);
    managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
        base::Value::List(), global_config);
    base::RunLoop().RunUntilIdle();
  }

  void AddCellularDevice() {
    network_config_helper_->network_state_helper().AddDevice(
        kTestCellularDevicePath, shill::kTypeCellular,
        kTestCellularDevicePathName);
  }

  void AddCellularService(
      const std::string& service_path = kTestCellularServicePath,
      const std::string& iccid = kTestIccid) {
    // Add idle, non-connectable network.
    network_config_helper_->network_state_helper().service_test()->AddService(
        service_path, kTestCellularGuid, kTestCellularName,
        shill::kTypeCellular, shill::kStateIdle, /*visible=*/true);

    network_config_helper_->network_state_helper()
        .service_test()
        ->SetServiceProperty(service_path, shill::kEidProperty,
                             base::Value(kTestEid));

    network_config_helper_->network_state_helper()
        .service_test()
        ->SetServiceProperty(service_path, shill::kIccidProperty,
                             base::Value(iccid));
    base::RunLoop().RunUntilIdle();
  }

  void ClickOnNotification() {
    message_center::MessageCenter::Get()->ClickOnNotification(
        ManagedSimLockNotifier::kManagedSimLockNotificationId);
  }

  void RemoveNotification(bool by_user) {
    message_center::MessageCenter::Get()->RemoveNotification(
        ManagedSimLockNotifier::kManagedSimLockNotificationId, by_user);
  }

  // Returns the managed SIM lock notification if it is shown, and null if it is
  // not shown.
  message_center::Notification* GetManagedSimLockNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        ManagedSimLockNotifier::kManagedSimLockNotificationId);
  }

  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ManagedSimLockNotifierTest, PolicyChanged) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  AddCellularDevice();
  AddCellularService();
  EXPECT_FALSE(GetManagedSimLockNotification());

  SetCellularSimLockEnabled(true);
  EXPECT_FALSE(GetManagedSimLockNotification());

  SetAllowCellularSimLock(false);
  EXPECT_TRUE(GetManagedSimLockNotification());

  SetAllowCellularSimLock(true);
  EXPECT_FALSE(GetManagedSimLockNotification());
}

TEST_F(ManagedSimLockNotifierTest, NewActiveSession) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  AddCellularDevice();
  AddCellularService();
  SetCellularSimLockEnabled(true);
  SetAllowCellularSimLock(false);

  // Notification should be shown; proceed to remove it.
  EXPECT_TRUE(GetManagedSimLockNotification());
  RemoveNotification(/*by_user=*/false);
  EXPECT_FALSE(GetManagedSimLockNotification());

  LogOut();
  LogIn();
  base::RunLoop().RunUntilIdle();

  // Notification should surface in new active session since the SIM is PIN
  // locked and policy is true.
  EXPECT_TRUE(GetManagedSimLockNotification());

  RemoveNotification(/*by_user=*/false);
  SetAllowCellularSimLock(true);

  LogOut();
  LogIn();
  base::RunLoop().RunUntilIdle();

  // Notification should not surface in new active session since the policy is
  // false.
  EXPECT_FALSE(GetManagedSimLockNotification());

  SetCellularSimLockEnabled(false);
  SetAllowCellularSimLock(false);

  LogOut();
  LogIn();
  base::RunLoop().RunUntilIdle();

  // Notification should not surface in new active session since the SIM is not
  // PIN locked.
  EXPECT_FALSE(GetManagedSimLockNotification());

  SetAllowCellularSimLock(true);

  LogOut();
  LogIn();
  base::RunLoop().RunUntilIdle();

  // Notification should not surface in new active session since the SIM is not
  // PIN locked.
  EXPECT_FALSE(GetManagedSimLockNotification());
}

TEST_F(ManagedSimLockNotifierTest, HideNotificationOnLockDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  AddCellularDevice();
  AddCellularService();
  SetCellularSimLockEnabled(true);
  SetAllowCellularSimLock(false);

  EXPECT_TRUE(GetManagedSimLockNotification());

  // Notification will disappear once user disables SIM Lock setting.
  SetCellularSimLockEnabled(false);
  EXPECT_FALSE(GetManagedSimLockNotification());
}

TEST_F(ManagedSimLockNotifierTest, PrimarySimIccidChanged) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  AddCellularDevice();
  AddCellularService();
  SetCellularSimLockEnabled(true);
  SetAllowCellularSimLock(false);

  EXPECT_TRUE(GetManagedSimLockNotification());
  RemoveNotification(/*by_user=*/false);

  EXPECT_FALSE(GetManagedSimLockNotification());
  // Simulate primary ICCID changed. Notification should be shown after.
  base::Value::List sim_slot_infos;
  base::Value::Dict slot_info_item;
  slot_info_item.Set(shill::kSIMSlotInfoICCID, kTestIccid);
  slot_info_item.Set(shill::kSIMSlotInfoPrimary, false);
  sim_slot_infos.Append(std::move(slot_info_item));

  base::Value::Dict slot_info_item_2;
  slot_info_item_2.Set(shill::kSIMSlotInfoICCID, "kTestIccid2");
  slot_info_item_2.Set(shill::kSIMSlotInfoPrimary, true);
  sim_slot_infos.Append(std::move(slot_info_item_2));

  network_config_helper_->network_state_helper()
      .device_test()
      ->SetDeviceProperty(kTestCellularDevicePath, shill::kSIMSlotInfoProperty,
                          base::Value(std::move(sim_slot_infos)),
                          /*notify_changed=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetManagedSimLockNotification());
}

TEST_F(ManagedSimLockNotifierTest, NotificationOnCellularOnOrOff) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  base::HistogramTester histograms;

  AddCellularDevice();
  AddCellularService();
  SetCellularSimLockEnabled(true);
  SetAllowCellularSimLock(false);

  EXPECT_TRUE(GetManagedSimLockNotification());
  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationEventHistogram,
      CellularMetricsLogger::SimLockNotificationEvent::kShown, 1);

  // Notification will disappear if user turns off Cellular.
  SetCellularEnabled(false);
  EXPECT_FALSE(GetManagedSimLockNotification());

  // Notification will appear if user turns on Cellular.
  SetCellularEnabled(true);
  EXPECT_TRUE(GetManagedSimLockNotification());
}

TEST_F(ManagedSimLockNotifierTest, NotificationClicked) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  base::HistogramTester histograms;

  AddCellularDevice();
  AddCellularService();
  SetCellularSimLockEnabled(true);
  SetAllowCellularSimLock(false);

  ClickOnNotification();

  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationEventHistogram,
      CellularMetricsLogger::SimLockNotificationEvent::kShown, 1);

  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationEventHistogram,
      CellularMetricsLogger::SimLockNotificationEvent::kClicked, 1);

  // Notification will be dismissed by the system, in which case we shouldn't
  // be emitting the dismissed by user metric.
  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationEventHistogram,
      CellularMetricsLogger::SimLockNotificationEvent::kDismissed, 0);
}

TEST_F(ManagedSimLockNotifierTest, NotificationDismissedByUser) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  base::HistogramTester histograms;

  AddCellularDevice();
  AddCellularService();
  SetCellularSimLockEnabled(true);
  SetAllowCellularSimLock(false);

  RemoveNotification(/*by_user=*/true);

  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationEventHistogram,
      CellularMetricsLogger::SimLockNotificationEvent::kShown, 1);

  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationEventHistogram,
      CellularMetricsLogger::SimLockNotificationEvent::kClicked, 0);

  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationEventHistogram,
      CellularMetricsLogger::SimLockNotificationEvent::kDismissed, 1);
}

TEST_F(ManagedSimLockNotifierTest, SIMLockTypeMetrics) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  base::HistogramTester histograms;

  AddCellularDevice();
  AddCellularService();
  SetCellularSimLockEnabled(true, shill::kSIMLockPin);
  SetAllowCellularSimLock(false);

  EXPECT_TRUE(GetManagedSimLockNotification());
  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationLockType,
      CellularMetricsLogger::SimPinLockType::kPinLocked, 1);
  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationLockType,
      CellularMetricsLogger::SimPinLockType::kPukLocked, 0);

  SetCellularSimLockEnabled(false);
  SetAllowCellularSimLock(true);
  EXPECT_FALSE(GetManagedSimLockNotification());

  SetCellularSimLockEnabled(true, shill::kSIMLockPuk);
  SetAllowCellularSimLock(false);

  EXPECT_TRUE(GetManagedSimLockNotification());
  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationLockType,
      CellularMetricsLogger::SimPinLockType::kPinLocked, 1);
  histograms.ExpectBucketCount(
      CellularMetricsLogger::kSimLockNotificationLockType,
      CellularMetricsLogger::SimPinLockType::kPukLocked, 1);
}

}  // namespace ash
