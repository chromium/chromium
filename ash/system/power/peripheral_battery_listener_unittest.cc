// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_listener.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "ash/shell.h"
#include "ash/system/power/peripheral_battery_tests.h"
#include "ash/test/ash_test_base.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/message_center/public/cpp/notification.h"

using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::Eq;
using testing::Expectation;
using testing::Field;
using testing::Ge;
using testing::Gt;
using testing::InSequence;
using testing::Le;
using testing::Lt;
using testing::NiceMock;
using testing::Optional;
using testing::Sequence;
using testing::StrictMock;

using BI = ash::PeripheralBatteryListener::BatteryInfo;
using BatteryInfo = device::BluetoothDevice::BatteryInfo;
using BatteryType = device::BluetoothDevice::BatteryType;

// Annotate testing::Field invocations to improve feedback.
#define AFIELD(element, test) testing::Field(#element, element, test)

namespace {

class MockPeripheralBatteryObserver
    : public ash::PeripheralBatteryListener::Observer {
 public:
  MockPeripheralBatteryObserver() {}

  // ash::PeripheralBatteryListener::Observer:
  MOCK_METHOD(void,
              OnAddingBattery,
              (const ash::PeripheralBatteryListener::BatteryInfo& battery));
  MOCK_METHOD(void,
              OnRemovingBattery,
              (const ash::PeripheralBatteryListener::BatteryInfo& battery));
  MOCK_METHOD(void,
              OnUpdatedBatteryLevel,
              (const ash::PeripheralBatteryListener::BatteryInfo& battery));
};

}  // namespace

namespace ash {

class PeripheralBatteryListenerTest : public AshTestBase {
 public:
  // Constants for active field of PeripheralBatteryStylusReceived().
  const bool kBluetoothBatteryUpdate = true;
  const bool kBatteryPolledUpdate = false;
  const bool kBatteryEventUpdate = true;

  PeripheralBatteryListenerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  PeripheralBatteryListenerTest(const PeripheralBatteryListenerTest&) = delete;
  PeripheralBatteryListenerTest& operator=(
      const PeripheralBatteryListenerTest&) = delete;
  ~PeripheralBatteryListenerTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    AshTestBase::SetUp();
    ASSERT_TRUE(ui::DeviceDataManager::HasInstance());

    // Simulate the complete listing of input devices, required by the listener.
    if (complete_devices_)
      ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    mock_device_1_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), /*bluetooth_class=*/0, kBluetoothDeviceName1,
        kBluetoothDeviceAddress1, /*paired=*/true, /*connected=*/true);
    mock_device_2_ = std::make_unique<NiceMock<device::MockBluetoothDevice>>(
        mock_adapter_.get(), /*bluetooth_class=*/0, kBluetoothDeviceName2,
        kBluetoothDeviceAddress2, /*paired=*/true, /*connected=*/true);

    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    battery_listener_ = std::make_unique<PeripheralBatteryListener>();
  }

  void TearDown() override {

    battery_listener_.reset();
    AshTestBase::TearDown();
    chromeos::PowerManagerClient::Shutdown();
  }

  base::TimeTicks GetTestingClock() { return base::TimeTicks::Now(); }

  void ClockAdvance(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
  }

  void CreateInternalTouchscreen(bool garage) {
    // Add an internal stylus to our test device manager.

    ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_INTERNAL,
                                 kTestStylusName, gfx::Size(),
                                 /*touch_points=*/1, /*has_stylus=*/true,
                                 /*has_stylus_garage_switch=*/garage);
    stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

    ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});
  }

  void CreateExternalTouchscreen() {
    // Add an external stylus to our test device manager.
    ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB,
                                 kTestStylusName, gfx::Size(),
                                 /*touch_points=*/1, /*has_stylus=*/true);
    stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

    ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});
  }

 protected:
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_1_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_2_;
  std::unique_ptr<PeripheralBatteryListener> battery_listener_;

  void set_complete_devices(bool complete_devices) {
    complete_devices_ = complete_devices;
  }

  // SetUp() doesn't complete devices if this is set to false.
  bool complete_devices_ = true;
};

class PeripheralBatteryListenerIncompleteDevicesTest
    : public PeripheralBatteryListenerTest {
 public:
  PeripheralBatteryListenerIncompleteDevicesTest() {
    set_complete_devices(false);
  }

  PeripheralBatteryListenerIncompleteDevicesTest(
      const PeripheralBatteryListenerIncompleteDevicesTest&) = delete;
  PeripheralBatteryListenerIncompleteDevicesTest& operator=(
      const PeripheralBatteryListenerIncompleteDevicesTest&) = delete;

  ~PeripheralBatteryListenerIncompleteDevicesTest() override {}
};

