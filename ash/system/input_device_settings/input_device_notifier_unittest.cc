// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_notifier.h"

#include <memory>

#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {

using DeviceId = InputDeviceSettingsController::DeviceId;

namespace {
const ui::KeyboardDevice kSampleKeyboardInternal = {
    5, ui::INPUT_DEVICE_INTERNAL, "kSampleKeyboardInternal"};
const ui::KeyboardDevice kSampleKeyboardBluetooth = {
    10, ui::INPUT_DEVICE_BLUETOOTH, "kSampleKeyboardBluetooth"};
const ui::KeyboardDevice kSampleKeyboardUsb = {15, ui::INPUT_DEVICE_USB,
                                               "kSampleKeyboardUsb"};
const ui::KeyboardDevice kLogitechMXKeysKeyboard = {
    20,
    ui::INPUT_DEVICE_BLUETOOTH,
    "Logitech MX Keys",
    /*phys=*/"",
    /*sys_path=*/base::FilePath(),
    /*vendor=*/0x046d,
    /*product=*/0xb35b,
    /*version=*/0x0};

const ui::InputDevice kSampleMouseUsb = {20, ui::INPUT_DEVICE_USB,
                                         "kSampleMouseUsb"};
const ui::InputDevice kSampleMouseBluetooth = {25, ui::INPUT_DEVICE_BLUETOOTH,
                                               "kSampleMouseBluetooth"};
const ui::InputDevice kSampleMouseInternal = {30, ui::INPUT_DEVICE_INTERNAL,
                                              "kSampleMouseInternal"};

template <typename Comp = base::ranges::less>
void SortDevices(std::vector<ui::KeyboardDevice>& devices, Comp comp = {}) {
  base::ranges::sort(devices, comp, [](const ui::KeyboardDevice& keyboard) {
    return keyboard.id;
  });
}
}  // namespace

struct InputDeviceNotifierParamaterizedTestData {
  InputDeviceNotifierParamaterizedTestData() = default;
  InputDeviceNotifierParamaterizedTestData(
      std::vector<ui::KeyboardDevice> initial_devices,
      std::vector<ui::KeyboardDevice> updated_devices,
      std::vector<ui::KeyboardDevice> expected_devices_to_add,
      std::vector<ui::KeyboardDevice> expected_devices_to_remove)
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

  std::vector<ui::KeyboardDevice> initial_devices;
  std::vector<ui::KeyboardDevice> updated_devices;
  std::vector<ui::KeyboardDevice> expected_devices_to_add;
  std::vector<ui::KeyboardDevice> expected_devices_to_remove;
};

class InputDeviceStateNotifierTest : public AshTestBase {
 public:
  InputDeviceStateNotifierTest() = default;
  InputDeviceStateNotifierTest(const InputDeviceStateNotifierTest&) = delete;
  InputDeviceStateNotifierTest& operator=(const InputDeviceStateNotifierTest&) =
      delete;
  ~InputDeviceStateNotifierTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    notifier_ = std::make_unique<
        InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>>(
        &keyboards_,
        base::BindRepeating(&InputDeviceStateNotifierTest::SaveNotifierResults,
                            base::Unretained(this)));
  }

  void TearDown() override {
    devices_to_add_.clear();
    device_ids_to_remove_.clear();
    AshTestBase::TearDown();
  }

  void SaveNotifierResults(std::vector<ui::KeyboardDevice> devices_to_add,
                           std::vector<DeviceId> device_ids_to_remove) {
    devices_to_add_ = std::move(devices_to_add);
    device_ids_to_remove_ = std::move(device_ids_to_remove);
  }

 protected:
  std::unique_ptr<InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>>
      notifier_;
  base::flat_map<DeviceId, mojom::KeyboardPtr> keyboards_;

  std::vector<ui::KeyboardDevice> devices_to_add_;
  std::vector<DeviceId> device_ids_to_remove_;
};

