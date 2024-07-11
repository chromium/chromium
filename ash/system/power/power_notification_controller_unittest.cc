// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_notification_controller.h"

#include <map>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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
    if (notification && notification->delegate()) {
      notification->delegate()->Close(by_user);
    }
    remove_count_++;
    notifications_.erase(id);
  }
  void UpdateNotification(
      const std::string& id,
      std::unique_ptr<Notification> new_notification) override {
    update_count_++;
    Notification* notification = FindVisibleNotificationById(id);
    if (notification) {
      notifications_.erase(id);
    }
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
  PowerNotificationControllerTest(
      const std::vector<base::test::FeatureRefAndParams>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features =
          {features::kBatterySaver})
      : enabled_features_(enabled_features),
        disabled_features_(disabled_features) {}

  PowerNotificationControllerTest(const PowerNotificationControllerTest&) =
      delete;
  PowerNotificationControllerTest& operator=(
      const PowerNotificationControllerTest&) = delete;

  ~PowerNotificationControllerTest() override = default;

  MockMessageCenter* message_center() { return message_center_.get(); }
  PowerNotificationController* controller() { return controller_.get(); }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeaturesAndParameters(enabled_features_,
                                                        disabled_features_);

    AshTestBase::SetUp();
    message_center_ = std::make_unique<MockMessageCenter>();
    controller_ =
        std::make_unique<PowerNotificationController>(message_center_.get());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    controller_.reset();
    message_center_.reset();
    histogram_tester_.reset();
    AshTestBase::TearDown();
    scoped_feature_list_.reset();
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

  void SimulateSuspend() {
    controller_->SuspendImminent(power_manager::SuspendImminent::LID_CLOSED);
  }

  void SimulateRestart() {
    controller_->RestartRequested(
        power_manager::RequestRestartReason::REQUEST_RESTART_FOR_USER);
    controller_->OnShellDestroying();
    controller_.reset();
  }

  void SimulateShutdownByUser() {
    controller_->ShutdownRequested(
        power_manager::RequestShutdownReason::REQUEST_SHUTDOWN_FOR_USER);
    controller_->OnShellDestroying();
    controller_.reset();
  }

  void SimulateShutdownByPowerd() {
    controller_->OnShellDestroying();
    controller_.reset();
  }

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

  int GetLowPowerPercentageExperiment() const {
    return controller_->low_power_percentage_;
  }
  int GetCriticalPowerPercentageExperiment() const {
    return controller_->critical_percentage_;
  }
  int GetNoWarningPercentageExperiment() const {
    return controller_->no_warning_percentage_;
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  std::unique_ptr<MockMessageCenter> message_center_;
  std::unique_ptr<PowerNotificationController> controller_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  const std::vector<base::test::FeatureRefAndParams> enabled_features_;
  const std::vector<base::test::FeatureRef> disabled_features_;
};

class PowerNotificationControllerWithBatterySaverTest
    : public PowerNotificationControllerTest,
      public testing::WithParamInterface<
          features::BatterySaverNotificationBehavior> {
 public:
  PowerNotificationControllerWithBatterySaverTest()
      : PowerNotificationControllerTest(
            {{features::kBatterySaver,
              {{features::kBatterySaverNotificationBehavior.name,
                features::kBatterySaverNotificationBehavior.options[GetParam()]
                    .name}}}},
            {}) {}

  PowerNotificationController::NotificationState GetLowPowerNotificationState()
      const {
    const auto arm = features::kBatterySaverNotificationBehavior.Get();
    CHECK(arm == GetParam());
    switch (arm) {
      case features::kBSMAutoEnable:
        return PowerNotificationController::
            NOTIFICATION_BSM_ENABLING_AT_THRESHOLD;

      case features::kBSMOptIn:
        return PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN;
    }
    NOTREACHED();
  }
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
  EXPECT_TRUE(base::Contains((notification->message()), u"60W"))
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
        low, PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN,
        true, false);
  }

  // It should persist at lower values.
  low.set_battery_time_to_empty_sec(
      PowerNotificationController::kLowPowerMinutes * 60 - 20);
  {
    SCOPED_TRACE("Notification persists at lower values");
    UpdateNotificationState(
        low, PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN,
        false, false);
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
    UpdateNotificationState(
        low_usb, PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN,
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

TEST_P(PowerNotificationControllerWithBatterySaverTest,
       UpdateNotificationStateWithBSM) {

  // There should be no notification when we are above the threshold.
  PowerSupplyProperties battery_saver_low = DefaultPowerSupplyProperties();
  battery_saver_low.set_battery_percent(GetLowPowerPercentageExperiment() + 1);
  {
    SCOPED_TRACE("No notification when percentage above threshold");
    UpdateNotificationState(battery_saver_low,
                            PowerNotificationController::NOTIFICATION_NONE,
                            false, false);
  }

  // There should be an opt in/out notification when we are at the threshold.
  battery_saver_low.set_battery_percent(GetLowPowerPercentageExperiment());
  {
    SCOPED_TRACE("Notification when percentage matches threshold");
    UpdateNotificationState(battery_saver_low, GetLowPowerNotificationState(),
                            true, false);
  }

  // It should persist at lower values.
  battery_saver_low.set_battery_percent(GetLowPowerPercentageExperiment() - 1);
  {
    SCOPED_TRACE("Notification persists at lower values");
    UpdateNotificationState(battery_saver_low, GetLowPowerNotificationState(),
                            false, false);
  }

  // No notification when charging.
  battery_saver_low.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  battery_saver_low.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  {
    SCOPED_TRACE("No notification when charging");
    UpdateNotificationState(battery_saver_low,
                            PowerNotificationController::NOTIFICATION_NONE,
                            false, true);
  }

  // Notification reappears when discharging.
  battery_saver_low.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  battery_saver_low.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  {
    SCOPED_TRACE(
        "Notification when previously charging, but no longer charging.");
    UpdateNotificationState(battery_saver_low, GetLowPowerNotificationState(),
                            true, false);
  }

  // The critical low battery notification should be shown when the rounded
  // value is at the lower threshold.
  PowerSupplyProperties battery_saver_critical = DefaultPowerSupplyProperties();
  battery_saver_critical.set_battery_percent(
      GetCriticalPowerPercentageExperiment());
  {
    SCOPED_TRACE("Critical notification when time to empty is critical");
    UpdateNotificationState(battery_saver_critical,
                            PowerNotificationController::NOTIFICATION_CRITICAL,
                            true, true);
  }

  // The notification should be dismissed when the no-warning threshold is
  // reached.
  PowerSupplyProperties battery_saver_safe = DefaultPowerSupplyProperties();
  battery_saver_safe.set_battery_percent(GetNoWarningPercentageExperiment());
  {
    SCOPED_TRACE("Notification removed when battery not low");
    UpdateNotificationState(battery_saver_safe,
                            PowerNotificationController::NOTIFICATION_NONE,
                            false, true);
  }
}

TEST_P(PowerNotificationControllerWithBatterySaverTest,
       StickyOptStatusOnChargerUnplugHonorNoInput) {
  // Show Battery Saver notification by going below the threshold.
  PowerSupplyProperties battery_saver_low = DefaultPowerSupplyProperties();
  battery_saver_low.set_battery_percent(GetLowPowerPercentageExperiment() - 1);
  {
    SCOPED_TRACE("'Turning on BSM' Notification should appear.");
    UpdateNotificationState(battery_saver_low, GetLowPowerNotificationState(),
                            true, false);
    const Notification* notification =
        message_center()->FindVisibleNotificationById("battery");
    const std::vector<message_center::ButtonInfo> buttons =
        notification->buttons();
    EXPECT_EQ(static_cast<int>(buttons.size()), 1);
  }

  // Plug in charger.
  battery_saver_low.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  battery_saver_low.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  {
    SCOPED_TRACE("Notification should disappear due to charger plugged in.");
    UpdateNotificationState(battery_saver_low,
                            PowerNotificationController::NOTIFICATION_NONE,
                            false, true);
  }

  // Unplug charger.
  battery_saver_low.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  battery_saver_low.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  {
    SCOPED_TRACE(
        "'Turning on BSM' Notification should reappear due to charger "
        "unplugged.");
    UpdateNotificationState(battery_saver_low, GetLowPowerNotificationState(),
                            true, false);
  }
}

TEST_P(PowerNotificationControllerWithBatterySaverTest,
       StickyOptStatusOnChargerUnplugHonorViaButton) {
  // Show Battery Saver notification by going below the threshold.
  PowerSupplyProperties battery_saver_low = DefaultPowerSupplyProperties();
  battery_saver_low.set_battery_percent(GetLowPowerPercentageExperiment() - 1);
  {
    SCOPED_TRACE("'Turning on BSM' Notification should appear.");
    UpdateNotificationState(battery_saver_low, GetLowPowerNotificationState(),
                            true, false);

    const Notification* notification =
        message_center()->FindVisibleNotificationById("battery");
    const std::vector<message_center::ButtonInfo> buttons =
        notification->buttons();
    EXPECT_EQ(static_cast<int>(buttons.size()), 1);

    // Simulate Clicking Opt-Out/In
    notification->delegate()->Click(0, std::nullopt);
  }

  // Plug in charger.
  battery_saver_low.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  battery_saver_low.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  {
    SCOPED_TRACE("Notification should disappear due to charger plugged in.");
    UpdateNotificationState(battery_saver_low,
                            PowerNotificationController::NOTIFICATION_NONE,
                            false, true);
  }

  // Unplug charger.
  battery_saver_low.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  battery_saver_low.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  {
    SCOPED_TRACE(
        "Generic Low Power Notification should appear due to charger "
        "unplugged.");
    UpdateNotificationState(
        battery_saver_low,
        PowerNotificationController::NOTIFICATION_GENERIC_LOW_POWER, true,
        false);
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

TEST_F(PowerNotificationControllerTest,
       HistogramTest_TimeToEmptyForCriticalState) {
  // Verify no initial histogram data is recorded.
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyForCritialState", 0);

  // Set the default power state.
  PowerSupplyProperties proto = DefaultPowerSupplyProperties();
  EXPECT_EQ(PowerNotificationController::NOTIFICATION_NONE,
            notification_state());

  // Simulate setting to low power without reaching critical state, expecting no
  // metrics emitted.
  proto.set_battery_time_to_empty_sec(
      PowerNotificationController::kLowPowerMinutes * 60);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN,
      true, false);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyForCritialState", 0);

  // Set conditions to trigger a critical notification and record the metrics.
  proto.set_battery_time_to_empty_sec(
      PowerNotificationController::kCriticalMinutes * 60);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, true, true);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyForCritialState", 1);
  histogram_tester_->ExpectBucketCount(
      "Ash.PowerNotification.TimeToEmptyForCritialState", 300, 1);

  // Trigger another update; the state remains critical, so no additional
  // metrics should be emitted.
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, false, false);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyForCritialState", 1);
}

TEST_F(PowerNotificationControllerTest, HistogramTest_TimeToEmptyPluggedIn) {
  // Verify no initial histogram data is recorded.
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyPluggedIn", 0);

  // Set the default power state.
  PowerSupplyProperties proto = DefaultPowerSupplyProperties();
  EXPECT_EQ(PowerNotificationController::NOTIFICATION_NONE,
            notification_state());

  // Simulate plugging in the charger without any critical state.
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  UpdateNotificationState(proto, PowerNotificationController::NOTIFICATION_NONE,
                          false, false);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyPluggedIn", 0);

  // Simulate unplugging the charger.
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  UpdateNotificationState(proto, PowerNotificationController::NOTIFICATION_NONE,
                          false, false);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyPluggedIn", 0);

  // Transition to critical state.
  proto.set_battery_time_to_empty_sec(
      PowerNotificationController::kCriticalMinutes * 60);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, true, false);

  // Simulate plugging in the charger while in a critical state.
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  UpdateNotificationState(proto, PowerNotificationController::NOTIFICATION_NONE,
                          false, true);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyPluggedIn", 1);
  histogram_tester_->ExpectBucketCount(
      "Ash.PowerNotification.TimeToEmptyPluggedIn", 300, 1);

  // Trigger another update, the state remain normal, no additional metric
  // should be emitted.
  UpdateNotificationState(proto, PowerNotificationController::NOTIFICATION_NONE,
                          false, false);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.TimeToEmptyPluggedIn", 1);
}