TEST_F(PeripheralBatteryListenerTest, Basic) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::Seconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp, Eq(std::nullopt)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status, Eq(kTestBatteryStatusOut)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  // Level 5 at time 110, listener should be notified.
  ClockAdvance(base::Seconds(10));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp, Eq(std::nullopt)),
                  AFIELD(&BI::level, Eq(5)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  // Level -1 at time 115, listener should be notified.
  ClockAdvance(base::Seconds(5));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp, Eq(std::nullopt)),
                  AFIELD(&BI::level, Eq(std::nullopt)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, -1, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  // Level 50 at time 120, listener should be notified.
  ClockAdvance(base::Seconds(5));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp, Eq(std::nullopt)),
                  AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, ActiveUpdates) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::Seconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kTestBatteryId)),
                AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                AFIELD(&BI::charge_status, Eq(kTestBatteryStatusOut)),
                AFIELD(&BI::last_active_update_timestamp, Eq(std::nullopt)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  // Level 5 at time 110, listener should be notified.
  ClockAdvance(base::Seconds(10));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp,
                         Optional(GetTestingClock())))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryEventUpdate);

  // Level -1 at time 115, listener should be notified.
  ClockAdvance(base::Seconds(5));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp,
                         Optional(GetTestingClock())))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, -1, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryEventUpdate);

  auto prior_active_update_time = GetTestingClock();

  // Level 50 at time 120, listener should be notified.
  ClockAdvance(base::Seconds(5));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp,
                         Optional(prior_active_update_time)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, FirstActiveUpdates) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::Seconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestBatteryId)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::last_active_update_timestamp,
                         Optional(GetTestingClock())))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryEventUpdate);
}

TEST_F(PeripheralBatteryListenerTest, InvalidBatteryInfo) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const std::string invalid_path1 = "invalid-path";
  const std::string invalid_path2 = "/sys/class/power_supply/hid-battery";

  EXPECT_CALL(listener_observer_mock, OnAddingBattery(_)).Times(0);
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_)).Times(0);

  battery_listener_->PeripheralBatteryStatusReceived(
      invalid_path1, kTestDeviceName, 10, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  battery_listener_->PeripheralBatteryStatusReceived(
      invalid_path2, kTestDeviceName, 10, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, -2, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 101, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  // Note that -1 is a valid battery level for the Listener, so not checked.
}

// Verify that for Bluetooth devices, the correct address gets stored in the
// BatteryInfo's bluetooth_address member, and for non-Bluetooth devices, that
// bluetooth_address member is empty.
TEST_F(PeripheralBatteryListenerTest, ExtractBluetoothAddress) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const std::string bluetooth_path =
      "/sys/class/power_supply/hid-A0:b1:C2:d3:E4:f5-battery";
  const std::string expected_bluetooth_address = "a0:b1:c2:d3:e4:f5";
  const std::string expected_bluetooth_id =
      "battery_bluetooth-a0:b1:c2:d3:e4:f5";
  const std::string non_bluetooth_path =
      "/sys/class/power_supply/hid-notbluetooth-battery";

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(expected_bluetooth_id))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(expected_bluetooth_id)),
                                  AFIELD(&BI::level, Eq(10)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      bluetooth_path, kTestDeviceName, 10, kTestBatteryStatusIn,
      /*serial_number=*/"", kBluetoothBatteryUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(non_bluetooth_path))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(non_bluetooth_path)),
                                  AFIELD(&BI::bluetooth_address, Eq("")))));

  battery_listener_->PeripheralBatteryStatusReceived(
      non_bluetooth_path, kTestDeviceName, 10, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, DeviceRemove) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

TEST_F(PeripheralBatteryListenerTest, StylusNotification) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const std::string kTestStylusBatteryPath =
      "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusName = "test_stylus";
  const auto kTestStylusBatteryStatusDischargingIn = power_manager::
      PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING;
  const auto kTestStylusBatteryStatusDischargingOut =
      BI::ChargeStatus::kDischarging;

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryPolledUpdate);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                                  AFIELD(&BI::level, Eq(5)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 5,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryEventUpdate);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                                  AFIELD(&BI::level, Eq(std::nullopt)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, -1,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryEventUpdate);
}