TEST_F(InputDeviceStateNotifierTest, ImpostersRemoved) {
  ui::KeyboardDevice imposter_keyboard = kSampleKeyboardBluetooth;
  imposter_keyboard.suspected_imposter = true;

  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {imposter_keyboard, kSampleKeyboardUsb});
  ASSERT_EQ(1u, devices_to_add_.size());
  EXPECT_EQ(kSampleKeyboardUsb.name, devices_to_add_[0].name);
  EXPECT_EQ(kSampleKeyboardUsb.id, devices_to_add_[0].id);

  imposter_keyboard.suspected_imposter = false;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {imposter_keyboard, kSampleKeyboardUsb});
  ASSERT_EQ(2u, devices_to_add_.size());
  EXPECT_EQ(imposter_keyboard.name, devices_to_add_[0].name);
  EXPECT_EQ(imposter_keyboard.id, devices_to_add_[0].id);
  EXPECT_EQ(kSampleKeyboardUsb.name, devices_to_add_[1].name);
  EXPECT_EQ(kSampleKeyboardUsb.id, devices_to_add_[1].id);
}

// Since Logitech MX Keys pretends to be a mouse, it should always be shown as a
// keyboard even if it is a suspected imposter. We know better due to our
// block/allowlist.
TEST_F(InputDeviceStateNotifierTest,
       KeyboardWhichImpersonatesMouseAlwaysShown) {
  ui::KeyboardDevice imposter_keyboard = kLogitechMXKeysKeyboard;
  imposter_keyboard.suspected_imposter = true;

  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, imposter_keyboard});
  ASSERT_EQ(2u, devices_to_add_.size());
  EXPECT_EQ(kSampleKeyboardUsb.name, devices_to_add_[0].name);
  EXPECT_EQ(kSampleKeyboardUsb.id, devices_to_add_[0].id);
  EXPECT_EQ(imposter_keyboard.name, devices_to_add_[1].name);
  EXPECT_EQ(imposter_keyboard.id, devices_to_add_[1].id);

  imposter_keyboard.suspected_imposter = false;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, imposter_keyboard});
  ASSERT_EQ(2u, devices_to_add_.size());
  EXPECT_EQ(kSampleKeyboardUsb.name, devices_to_add_[0].name);
  EXPECT_EQ(kSampleKeyboardUsb.id, devices_to_add_[0].id);
  EXPECT_EQ(imposter_keyboard.name, devices_to_add_[1].name);
  EXPECT_EQ(imposter_keyboard.id, devices_to_add_[1].id);
}

