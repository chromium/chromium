// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_notification_controller.h"

#include <map>
#include <memory>
#include <string>

#include "ash/test/ash_test_base.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/message_center/fake_message_center.h"

using message_center::Notification;
using power_manager::PowerSupplyProperties;

namespace {

class MockMessageCenter : public message_center::FakeMessageCenter {
 public:
  MockMessageCenter() : add_count_(0), remove_count_(0), update_count_(0) {}

  MockMessageCenter(const MockMessageCenter&) = delete;
  MockMessageCenter& operator=(const MockMessageCenter&) = delete;

  ~MockMessageCenter() override = default;

  int add_count() const { return add_count_; }
  int remove_count() const { return remove_count_; }
  int update_count() const { return update_count_; }

  // message_center::FakeMessageCenter overrides:
  void AddNotification(std::unique_ptr<Notification> notification) override {
    add_count_++;
    notifications_.insert(
        std::make_pair(notification->id(), std::move(notification)));
  }
  void RemoveNotification(const std::string& id, bool by_user) override {
    Notification* notification = FindVisibleNotificationById(id);
    if (notification && notification->delegate())
      notification->delegate()->Close(by_user);
    remove_count_++;
    notifications_.erase(id);
  }
  void UpdateNotification(
      const std::string& id,
      std::unique_ptr<Notification> new_notification) override {
    update_count_++;
    Notification* notification = FindVisibleNotificationById(id);
    if (notification)
      notifications_.erase(id);
    notifications_.insert(
        std::make_pair(new_notification->id(), std::move(new_notification)));
  }

  Notification* FindVisibleNotificationById(const std::string& id) override {
    auto it = notifications_.find(id);
    return it == notifications_.end() ? NULL : it->second.get();
  }

 private:
  int add_count_;
  int remove_count_;
  int update_count_;
  std::map<std::string, std::unique_ptr<Notification>> notifications_;
};

}  // namespace

namespace ash {

class PowerNotificationControllerTest : public AshTestBase {
 public:
  PowerNotificationControllerTest() = default;

  PowerNotificationControllerTest(const PowerNotificationControllerTest&) =
      delete;
  PowerNotificationControllerTest& operator=(
      const PowerNotificationControllerTest&) = delete;

  ~PowerNotificationControllerTest() override = default;

  MockMessageCenter* message_center() { return message_center_.get(); }
  PowerNotificationController* controller() { return controller_.get(); }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    message_center_ = std::make_unique<MockMessageCenter>();
    controller_ =
        std::make_unique<PowerNotificationController>(message_center_.get());
  }

  void TearDown() override {
    controller_.reset();
    message_center_.reset();
    AshTestBase::TearDown();
  }

  PowerNotificationController::NotificationState notification_state() const {
    return controller_->notification_state_;
  }

  bool MaybeShowUsbChargerNotification(const PowerSupplyProperties& proto) {
    PowerStatus::Get()->SetProtoForTesting(proto);
    return controller_->MaybeShowUsbChargerNotification();
  }

  void MaybeShowDualRoleNotification(const PowerSupplyProperties& proto) {
    PowerStatus::Get()->SetProtoForTesting(proto);
    controller_->MaybeShowDualRoleNotification();
  }

  void UpdateNotificationState(
      const PowerSupplyProperties& proto,
      PowerNotificationController::NotificationState expected_state,
      bool expected_add,
      bool expected_remove) {
    int prev_add = message_center_->add_count();
    int prev_remove = message_center_->remove_count();
    PowerStatus::Get()->SetProtoForTesting(proto);
    controller_->OnPowerStatusChanged();
    EXPECT_EQ(expected_state, notification_state());
    EXPECT_EQ(expected_add, message_center_->add_count() == prev_add + 1);
    EXPECT_EQ(expected_remove,
              message_center_->remove_count() == prev_remove + 1);
  }

  void SetUsbChargerWasConnected(bool connected) {
    controller_->usb_charger_was_connected_ = connected;
  }