TEST_F(PeripheralBatteryListenerTest,
       Bluetooth_CreatesANotificationForEachDevice) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId1)), AFIELD(&BI::level, Eq(5)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(kBluetoothDeviceName116)),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId2)), AFIELD(&BI::level, Eq(0)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(kBluetoothDeviceName216)),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress2)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));
  mock_device_2_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/0));
}

TEST_F(PeripheralBatteryListenerTest,
       Bluetooth_RemovesNotificationForDisconnectedDevices) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId1)), AFIELD(&BI::level, Eq(5)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(kBluetoothDeviceName116)),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId2)), AFIELD(&BI::level, Eq(0)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(kBluetoothDeviceName216)),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress2)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));
  mock_device_2_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/0));

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));

  // Verify only the notification for device 1 gets removed.
  battery_listener_->DeviceConnectedStateChanged(mock_adapter_.get(),
                                                 mock_device_1_.get(), false);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));

  // Remove the second notification.
  battery_listener_->DeviceRemoved(mock_adapter_.get(), mock_device_2_.get());
}

TEST_F(PeripheralBatteryListenerTest,
       Bluetooth_RemovesNotificationForDisconnectedDevicesInOtherOrder) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId2)),
                AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress2)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));
  mock_device_2_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId2))));

  // Remove the second notification.
  battery_listener_->DeviceRemoved(mock_adapter_.get(), mock_device_2_.get());

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));

  // Verify only the notification for device 1 gets removed.
  battery_listener_->DeviceConnectedStateChanged(mock_adapter_.get(),
                                                 mock_device_1_.get(), false);
}

TEST_F(PeripheralBatteryListenerTest, Bluetooth_RemoveAndReconnect) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));

  battery_listener_->DeviceConnectedStateChanged(mock_adapter_.get(),
                                                 mock_device_1_.get(), false);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));
}

TEST_F(PeripheralBatteryListenerTest,
       Bluetooth_CancelNotificationForInvalidBatteryLevel) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                                  AFIELD(&BI::level, Eq(1)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                                  AFIELD(&BI::level, Eq(std::nullopt)))));

  mock_device_1_->RemoveBatteryInfo(BatteryType::kDefault);
}

// Do notify observer if the battery level drops again under the
// threshold before kNotificationInterval is completed.
TEST_F(PeripheralBatteryListenerTest, EnsureUpdatesWithinSmallTimeIntervals) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  ClockAdvance(base::Seconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(1)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));

  ClockAdvance(base::Seconds(1));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(std::nullopt)))));
  mock_device_1_->RemoveBatteryInfo(BatteryType::kDefault);

  ClockAdvance(base::Seconds(1));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(1)))));
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));
}

// Notify observer if the battery is under threshold, then unknown level and
// then is again under the threshold after kNotificationInterval is completed.
// (Listener should not pay attention to kNotificationInterval anyway.)
TEST_F(PeripheralBatteryListenerTest,
       PostNotificationIfBatteryGoesFromUnknownLevelToBelowThreshold) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  ClockAdvance(base::Seconds(100));

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(1)))));
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));

  ClockAdvance(base::Seconds(1));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(std::nullopt)))));
  mock_device_1_->RemoveBatteryInfo(BatteryType::kDefault);

  ClockAdvance(base::Seconds(100));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(1)))));
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/1));
}

// If there is an existing notification and the battery level remains low,
// update its content.
TEST_F(PeripheralBatteryListenerTest, UpdateNotificationIfVisible) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  ClockAdvance(base::Seconds(100));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(5)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));

  // The battery level remains low, should update the notification.
  ClockAdvance(base::Seconds(100));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kBluetoothDeviceId1)),
                  AFIELD(&BI::last_update_timestamp, Eq(GetTestingClock())),
                  AFIELD(&BI::level, Eq(3)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/3));
}

TEST_F(PeripheralBatteryListenerTest, MultipleObserversCoexist) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock_1;
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock_2;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs_1{&listener_observer_mock_1};
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs_2{&listener_observer_mock_2};

  scoped_listener_obs_1.Observe(battery_listener_.get());
  scoped_listener_obs_2.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock_1,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock_2,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock_1,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kTestBatteryId)),
                                          AFIELD(&BI::level, Eq(50)))));
  EXPECT_CALL(listener_observer_mock_2,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::key, Eq(kTestBatteryId)),
                                          AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 50, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerTest, ObserverationLifetimeObeyed) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  testing::InSequence sequence;

  // Connect observer, add and remove battery
  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);

  // Disconnect observer, add and remove battery

  scoped_listener_obs.Reset();

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);

  // Reconnect observer, add and remove battery

  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnRemovingBattery(AFIELD(&BI::key, Eq(kTestBatteryId))));

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

