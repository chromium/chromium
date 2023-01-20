// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <memory>

#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_pref_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
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
}  // namespace

class FakeInputDevicePrefManager : public InputDevicePrefManager {
 public:
  void InitializeKeyboardSettings(mojom::Keyboard* keyboard) override {
    num_keyboard_settings_initialized_++;
  }

  uint32_t num_keyboard_settings_initialized() {
    return num_keyboard_settings_initialized_;
  }

 private:
  uint32_t num_keyboard_settings_initialized_ = 0;
};

class FakeInputDeviceSettingsControllerObserver
    : public InputDeviceSettingsController::Observer {
 public:
  void OnKeyboardConnected(const mojom::Keyboard& keyboard) override {
    num_keyboards_connected_++;
  }

  void OnKeyboardDisconnected(const mojom::Keyboard& keyboard) override {
    num_keyboards_connected_--;
  }

  uint32_t num_keyboards_connected() { return num_keyboards_connected_; }

 private:
  uint32_t num_keyboards_connected_;
};

class InputDeviceSettingsControllerTest : public testing::Test {
 public:
  InputDeviceSettingsControllerTest() = default;
  InputDeviceSettingsControllerTest(const InputDeviceSettingsControllerTest&) =
      delete;
  InputDeviceSettingsControllerTest& operator=(
      const InputDeviceSettingsControllerTest&) = delete;
  ~InputDeviceSettingsControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    observer_ = std::make_unique<FakeInputDeviceSettingsControllerObserver>();

    std::unique_ptr<FakeInputDevicePrefManager> pref_manager =
        std::make_unique<FakeInputDevicePrefManager>();
    pref_manager_ = pref_manager.get();

    controller_ = std::make_unique<InputDeviceSettingsControllerImpl>(
        std::move(pref_manager));
    controller_->AddObserver(observer_.get());

    sample_keyboards_ = {kSampleKeyboardUsb, kSampleKeyboardInternal,
                         kSampleKeyboardBluetooth};
  }

  void TearDown() override {
    controller_->RemoveObserver(observer_.get());
    observer_.reset();

    pref_manager_ = nullptr;
    controller_.reset();
  }

 protected:
  std::vector<ui::InputDevice> sample_keyboards_;
  std::unique_ptr<InputDeviceSettingsControllerImpl> controller_;
  std::unique_ptr<FakeInputDeviceSettingsControllerObserver> observer_;

  FakeInputDevicePrefManager* pref_manager_ = nullptr;
};

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingOne) {
  controller_->OnKeyboardListUpdated({kSampleKeyboardUsb}, {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingMultiple) {
  controller_->OnKeyboardListUpdated(
      {kSampleKeyboardUsb, kSampleKeyboardInternal, kSampleKeyboardBluetooth},
      {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 3u);
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingThenRemovingOne) {
  controller_->OnKeyboardListUpdated({kSampleKeyboardUsb}, {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(), 1u);

  controller_->OnKeyboardListUpdated({}, {(DeviceId)kSampleKeyboardUsb.id});

  EXPECT_EQ(observer_->num_keyboards_connected(), 0u);
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingThenRemovingMultiple) {
  controller_->OnKeyboardListUpdated(
      {kSampleKeyboardUsb, kSampleKeyboardInternal, kSampleKeyboardBluetooth},
      {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 3u);
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(), 3u);

  controller_->OnKeyboardListUpdated(
      {},
      {(DeviceId)kSampleKeyboardUsb.id, (DeviceId)kSampleKeyboardInternal.id,
       (DeviceId)kSampleKeyboardBluetooth.id});

  EXPECT_EQ(observer_->num_keyboards_connected(), 0u);
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingAndRemoving) {
  controller_->OnKeyboardListUpdated({kSampleKeyboardUsb}, {});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(), 1u);

  controller_->OnKeyboardListUpdated({kSampleKeyboardInternal},
                                     {(DeviceId)kSampleKeyboardUsb.id});

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(), 2u);
}

}  // namespace ash