class InputDeviceNotifierParamaterizedTest
    : public InputDeviceStateNotifierTest,
      public testing::WithParamInterface<
          InputDeviceNotifierParamaterizedTestData> {
 public:
  InputDeviceNotifierParamaterizedTest() = default;
  InputDeviceNotifierParamaterizedTest(
      const InputDeviceNotifierParamaterizedTest&) = delete;
  InputDeviceNotifierParamaterizedTest& operator=(
      const InputDeviceNotifierParamaterizedTest&) = delete;
  ~InputDeviceNotifierParamaterizedTest() override = default;

  // testing::Test:
  void SetUp() override {
    InputDeviceStateNotifierTest::SetUp();
    test_data_ = GetParam();
    Initialize();

    notifier_ = std::make_unique<
        InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>>(
        &keyboards_,
        base::BindRepeating(
            &InputDeviceNotifierParamaterizedTest::SaveNotifierResults,
            base::Unretained(this)));
  }

  void TearDown() override {
    devices_to_add_.clear();
    device_ids_to_remove_.clear();
    AshTestBase::TearDown();
  }

  void SaveNotifierResults(std::vector<ui::KeyboardDevice> devices_to_add,
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
  InputDeviceNotifierParamaterizedTestData test_data_;
  std::unique_ptr<InputDeviceNotifier<mojom::KeyboardPtr, ui::KeyboardDevice>>
      notifier_;
  base::flat_map<DeviceId, mojom::KeyboardPtr> keyboards_;

  std::vector<ui::KeyboardDevice> devices_to_add_;
  std::vector<DeviceId> device_ids_to_remove_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    InputDeviceNotifierParamaterizedTest,
    testing::Values(
        // Empty input and update results in empty output.
        InputDeviceNotifierParamaterizedTestData({}, {}, {}, {}),

        // Empty at start and add 3 devices.
        InputDeviceNotifierParamaterizedTestData(
            {},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {}),

        // 3 devices at start and all are removed.
        InputDeviceNotifierParamaterizedTestData(
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {},
            {},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb}),

        // 3 devices at start and none are removed.
        InputDeviceNotifierParamaterizedTestData(
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {},
            {}),

        // 2 devices at start and middle id device is added.
        InputDeviceNotifierParamaterizedTestData(
            {kSampleKeyboardInternal, kSampleKeyboardUsb},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardBluetooth},
            {}),

        // 1 device at start which is removed when another is added.
        InputDeviceNotifierParamaterizedTestData({kSampleKeyboardInternal},
                                                 {kSampleKeyboardBluetooth},
                                                 {kSampleKeyboardBluetooth},
                                                 {kSampleKeyboardInternal}),

        // 2 devices at start and a low id device is added.
        InputDeviceNotifierParamaterizedTestData(
            {kSampleKeyboardBluetooth, kSampleKeyboardUsb},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardInternal},
            {}),

        // 2 devices at start and a high id device is added.
        InputDeviceNotifierParamaterizedTestData(
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth},
            {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
             kSampleKeyboardUsb},
            {kSampleKeyboardUsb},
            {})));

TEST_P(InputDeviceNotifierParamaterizedTest,
       OnInputDeviceConfigurationChanged) {
  ui::DeviceDataManagerTestApi()
      .NotifyObserversKeyboardDeviceConfigurationChanged();
  CheckNotifierResults();
}

TEST_P(InputDeviceNotifierParamaterizedTest, OnDeviceListsComplete) {
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  CheckNotifierResults();
}

class InputDeviceMouseNotifierTest : public AshTestBase {
 public:
  InputDeviceMouseNotifierTest() = default;
  InputDeviceMouseNotifierTest(const InputDeviceMouseNotifierTest&) = delete;
  InputDeviceMouseNotifierTest& operator=(const InputDeviceMouseNotifierTest&) =
      delete;
  ~InputDeviceMouseNotifierTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    notifier_ =
        std::make_unique<InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>>(
            &mice_, base::BindRepeating(
                        &InputDeviceMouseNotifierTest::SaveNotifierResults,
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

 protected:
  InputDeviceNotifierParamaterizedTestData test_data_;
  std::unique_ptr<InputDeviceNotifier<mojom::MousePtr, ui::InputDevice>>
      notifier_;
  base::flat_map<DeviceId, mojom::MousePtr> mice_;

  std::vector<ui::InputDevice> devices_to_add_;
  std::vector<DeviceId> device_ids_to_remove_;
};

// When an internal mouse in the list received from DeviceDataManager, filter it
// out as this is likely not a real device.
TEST_F(InputDeviceMouseNotifierTest, InternalMiceFilteredOut) {
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {kSampleMouseUsb, kSampleMouseBluetooth, kSampleMouseInternal});
  EXPECT_TRUE(device_ids_to_remove_.empty());
  ASSERT_EQ(2u, devices_to_add_.size());
  EXPECT_EQ(kSampleMouseUsb.name, devices_to_add_[0].name);
  EXPECT_EQ(kSampleMouseUsb.id, devices_to_add_[0].id);
  EXPECT_EQ(kSampleMouseBluetooth.name, devices_to_add_[1].name);
  EXPECT_EQ(kSampleMouseBluetooth.id, devices_to_add_[1].id);
}

}  // namespace ash