// Check that observers only see events occuring while they are connected.
TEST_F(PeripheralBatteryListenerTest, PartialObserverationLifetimeObeyed) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  testing::InSequence sequence;

  // Connect observer, add and remove battery.
  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  // Disconnect observer before we remove battery.

  scoped_listener_obs.Reset();

  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);

  // Reconnect battery.

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", true);

  // Reconnect observer, add and remove battery.

  EXPECT_CALL(listener_observer_mock, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));
  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock, OnRemovingBattery(_));
  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

// Check that observers will get events to 'catch up' on batteries they missed.
TEST_F(PeripheralBatteryListenerTest, PartialObserverationLifetimeCatchUp) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  testing::InSequence sequence;

  // Connect battery.

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryEventUpdate);

  EXPECT_CALL(listener_observer_mock, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock, OnUpdatedBatteryLevel(_));
  scoped_listener_obs.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock, OnRemovingBattery(_));
  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

TEST_F(PeripheralBatteryListenerTest, MultipleObserverationLifetimeObeyed) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock_1;
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock_2;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs_1{&listener_observer_mock_1};
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs_2{&listener_observer_mock_2};

  testing::InSequence sequence;

  scoped_listener_obs_1.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock_1, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock_1, OnUpdatedBatteryLevel(_));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock_2, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock_2, OnUpdatedBatteryLevel(_));
  scoped_listener_obs_2.Observe(battery_listener_.get());

  EXPECT_CALL(listener_observer_mock_1, OnRemovingBattery(_));
  EXPECT_CALL(listener_observer_mock_2, OnRemovingBattery(_));
  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);

  scoped_listener_obs_1.Reset();

  EXPECT_CALL(listener_observer_mock_2, OnAddingBattery(_));
  EXPECT_CALL(listener_observer_mock_2, OnUpdatedBatteryLevel(_));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestBatteryPath, kTestDeviceName, 5, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock_2, OnRemovingBattery(_));
  battery_listener_->RemoveBluetoothBattery(kTestBatteryAddress);
}

TEST_F(PeripheralBatteryListenerTest, Charger) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kTestChargerId)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                AFIELD(&BI::level, Eq(50)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      /*serial_number=*/"", kBatteryEventUpdate);
}

// TODO(b/215381232): Temporarily support both 'PCHG' name and 'peripheral' name
// till upstream kernel driver is merged. Remove test case when upstream kernel
// driver is merged.
TEST_F(PeripheralBatteryListenerTest, Charger_PCHG) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kTestChargerId)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                AFIELD(&BI::level, Eq(50)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestPCHGChargerPath, kTestChargerName, 50,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      /*serial_number=*/"", kBatteryEventUpdate);
}

TEST_F(PeripheralBatteryListenerTest, ChargerError) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kTestChargerId)), AFIELD(&BI::level, Eq(50)),
          AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kError)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_ERROR,
      /*serial_number=*/"", kBatteryPolledUpdate);
}

// Check that zero value from charger is accepted, this may happen
// if it is charging from a deep discharge.
TEST_F(PeripheralBatteryListenerTest, StylusChargingFromDeepDischarge) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kTestChargerId)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
          AFIELD(&BI::level, Eq(0)),
          AFIELD(&BI::charge_status, Eq(PeripheralBatteryListener::BatteryInfo::
                                            ChargeStatus::kCharging)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 0,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      /*serial_number=*/"", kBatteryEventUpdate);
}

// Check that zero value from charger is accepted, even if there was
// an existing non-zero value from another stylus.
TEST_F(PeripheralBatteryListenerTest, StylusChargingFromDeepDischarge2) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const int nonZeroBatteryLevel = 74;

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));

  // First establish a stylus is charging at a normal level
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kTestChargerId)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
          AFIELD(&BI::level, Eq(nonZeroBatteryLevel)),
          AFIELD(&BI::charge_status, Eq(PeripheralBatteryListener::BatteryInfo::
                                            ChargeStatus::kCharging)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, nonZeroBatteryLevel,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      /*serial_number=*/"", kBatteryEventUpdate);

  // Then put on a stylus that is in deep discharge
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(0)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 0,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      /*serial_number=*/"", kBatteryEventUpdate);
}

