// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_notifier.h"

#include <memory>

#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {

using DeviceId = InputDeviceSettingsController::DeviceId;

namespace {
const ui::InputDevice kSampleKeyboardInternal = {5, ui::INPUT_DEVICE_INTERNAL,
                                                 "kSampleKeyboardInternal"};
const ui::InputDevice kSampleKeyboardBluetooth = {
    10, ui::INPUT_DEVICE_BLUETOOTH, "kSampleKeyboardBluetooth"};
const ui::InputDevice kSampleKeyboardUsb = {15, ui::INPUT_DEVICE_USB,
                                            "kSampleKeyboardUsb"};

template <typename Comp = base::ranges::less>
void SortDevices(std::vector<ui::InputDevice>& devices, Comp comp = {}) {
  base::ranges::sort(devices, comp, [](const ui::InputDevice& keyboard) {
    return keyboard.id;
  });
}
}  // namespace

struct InputDeviceStateNotifierTestData {
  InputDeviceStateNotifierTestData() = default;
  InputDeviceStateNotifierTestData(
      std::vector<ui::InputDevice> initial_devices,
      std::vector<ui::InputDevice> updated_devices,
      std::vector<ui::InputDevice> expected_devices_to_add,
      std::vector<ui::InputDevice> expected_devices_to_remove)
      : initial_devices(initial_devices),
        updated_devices(updated_devices),
        expected_devices_to_add(expected_devices_to_add),
        expected_devices_to_remove(expected_devices_to_remove) {
    SortAllLists();
  }

  void SortAllLists() {
    SortDevices(initial_devices);
    SortDevices(updated_devices);
    SortDevices(expected_devices_to_add);
    SortDevices(expected_devices_to_remove);
  }

  std::vector<ui::InputDevice> initial_devices;
  std::vector<ui::InputDevice> updated_devices;
  std::vector<ui::InputDevice> expected_devices_to_add;
  std::vector<ui::InputDevice> expected_devices_to_remove;
};

class InputDeviceStateNotifierTest
    : public AshTestBase,
      public testing::WithParamInterface<InputDeviceStateNotifierTestData> {
 public:
  InputDeviceStateNotifierTest() = default;
  InputDeviceStateNotifierTest(const InputDeviceStateNotifierTest&) = delete;
  InputDeviceStateNotifierTest& operator=(const InputDeviceStateNotifierTest&) =
      delete;
  ~InputDeviceStateNotifierTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_data_ = GetParam();
    AshTestBase::SetUp();
    Initialize();

    notifier_ = std::make_unique<InputDeviceNotifier<mojom::KeyboardPtr>>(
        &keyboards_,
        base::BindRepeating(&InputDeviceStateNotifierTest::SaveNotifierResults,
                            base::Unretained(this)));
  }

  void TearDown() override {
    devices_to_add_.clear();
    device_ids_to_remove_.clear();
    AshTestBase::TearDown();
  }

  void SaveNotifierResults(std::vector<ui::InputDevice> devices_to_add,
                           std::vector<DeviceId> device_ids_to_remove) {
    devices_to_add_ = std::move(devices_to_add);
    device_ids_to_remove_ = std::move(device_ids_to_remove);
  }

  void Initialize() {
    for (const auto& device : test_data_.initial_devices) {
      auto keyboard = mojom::Keyboard::New();
      keyboard->id = device.id;
      keyboard->name = device.name;
      keyboards_[device.id] = std::move(keyboard);
    }

    ui::DeviceDataManagerTestApi().SetKeyboardDevices(
        test_data_.updated_devices);
  }

  void CheckNotifierResults() {
    ASSERT_EQ(test_data_.expected_devices_to_add.size(),
              devices_to_add_.size());
    for (size_t i = 0; i < test_data_.expected_devices_to_add.size(); i++) {
      EXPECT_EQ(test_data_.expected_devices_to_add[i].id,
                devices_to_add_[i].id);
      EXPECT_EQ(test_data_.expected_devices_to_add[i].name,
                devices_to_add_[i].name);
    }

    ASSERT_EQ(test_data_.expected_devices_to_remove.size(),
              device_ids_to_remove_.size());
    for (size_t i = 0; i < test_data_.expected_devices_to_remove.size(); i++) {
      EXPECT_EQ((DeviceId)test_data_.expected_devices_to_remove[i].id,
                device_ids_to_remove_[i]);
    }
  }

 protected:
  InputDeviceStateNotifierTestData test_data_;
  std::unique_ptr<InputDeviceNotifier<mojom::KeyboardPtr>> notifier_;
  base::flat_map<DeviceId, mojom::KeyboardPtr> keyboards_;

  std::vector<ui::InputDevice> devices_to_add_;
  std::vector<DeviceId> device_ids_to_remove_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    InputDeviceStateNotifierTest,
    testing::Values(
        // Empty input and update results in empty output.
        InputDeviceStateNotifierTestData({}, {}, {}, {}),

        // Empty at start and add 3 devices.
        InputDeviceStateNotifierTestData(
            {},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {}),

        // 3 devices at start and all are removed.
        InputDeviceStateNotifierTestData(
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {},
            {},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb}),

        // 3 devices at start and none are removed.
        InputDeviceStateNotifierTestData(
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {},
            {}),

        // 2 devices at start and middle id device is added.
        InputDeviceStateNotifierTestData(
            {kSampleKeyboardInternal, kSampleKeyboardUsb},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardBluetooth},
            {}),

        // 1 device at start which is removed when another is added.
        InputDeviceStateNotifierTestData({kSampleKeyboardInternal},
                                         {kSampleKeyboardBluetooth},
                                         {kSampleKeyboardBluetooth},
                                         {kSampleKeyboardInternal}),

        // 2 devices at start and a low id device is added.
        InputDeviceStateNotifierTestData(
            {kSampleKeyboardBluetooth, kSampleKeyboardUsb},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardInternal},
            {}),

        // 2 devices at start and a high id device is added.
        InputDeviceStateNotifierTestData(
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardUsb},
            {})));

TEST_P(InputDeviceStateNotifierTest, OnInputDeviceConfigurationChanged) {
  ui::DeviceDataManagerTestApi()
      .NotifyObserversKeyboardDeviceConfigurationChanged();
  CheckNotifierResults();
}

TEST_P(InputDeviceStateNotifierTest, OnDeviceListsComplete) {
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  CheckNotifierResults();
}

}  // namespace ash