TEST_F(PowerNotificationControllerTest,
       HistogramTest_CriticalNotificationOutcome_PluggedIn_Suspended) {
  // Verify no initial histogram data is recorded.
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationOutcome", 0);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration.PluggedIn",
      0);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration.Suspended",
      0);

  // Set power state to default and simulate critical battery level.
  PowerSupplyProperties proto = DefaultPowerSupplyProperties();
  EXPECT_EQ(PowerNotificationController::NOTIFICATION_NONE,
            notification_state());
  proto.set_battery_time_to_empty_sec(
      PowerNotificationController::kCriticalMinutes * 60);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, true, false);

  // Simulate device being plugged into power and validate outcomes.
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_AC);
  UpdateNotificationState(proto, PowerNotificationController::NOTIFICATION_NONE,
                          false, true);
  histogram_tester_->ExpectBucketCount(
      "Ash.PowerNotification.CriticalNotificationOutcome",
      PowerNotificationController::CriticalNotificationOutcome::PluggedIn, 1);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration.PluggedIn",
      1);

  // Unplug the power.
  proto.set_external_power(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, true, false);

  // Simulate suspending the device.
  SimulateSuspend();
  histogram_tester_->ExpectBucketCount(
      "Ash.PowerNotification.CriticalNotificationOutcome",
      PowerNotificationController::CriticalNotificationOutcome::Suspended, 1);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration.Suspended",
      1);
}