TEST_F(PeripheralBatteryListenerTest, ChargerErrorTransition) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kTestChargerId)), AFIELD(&BI::level, Eq(50)),
          AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      /*serial_number=*/"", kBatteryPolledUpdate);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kTestChargerId)), AFIELD(&BI::level, Eq(50)),
          AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kError)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_ERROR,
      /*serial_number=*/"", kBatteryEventUpdate);
}

// Stylus-via-screen updates of level zero should be translated to
// nullopt as zero is not a valid level, but may come through during
// boot or other device creation scenarios.
TEST_F(PeripheralBatteryListenerTest, StylusDiscardsZeros) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  CreateExternalTouchscreen();

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 0,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryEventUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryEventUpdate);

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(std::nullopt)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 0,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryEventUpdate);
}

// Stylus-via-charger updates of level zero should translate to nullopt if
// no value is known; otherwise they should be ignored as not providing
// information.
TEST_F(PeripheralBatteryListenerTest, StylusChargerDoesNullZeros) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestChargerId))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestChargerId)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status, Eq(kTestBatteryStatusOut)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 0, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryEventUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryEventUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(50)))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 0, kTestBatteryStatusIn,
      /*serial_number=*/"", kBatteryEventUpdate);
}

// Bluetooth/other HID updates of level zero should come through as expected, as
// we don't know that 0 is invalid.
TEST_F(PeripheralBatteryListenerTest, BluetoothDoesNotDiscardZeros) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId1)), AFIELD(&BI::level, Eq(0)),
          AFIELD(&BI::type, Eq(BI::PeripheralType::kOther)),
          AFIELD(&BI::name, Eq(kBluetoothDeviceName116)),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/0));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(AFIELD(&BI::level, Eq(5)))));

  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));
}

// Stylus garage charging

TEST_F(PeripheralBatteryListenerIncompleteDevicesTest,
       DoNotSynthesizeGarageCharger) {
  // Create touchscreen w/ stylus, w/o dockswitch
  // Verify Stylus Garage does not exist
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  testing::InSequence sequence;

  CreateInternalTouchscreen(false);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::Seconds(100));

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))));

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerIncompleteDevicesTest,
       DoSynthesizeGarageCharger) {
  // Create touchscreen w/ stylus, w/ dockswitch
  // Stylus is not garaged at start
  // Trigger touchscreen stylus update event
  // Verify Stylus Garage does exist
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  CreateInternalTouchscreen(true);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::Seconds(100));

  Expectation a = EXPECT_CALL(
      listener_observer_mock,
      OnAddingBattery(AFIELD(&BI::key, Eq(kStylusChargerDeviceName))));

  Expectation b = EXPECT_CALL(
      listener_observer_mock,
      OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))));

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kUnknown)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .After(a);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .After(b);

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryPolledUpdate);
}

TEST_F(PeripheralBatteryListenerIncompleteDevicesTest, GarageCharging) {
  // Create touchscreen w/ stylus, w/ dockswitch
  // Stylus not in dock at beginning
  // Put stylus on charger, do not have it touch screen
  // Wait for it to come to a full charge
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  CreateInternalTouchscreen(true);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::Seconds(100));

  Sequence a, b;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kStylusChargerDeviceName))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))))
      .InSequence(b);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kUnknown)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(b);

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryPolledUpdate);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(a, b);

  battery_listener_->OnStylusStateChanged(ui::StylusState::INSERTED);
}

TEST_F(PeripheralBatteryListenerIncompleteDevicesTest, GarageChargesFully) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  CreateInternalTouchscreen(true);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::Seconds(100));

  Sequence a, b;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kStylusChargerDeviceName))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))))
      .InSequence(b);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kUnknown)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(b);

  // This is a polled update, so it doesn't count as timely information
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryPolledUpdate);

  // This will be called once the stylus is inserted, and called repeatedly
  // until the stylus is estimated to be fully charged. Since we started
  // without a known level for the stylus, the level will start from 1, counting
  // up to 99 until the charge is believed complete.
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                AFIELD(&BI::level, Lt(100)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)))))
      .Times(AnyNumber())
      .InSequence(a, b);

  battery_listener_->OnStylusStateChanged(ui::StylusState::INSERTED);

  // Then we should have one update at 100% charge.
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(100)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kFull)))))
      .InSequence(a, b);

  // Move time forward more than enough to fully charge, ensuring timers fire.
  task_environment()->FastForwardBy(base::Seconds(kFullGarageChargeTime));
}