  void SetBatteryWasFull(bool full) { controller_->battery_was_full_ = full; }

  // Returns a discharging PowerSupplyProperties more appropriate for testing.
  static PowerSupplyProperties DefaultPowerSupplyProperties() {
    PowerSupplyProperties proto;
    proto.set_external_power(
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
    proto.set_battery_state(
        power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
    proto.set_battery_percent(50.0);
    proto.set_battery_time_to_empty_sec(3 * 60 * 60);
    proto.set_battery_time_to_full_sec(2 * 60 * 60);
    proto.set_is_calculating_battery_time(false);
    return proto;
  }

 private:
  std::unique_ptr<MockMessageCenter> message_center_;
  std::unique_ptr<PowerNotificationController> controller_;
};

TEST_F(PowerNotificationControllerTest, MaybeShowUsbChargerNotification) {
  PowerSupplyProperties discharging = DefaultPowerSupplyProperties();
  EXPECT_FALSE(MaybeShowUsbChargerNotification(discharging));
  EXPECT_EQ(0, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Notification shows when connecting a USB charger.
  PowerSupplyProperties usb_connected = DefaultPowerSupplyProperties();
  usb_connected.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  EXPECT_TRUE(MaybeShowUsbChargerNotification(usb_connected));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());
  SetUsbChargerWasConnected(true);

  // Change in charge does not trigger the notification again.
  PowerSupplyProperties more_charge = DefaultPowerSupplyProperties();
  more_charge.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  more_charge.set_battery_time_to_full_sec(60 * 60);
  more_charge.set_battery_percent(75.0);
  EXPECT_FALSE(MaybeShowUsbChargerNotification(more_charge));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Disconnecting a USB charger with the notification showing should close
  // the notification.
  EXPECT_TRUE(MaybeShowUsbChargerNotification(discharging));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(1, message_center()->remove_count());
  SetUsbChargerWasConnected(false);

  // Notification shows when connecting a USB charger again.
  EXPECT_TRUE(MaybeShowUsbChargerNotification(usb_connected));
  EXPECT_EQ(2, message_center()->add_count());
  EXPECT_EQ(1, message_center()->remove_count());
  SetUsbChargerWasConnected(true);

  // Notification hides when external power switches to AC.
  PowerSupplyProperties ac_charger = DefaultPowerSupplyProperties();
  ac_charger.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  EXPECT_TRUE(MaybeShowUsbChargerNotification(ac_charger));
  EXPECT_EQ(2, message_center()->add_count());
  EXPECT_EQ(2, message_center()->remove_count());
  SetUsbChargerWasConnected(false);

  // Notification shows when external power switches back to USB.
  EXPECT_TRUE(MaybeShowUsbChargerNotification(usb_connected));
  EXPECT_EQ(3, message_center()->add_count());
  EXPECT_EQ(2, message_center()->remove_count());
  SetUsbChargerWasConnected(true);

  // Notification does not re-appear after being manually dismissed if
  // power supply flickers between AC and USB charger.
  message_center()->RemoveNotification(
      PowerNotificationController::kUsbNotificationId, true);
  EXPECT_EQ(3, message_center()->remove_count());
  EXPECT_TRUE(MaybeShowUsbChargerNotification(ac_charger));
  SetUsbChargerWasConnected(false);
  EXPECT_FALSE(MaybeShowUsbChargerNotification(usb_connected));
  EXPECT_EQ(3, message_center()->add_count());
  SetUsbChargerWasConnected(true);

  // Notification appears again after being manually dismissed if the charger
  // is removed, and then a USB charger is attached.
  MaybeShowUsbChargerNotification(discharging);
  EXPECT_EQ(3, message_center()->add_count());
  SetUsbChargerWasConnected(false);
  MaybeShowUsbChargerNotification(usb_connected);
  EXPECT_EQ(4, message_center()->add_count());
  SetUsbChargerWasConnected(true);
}

TEST_F(PowerNotificationControllerTest,
       AvoidUsbChargerNotificationWhenBatteryFull) {
  PowerSupplyProperties full_proto;
  full_proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  full_proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  full_proto.set_battery_percent(100.0);
  full_proto.set_is_calculating_battery_time(false);

  PowerSupplyProperties not_full_proto;
  not_full_proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  not_full_proto.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  full_proto.set_battery_percent(90.0);
  full_proto.set_is_calculating_battery_time(false);

  // When the battery is reported as full, a notification shouldn't be displayed
  // for a low-power charger: http://b/64913617
  SetUsbChargerWasConnected(false);
  SetBatteryWasFull(false);
  EXPECT_FALSE(MaybeShowUsbChargerNotification(full_proto));
  EXPECT_EQ(0, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // The notification should be displayed if the battery isn't full, though.
  SetUsbChargerWasConnected(false);
  SetBatteryWasFull(false);
  EXPECT_TRUE(MaybeShowUsbChargerNotification(not_full_proto));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // It should be dismissed if the battery becomes full again while the charger
  // is still connected.
  SetUsbChargerWasConnected(true);
  SetBatteryWasFull(false);
  EXPECT_TRUE(MaybeShowUsbChargerNotification(full_proto));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(1, message_center()->remove_count());
}

TEST_F(PowerNotificationControllerTest,
       MaybeShowUsbChargerNotification_NoBattery) {
  // Notification does not show when powered by AC (including high-power
  // USB PD.
  PowerSupplyProperties ac_connected = DefaultPowerSupplyProperties();
  ac_connected.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  ac_connected.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT);
  ac_connected.set_preferred_minimum_external_power(60.0);
  EXPECT_FALSE(MaybeShowUsbChargerNotification(ac_connected));
  EXPECT_EQ(0, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Notification shows when powered by low-power USB.
  PowerSupplyProperties usb_connected = DefaultPowerSupplyProperties();
  usb_connected.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  usb_connected.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT);
  usb_connected.set_preferred_minimum_external_power(60.0);
  EXPECT_TRUE(MaybeShowUsbChargerNotification(usb_connected));
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(0, message_center()->remove_count());
  auto* notification =
      message_center()->FindVisibleNotificationById("usb-charger");
  ASSERT_TRUE(notification);
  EXPECT_TRUE(notification->never_timeout());
  EXPECT_FALSE(notification->pinned());
  EXPECT_NE(std::string::npos, notification->message().find(u"60W"))
      << notification->message();
}

TEST_F(PowerNotificationControllerTest, MaybeShowDualRoleNotification) {
  PowerSupplyProperties discharging = DefaultPowerSupplyProperties();
  discharging.set_supports_dual_role_devices(true);
  MaybeShowDualRoleNotification(discharging);
  EXPECT_EQ(0, message_center()->add_count());
  EXPECT_EQ(0, message_center()->update_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Notification shows when connecting a dual-role device.
  PowerSupplyProperties dual_role = DefaultPowerSupplyProperties();
  dual_role.set_supports_dual_role_devices(true);
  power_manager::PowerSupplyProperties_PowerSource* source =
      dual_role.add_available_external_power_source();
  source->set_id("dual-role1");
  source->set_active_by_default(false);
  MaybeShowDualRoleNotification(dual_role);
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(0, message_center()->update_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Connecting another dual-role device updates the notification to be plural.
  source = dual_role.add_available_external_power_source();
  source->set_id("dual-role2");
  source->set_active_by_default(false);
  MaybeShowDualRoleNotification(dual_role);
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(1, message_center()->update_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Connecting a 3rd dual-role device doesn't affect the notification.
  source = dual_role.add_available_external_power_source();
  source->set_id("dual-role3");
  source->set_active_by_default(false);
  MaybeShowDualRoleNotification(dual_role);
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(1, message_center()->update_count());
  EXPECT_EQ(0, message_center()->remove_count());

  // Connecting a legacy USB device removes the notification.
  PowerSupplyProperties legacy(dual_role);
  power_manager::PowerSupplyProperties_PowerSource* legacy_source =
      legacy.add_available_external_power_source();
  legacy_source->set_id("legacy");
  legacy_source->set_active_by_default(true);
  legacy.set_external_power_source_id("legacy");
  legacy.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  MaybeShowDualRoleNotification(legacy);
  EXPECT_EQ(1, message_center()->add_count());
  EXPECT_EQ(1, message_center()->update_count());
  EXPECT_EQ(1, message_center()->remove_count());

  // Removing the legacy USB device adds the notification again.
  MaybeShowDualRoleNotification(dual_role);
  EXPECT_EQ(2, message_center()->add_count());
  EXPECT_EQ(1, message_center()->update_count());
  EXPECT_EQ(1, message_center()->remove_count());

  // Charging from the device updates the notification.
  dual_role.set_external_power_source_id("dual-role1");
  dual_role.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  MaybeShowDualRoleNotification(dual_role);
  EXPECT_EQ(2, message_center()->add_count());
  EXPECT_EQ(2, message_center()->update_count());
  EXPECT_EQ(1, message_center()->remove_count());

  // Adding a device as a sink doesn't change the notification, because the
  // notification exposes the source.
  source = dual_role.add_available_external_power_source();
  source->set_active_by_default(false);
  MaybeShowDualRoleNotification(dual_role);
  EXPECT_EQ(2, message_center()->add_count());
  EXPECT_EQ(2, message_center()->update_count());
  EXPECT_EQ(1, message_center()->remove_count());

  // Changing the source to a sink changes the notification.
  dual_role.set_external_power_source_id("");
  dual_role.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  MaybeShowDualRoleNotification(dual_role);
  EXPECT_EQ(2, message_center()->add_count());
  EXPECT_EQ(3, message_center()->update_count());
  EXPECT_EQ(1, message_center()->remove_count());

  // An unrelated change has no effect.
  dual_role.set_battery_time_to_empty_sec(2 * 60 * 60);
  MaybeShowDualRoleNotification(dual_role);
  EXPECT_EQ(2, message_center()->add_count());
  EXPECT_EQ(3, message_center()->update_count());
  EXPECT_EQ(1, message_center()->remove_count());

  // Removing devices hides the notification.
  MaybeShowDualRoleNotification(discharging);
  EXPECT_EQ(2, message_center()->add_count());
  EXPECT_EQ(3, message_center()->update_count());
  EXPECT_EQ(2, message_center()->remove_count());
}

TEST_F(PowerNotificationControllerTest, UpdateNotificationState) {
  // No notifications when no battery present.
  PowerSupplyProperties no_battery = DefaultPowerSupplyProperties();
  no_battery.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  no_battery.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT);
  {
    SCOPED_TRACE("No notifications when no battery present");
    UpdateNotificationState(no_battery,
                            PowerNotificationController::NOTIFICATION_NONE,
                            false, false);
  }

  // No notification when calculating remaining battery time.
  PowerSupplyProperties calculating = DefaultPowerSupplyProperties();
  calculating.set_is_calculating_battery_time(true);
  {
    SCOPED_TRACE("No notification when calculating remaining battery time");
    UpdateNotificationState(calculating,
                            PowerNotificationController::NOTIFICATION_NONE,
                            false, false);
  }

  // No notification when charging.
  PowerSupplyProperties charging = DefaultPowerSupplyProperties();
  charging.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  charging.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  {
    SCOPED_TRACE("No notification when charging");
    UpdateNotificationState(
        charging, PowerNotificationController::NOTIFICATION_NONE, false, false);
  }

  // When the rounded minutes-to-empty are above the threshold, no notification
  // should be shown.
  PowerSupplyProperties low = DefaultPowerSupplyProperties();
  low.set_battery_time_to_empty_sec(
      PowerNotificationController::kLowPowerMinutes * 60 + 30);
  {
    SCOPED_TRACE("No notification when time to empty above threshold");
    UpdateNotificationState(low, PowerNotificationController::NOTIFICATION_NONE,
                            false, false);
  }

  // When the rounded value matches the threshold, the notification should
  // appear.
  low.set_battery_time_to_empty_sec(
      PowerNotificationController::kLowPowerMinutes * 60 + 29);
  {
    SCOPED_TRACE("Notification when time to empty matches threshold");
    UpdateNotificationState(
        low, PowerNotificationController::NOTIFICATION_LOW_POWER, true, false);
  }

  // It should persist at lower values.
  low.set_battery_time_to_empty_sec(
      PowerNotificationController::kLowPowerMinutes * 60 - 20);
  {
    SCOPED_TRACE("Notification persists at lower values");
    UpdateNotificationState(
        low, PowerNotificationController::NOTIFICATION_LOW_POWER, false, false);
  }

  // The critical low battery notification should be shown when the rounded
  // value is at the lower threshold.
  PowerSupplyProperties critical = DefaultPowerSupplyProperties();
  critical.set_battery_time_to_empty_sec(
      PowerNotificationController::kCriticalMinutes * 60 + 29);
  {
    SCOPED_TRACE("Critical notification when time to empty is critical");
    UpdateNotificationState(critical,
                            PowerNotificationController::NOTIFICATION_CRITICAL,
                            true, true);
  }

  // The notification should be dismissed when the no-warning threshold is
  // reached.
  PowerSupplyProperties safe = DefaultPowerSupplyProperties();
  safe.set_battery_time_to_empty_sec(
      PowerNotificationController::kNoWarningMinutes * 60 - 29);
  {
    SCOPED_TRACE("Notification removed when battery not low");
    UpdateNotificationState(
        safe, PowerNotificationController::NOTIFICATION_NONE, false, true);
  }

  // Test that rounded percentages are used when a USB charger is connected.
  PowerSupplyProperties low_usb = DefaultPowerSupplyProperties();
  low_usb.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  low_usb.set_battery_percent(PowerNotificationController::kLowPowerPercentage +
                              0.5);
  {
    SCOPED_TRACE("No notification for rounded battery percent");
    UpdateNotificationState(
        low_usb, PowerNotificationController::NOTIFICATION_NONE, true, false);
  }

  low_usb.set_battery_percent(PowerNotificationController::kLowPowerPercentage +
                              0.49);
  {
    SCOPED_TRACE("Notification for rounded low power percent");
    UpdateNotificationState(low_usb,
                            PowerNotificationController::NOTIFICATION_LOW_POWER,
                            true, false);
  }

  PowerSupplyProperties critical_usb = DefaultPowerSupplyProperties();
  critical_usb.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  critical_usb.set_battery_percent(
      PowerNotificationController::kCriticalPercentage + 0.2);
  {
    SCOPED_TRACE("Notification for rounded critical power percent");
    UpdateNotificationState(critical_usb,
                            PowerNotificationController::NOTIFICATION_CRITICAL,
                            true, true);
  }

  PowerSupplyProperties safe_usb = DefaultPowerSupplyProperties();
  safe_usb.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_USB);
  safe_usb.set_battery_percent(
      PowerNotificationController::kNoWarningPercentage - 0.1);
  {
    SCOPED_TRACE("Notification removed for rounded percent above threshold");
    UpdateNotificationState(
        safe_usb, PowerNotificationController::NOTIFICATION_NONE, false, true);
  }
}

// Test that a notification isn't shown if powerd sends a -1 time-to-empty value
// to indicate that it couldn't produce an estimate: https://crbug.com/930358
TEST_F(PowerNotificationControllerTest, IgnoreMissingBatteryEstimates) {
  PowerSupplyProperties proto = DefaultPowerSupplyProperties();
  proto.set_battery_time_to_empty_sec(-1);
  UpdateNotificationState(proto, PowerNotificationController::NOTIFICATION_NONE,
                          false, false);
}

}  // namespace ash