TEST_F(PowerNotificationControllerTest,
       HistogramTest_CriticalOutcome_UserShutdown) {
  // Verify no initial histogram data is recorded.
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationOutcome", 0);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration."
      "UserShutdown",
      0);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration."
      "LowBatteryShutdown",
      0);

  // Set default power state, and simulate setting to critical state.
  PowerSupplyProperties proto = DefaultPowerSupplyProperties();
  EXPECT_EQ(PowerNotificationController::NOTIFICATION_NONE,
            notification_state());
  proto.set_battery_time_to_empty_sec(
      PowerNotificationController::kCriticalMinutes * 60);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, true, false);

  // Simulate shutdown.
  SimulateShutdownByUser();
  histogram_tester_->ExpectBucketCount(
      "Ash.PowerNotification.CriticalNotificationOutcome",
      PowerNotificationController::CriticalNotificationOutcome::UserShutdown,
      1);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration."
      "UserShutdown",
      1);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration."
      "LowBatteryShutdown",
      0);
}

TEST_F(PowerNotificationControllerTest,
       HistogramTest_CriticalOutcome_RequestRestart) {
  // Verify no initial histogram data is recorded.
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationOutcome", 0);

  // Set default power state, and simulate setting to critical state.
  PowerSupplyProperties proto = DefaultPowerSupplyProperties();
  EXPECT_EQ(PowerNotificationController::NOTIFICATION_NONE,
            notification_state());
  proto.set_battery_time_to_empty_sec(
      PowerNotificationController::kCriticalMinutes * 60);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, true, false);
  // Record the NotificationShown outcome.
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationOutcome", 1);

  // Simulate restart.
  SimulateRestart();
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationOutcome", 1);
}