TEST_F(PeripheralBatteryListenerIncompleteDevicesTest,
       GarageChargesFullyFromFiftyPercent) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  CreateInternalTouchscreen(true);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  scoped_listener_obs.Observe(battery_listener_.get());

  // Level 50 at time 100, listener should be notified.
  ClockAdvance(base::Seconds(100));

  Sequence a, b;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kStylusChargerDeviceName))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))))
      .InSequence(b);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kUnknown)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(50)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(b);

  // This is an active update, so states that the stylus level is definitely
  // 50%.
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryEventUpdate);

  // The rest of these are strictly sequential
  testing::InSequence sequence;

  // This will be called once the stylus is inserted, and called repeatedly
  // until the stylus is estimated to be fully charged. Since we started
  // with a known level for the stylus the level start there, indicating that
  // original level until the charge is complete.
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                AFIELD(&BI::level, Ge(50)), AFIELD(&BI::level, Le(99)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)))))
      .Times(AnyNumber());

  battery_listener_->OnStylusStateChanged(ui::StylusState::INSERTED);

  // Then we should have one update at 100% charge.
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(100)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kFull)))));

  // Move time forward more than enough to fully charge, ensuring timers fire.
  task_environment()->FastForwardBy(base::Seconds(kFullGarageChargeTime));
}

TEST_F(PeripheralBatteryListenerIncompleteDevicesTest,
       GarageChargingInterrupted) {
  // Create touchscreen w/ stylus, w/ dockswitch, w/o stylus in garage
  // Put stylus on in garage
  // Wait for it to start charging
  // Remove from charger
  // Ensure it stops charging
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  CreateInternalTouchscreen(true);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  scoped_listener_obs.Observe(battery_listener_.get());

  Sequence a, b;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kStylusChargerDeviceName))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))))
      .InSequence(b);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kUnknown)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(1)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(b);

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 1,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryEventUpdate);

  ClockAdvance(base::Seconds(100));

  // The rest of these are strictly sequential
  testing::InSequence sequence;

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                AFIELD(&BI::level, Eq(1)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)))))
      .Times(AnyNumber());

  battery_listener_->OnStylusStateChanged(ui::StylusState::INSERTED);

  // Move time forward more than enough to start charging.
  task_environment()->FastForwardBy(base::Seconds(3));

  // Remove stylus from garage

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                AFIELD(&BI::level, Eq(1)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kUnknown)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)))));

  battery_listener_->OnStylusStateChanged(ui::StylusState::REMOVED);

  // Move time forward enough for anything to go wrong with the timers.
  task_environment()->FastForwardBy(base::Seconds(kPartialGarageChargeTime));
}

TEST_F(PeripheralBatteryListenerIncompleteDevicesTest, GarageChargingResumed) {
  // Create touchscreen w/ stylus, w/ dockswitch, w/o stylus in garage
  // Put stylus on in garage
  // Wait for it to start charging
  // Remove from charger
  // Replace on charger
  // Ensure it finishes charging
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};

  CreateInternalTouchscreen(true);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  scoped_listener_obs.Observe(battery_listener_.get());

  Sequence a, b;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kStylusChargerDeviceName))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))))
      .InSequence(b);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(std::nullopt)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kUnknown)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(a);

  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                  AFIELD(&BI::level, Eq(1)),
                  AFIELD(&BI::charge_status,
                         Eq(kTestStylusBatteryStatusDischargingOut)),
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                  AFIELD(&BI::bluetooth_address, Eq("")))))
      .InSequence(b);

  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 1,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryEventUpdate);

  ClockAdvance(base::Seconds(100));

  // The rest of these are strictly sequential
  testing::InSequence sequence;

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                AFIELD(&BI::level, Eq(1)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)))))
      .Times(AnyNumber());

  battery_listener_->OnStylusStateChanged(ui::StylusState::INSERTED);

  // Move time forward more than enough to start charging.
  task_environment()->FastForwardBy(base::Seconds(kPartialGarageChargeTime));

  // Remove stylus from garage

  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                AFIELD(&BI::level, Eq(1)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kUnknown)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)))));

  battery_listener_->OnStylusStateChanged(ui::StylusState::REMOVED);

  // Move time forward enough for anything to go wrong with the timers.
  task_environment()->FastForwardBy(base::Seconds(kPartialGarageChargeTime));

  // Replace stylus, let run to full charge.

  // The level at the start should be unchanged, it's still the last known
  // level and it won't update until charge is definitely complete.
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(
          AllOf(AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                AFIELD(&BI::level, Eq(1)),
                AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kCharging)),
                AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaCharger)))))
      .Times(AnyNumber());

  // Then we should have one update at 100% charge.
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AllOf(
                  AFIELD(&BI::key, Eq(kStylusChargerDeviceName)),
                  AFIELD(&BI::level, Eq(100)),
                  AFIELD(&BI::charge_status, Eq(BI::ChargeStatus::kFull)))));

  battery_listener_->OnStylusStateChanged(ui::StylusState::INSERTED);

  // Move time forward more than enough to fully charge.
  task_environment()->FastForwardBy(base::Seconds(kFullGarageChargeTime));
}

// NOTE: Cannot yet mock OzonePlatform::GetInstance()->GetInputController(),
// so cannot test scenarios involving stylus on charger from 'boot'.

#if 0
TEST_F(PeripheralBatteryListenerIncompleteDevicesTest,
       StylusGaragedOnBoot) {
  // Create touchscreen w/ stylus, w/ dockswitch
  // Have stylus on charger from boot
  // Ensure that it starts on full charge
}
#endif

TEST_F(PeripheralBatteryListenerTest, StylusBatteryEligibility) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  const std::string kTestStylusBatteryPath =
      "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
  const std::string kTestStylusName = "test_stylus";
  const auto kTestStylusBatteryStatusDischargingIn = power_manager::
      PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING;
  const auto kTestStylusBatteryStatusDischargingOut =
      BI::ChargeStatus::kDischarging;

  // Add an external stylus to our test device manager.
  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  testing::InSequence sequence;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kTestStylusBatteryPath))));

  for (const char* sn : kStylusEligibleSerialNumbers) {
    EXPECT_CALL(listener_observer_mock,
                OnUpdatedBatteryLevel(AllOf(
                    AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                    AFIELD(&BI::battery_report_eligible, Eq(true)),
                    AFIELD(&BI::level, Eq(50)),
                    AFIELD(&BI::charge_status,
                           Eq(kTestStylusBatteryStatusDischargingOut)),
                    AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen)),
                    AFIELD(&BI::bluetooth_address, Eq("")))));

    battery_listener_->PeripheralBatteryStatusReceived(
        kTestStylusBatteryPath, kTestStylusName, 50,
        kTestStylusBatteryStatusDischargingIn, sn, kBatteryPolledUpdate);
  }

  for (const char* sn : kStylusIneligibleSerialNumbers) {
    EXPECT_CALL(listener_observer_mock,
                OnUpdatedBatteryLevel(
                    AllOf(AFIELD(&BI::key, Eq(kTestStylusBatteryPath)),
                          AFIELD(&BI::level, Eq(5)),
                          AFIELD(&BI::battery_report_eligible, Eq(false)))));

    battery_listener_->PeripheralBatteryStatusReceived(
        kTestStylusBatteryPath, kTestStylusName, 5,
        kTestStylusBatteryStatusDischargingIn, sn, kBatteryEventUpdate);
  }
}

TEST_F(PeripheralBatteryListenerTest,
       PostNofiticationWhenDeviceIsConnectedWithLowBattery) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());

  Sequence a;
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId1)), AFIELD(&BI::level, Eq(5)),
          AFIELD(&BI::battery_report_eligible, Eq(true)),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))))
      .InSequence(a);
  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(&BI::key, Eq(kBluetoothDeviceId1))));
  mock_device_1_->SetBatteryInfo(
      BatteryInfo(BatteryType::kDefault, /*percentage=*/5));

  Sequence b;
  EXPECT_CALL(
      listener_observer_mock,
      OnUpdatedBatteryLevel(AllOf(
          AFIELD(&BI::key, Eq(kBluetoothDeviceId1)), AFIELD(&BI::level, Eq(5)),
          AFIELD(&BI::battery_report_eligible, Eq(true)),
          AFIELD(&BI::bluetooth_address, Eq(kBluetoothDeviceAddress1)))))
      .InSequence(b);

  battery_listener_->DeviceConnectedStateChanged(
      mock_adapter_.get(), mock_device_1_.get(), /*is_now_connected=*/true);
}