TEST_F(PowerNotificationControllerTest,
       HistogramTest_CriticalOutcome_LowBatteryShutdown) {
  // Verify no initial histogram data is recorded.
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationOutcome", 0);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration."
      "UserShutdown",
      0);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration."
      "LowBatteryShutdown",
      0);

  // Set the default power state and set to critical state.
  PowerSupplyProperties proto = DefaultPowerSupplyProperties();
  EXPECT_EQ(PowerNotificationController::NOTIFICATION_NONE,
            notification_state());
  proto.set_battery_time_to_empty_sec(5);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, true, false);

  // Simulate shutdown due to battery depletion and validate the expected
  // outcome.
  SimulateShutdownByPowerd();
  histogram_tester_->ExpectBucketCount(
      "Ash.PowerNotification.CriticalNotificationOutcome",
      PowerNotificationController::CriticalNotificationOutcome::
          LowBatteryShutdown,
      1);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration."
      "UserShutdown",
      0);
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationToOutcomeDuration."
      "LowBatteryShutdown",
      1);
}

TEST_F(PowerNotificationControllerTest,
       HistogramTest_CriticalOutcome_Notificationshown) {
  // Verify no initial histogram data is recorded.
  histogram_tester_->ExpectTotalCount(
      "Ash.PowerNotification.CriticalNotificationOutcome", 0);

  // Set the default power state and set to critical state.
  PowerSupplyProperties proto = DefaultPowerSupplyProperties();
  EXPECT_EQ(PowerNotificationController::NOTIFICATION_NONE,
            notification_state());
  proto.set_battery_time_to_empty_sec(5);
  UpdateNotificationState(
      proto, PowerNotificationController::NOTIFICATION_CRITICAL, true, false);

  histogram_tester_->ExpectBucketCount(
      "Ash.PowerNotification.CriticalNotificationOutcome",
      PowerNotificationController::CriticalNotificationOutcome::
          NotificationShown,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PowerNotificationControllerWithBatterySaverTest,
    testing::Values(features::BatterySaverNotificationBehavior::kBSMAutoEnable,
                    features::BatterySaverNotificationBehavior::kBSMOptIn));

}  // namespace ash