TEST_F(PeripheralBatteryListenerTest,
       IneligibleBatteryReportingStylusViaScreenNoSerialNumber) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());
  base::HistogramTester histogram_tester_;

  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen))));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, /*serial_number=*/"",
      kBatteryPolledUpdate);
  histogram_tester_.ExpectUniqueSample(
      kStylusBatteryReportingEligibilityHistogramName,
      StylusBatteryReportingEligibility::kIneligibleDueToScreen, 1);
}

TEST_F(PeripheralBatteryListenerTest,
       EligibleBatteryReportingStylusViaChargerNoSerialNumber) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());
  base::HistogramTester histogram_tester_;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(
                  &BI::type, Eq(BI::PeripheralType::kStylusViaCharger))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AFIELD(
                  &BI::type, Eq(BI::PeripheralType::kStylusViaCharger))));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      /*serial_number=*/"", kBatteryEventUpdate);
  histogram_tester_.ExpectUniqueSample(
      kStylusBatteryReportingEligibilityHistogramName,
      StylusBatteryReportingEligibility::kEligible, 1);
}

TEST_F(PeripheralBatteryListenerTest,
       IncorrectReportingStylusViaChargerWithIneligibleSerialNumber) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());
  base::HistogramTester histogram_tester_;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(
                  &BI::type, Eq(BI::PeripheralType::kStylusViaCharger))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AFIELD(
                  &BI::type, Eq(BI::PeripheralType::kStylusViaCharger))));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      kStylusIneligibleSerialNumbers[0], kBatteryEventUpdate);
  histogram_tester_.ExpectUniqueSample(
      kStylusBatteryReportingEligibilityHistogramName,
      StylusBatteryReportingEligibility::kIncorrectReports, 1);
}

TEST_F(PeripheralBatteryListenerTest,
       IncorrectReportingStylusViaScreenWithIneligibleSerialNumber) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());
  base::HistogramTester histogram_tester_;

  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen))));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, kStylusIneligibleSerialNumbers[0],
      kBatteryPolledUpdate);
  histogram_tester_.ExpectUniqueSample(
      kStylusBatteryReportingEligibilityHistogramName,
      StylusBatteryReportingEligibility::kIncorrectReports, 1);
}

TEST_F(PeripheralBatteryListenerTest,
       EligibleBatteryReportingStylusViaChargerWithEligibleSerialNumber) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());
  base::HistogramTester histogram_tester_;

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(AFIELD(
                  &BI::type, Eq(BI::PeripheralType::kStylusViaCharger))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(AFIELD(
                  &BI::type, Eq(BI::PeripheralType::kStylusViaCharger))));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestChargerPath, kTestChargerName, 50,
      power_manager::
          PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING,
      kStylusEligibleSerialNumbers[0], kBatteryEventUpdate);
  histogram_tester_.ExpectUniqueSample(
      kStylusBatteryReportingEligibilityHistogramName,
      StylusBatteryReportingEligibility::kEligible, 1);
}

TEST_F(PeripheralBatteryListenerTest,
       EligibleBatteryReportingStylusViaScreenWithEligibleSerialNumber) {
  testing::StrictMock<MockPeripheralBatteryObserver> listener_observer_mock;
  base::ScopedObservation<PeripheralBatteryListener,
                          PeripheralBatteryListener::Observer>
      scoped_listener_obs{&listener_observer_mock};
  scoped_listener_obs.Observe(battery_listener_.get());
  base::HistogramTester histogram_tester_;

  ui::TouchscreenDevice stylus(/*id=*/0, ui::INPUT_DEVICE_USB, kTestStylusName,
                               gfx::Size(),
                               /*touch_points=*/1, /*has_stylus=*/true);
  stylus.sys_path = base::FilePath(kTestStylusBatteryPath);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({stylus});

  EXPECT_CALL(listener_observer_mock,
              OnAddingBattery(
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen))));
  EXPECT_CALL(listener_observer_mock,
              OnUpdatedBatteryLevel(
                  AFIELD(&BI::type, Eq(BI::PeripheralType::kStylusViaScreen))));
  battery_listener_->PeripheralBatteryStatusReceived(
      kTestStylusBatteryPath, kTestStylusName, 50,
      kTestStylusBatteryStatusDischargingIn, kStylusEligibleSerialNumbers[0],
      kBatteryPolledUpdate);
  histogram_tester_.ExpectUniqueSample(
      kStylusBatteryReportingEligibilityHistogramName,
      StylusBatteryReportingEligibility::kEligible, 1);
}

// TODO: Test needed for eligibility behaviour of stylus chargers.

}  // namespace ash
