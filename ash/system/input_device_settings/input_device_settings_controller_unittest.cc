// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"

#include <memory>

#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_pref_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {

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

void AssertKeyboardsEqual(const ui::InputDevice& input_device_keyboard,
                          const mojom::KeyboardPtr& mojom_keyboard) {
  ASSERT_EQ(
      (InputDeviceSettingsControllerImpl::DeviceId)input_device_keyboard.id,
      mojom_keyboard->id);
  ASSERT_EQ(input_device_keyboard.name, mojom_keyboard->name);
}

void AssertKeyboardListsEqual(
    std::vector<ui::InputDevice> input_device_keyboards,
    const std::vector<mojom::KeyboardPtr>& mojom_keyboards) {
  SortDevices(input_device_keyboards);

  ASSERT_EQ(input_device_keyboards.size(), mojom_keyboards.size());
  for (size_t i = 0; i < input_device_keyboards.size(); i++) {
    AssertKeyboardsEqual(input_device_keyboards[i], mojom_keyboards[i]);
  }
}
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

class InputDeviceSettingsControllerTest : public AshTestBase {
 public:
  InputDeviceSettingsControllerTest() = default;
  InputDeviceSettingsControllerTest(const InputDeviceSettingsControllerTest&) =
      delete;
  InputDeviceSettingsControllerTest& operator=(
      const InputDeviceSettingsControllerTest&) = delete;
  ~InputDeviceSettingsControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    InitializeDeviceDataManager();

    observer_ = std::make_unique<FakeInputDeviceSettingsControllerObserver>();

    std::unique_ptr<FakeInputDevicePrefManager> pref_manager =
        std::make_unique<FakeInputDevicePrefManager>();
    pref_manager_ = pref_manager.get();

    controller_ = std::make_unique<InputDeviceSettingsControllerImpl>(
        std::move(pref_manager));
    controller_->AddObserver(observer_.get());
  }

  void TearDown() override {
    controller_->RemoveObserver(observer_.get());
    observer_.reset();

    pref_manager_ = nullptr;
    controller_.reset();
    AshTestBase::TearDown();
  }

  void InitializeDeviceDataManager() {
    sample_keyboards_ = {kSampleKeyboardUsb, kSampleKeyboardInternal,
                         kSampleKeyboardBluetooth};
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(sample_keyboards_);
  }

  template <typename Comp = base::ranges::less>
  void SortSampleKeyboards(Comp comp = {}) {
    SortDevices(sample_keyboards_, comp);
  }

 protected:
  std::vector<ui::InputDevice> sample_keyboards_;
  std::unique_ptr<InputDeviceSettingsControllerImpl> controller_;
  std::unique_ptr<FakeInputDeviceSettingsControllerObserver> observer_;

  FakeInputDevicePrefManager* pref_manager_ = nullptr;
};

TEST_F(InputDeviceSettingsControllerTest, InitializationTest) {
  EXPECT_NE(controller_.get(), nullptr);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardLoadingList) {
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());
  ASSERT_EQ(pref_manager_->num_keyboard_settings_initialized(),
            sample_keyboards_.size());
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardLoadingListUnsorted) {
  std::vector<ui::InputDevice> unsorted_sample_keyboards = sample_keyboards_;
  SortDevices(unsorted_sample_keyboards, base::ranges::greater());

  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());
  ASSERT_EQ(pref_manager_->num_keyboard_settings_initialized(),
            sample_keyboards_.size());
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardRemovingOneAtATime) {
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());

  while (!sample_keyboards_.empty()) {
    sample_keyboards_.pop_back();
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(sample_keyboards_);
    output_keyboards = controller_->GetConnectedKeyboards();
    AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
    EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());
  }
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardRemoveAllAtOnce) {
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual({}, output_keyboards);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardRemoveMiddleDevice) {
  sample_keyboards_ = {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
                       kSampleKeyboardUsb};
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(sample_keyboards_);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());

  sample_keyboards_ = {kSampleKeyboardInternal, kSampleKeyboardUsb};
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(sample_keyboards_);
  output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardRemoveMultipleDevices) {
  sample_keyboards_ = {kSampleKeyboardInternal, kSampleKeyboardBluetooth,
                       kSampleKeyboardUsb};
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(sample_keyboards_);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());

  sample_keyboards_ = {kSampleKeyboardUsb};
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(sample_keyboards_);
  output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddOneAtATime) {
  std::vector<ui::InputDevice> keyboards = {};
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboards);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(keyboards, output_keyboards);

  for (const auto& keyboard : sample_keyboards_) {
    keyboards.push_back(keyboard);
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboards);
    output_keyboards = controller_->GetConnectedKeyboards();
    AssertKeyboardListsEqual(keyboards, output_keyboards);
  }

  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(),
            sample_keyboards_.size());
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddOneAtATimeUnsorted) {
  SortSampleKeyboards(base::ranges::greater());

  std::vector<ui::InputDevice> keyboards = {};
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboards);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(keyboards, output_keyboards);

  for (const auto& keyboard : sample_keyboards_) {
    keyboards.push_back(keyboard);
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboards);
    output_keyboards = controller_->GetConnectedKeyboards();
    AssertKeyboardListsEqual(keyboards, output_keyboards);
  }

  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(),
            sample_keyboards_.size());
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddMoreAfterInitialization) {
  SortSampleKeyboards(base::ranges::greater());

  sample_keyboards_ = {kSampleKeyboardInternal, kSampleKeyboardUsb};
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(sample_keyboards_);
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  auto output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());

  sample_keyboards_.push_back(kSampleKeyboardBluetooth);
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(sample_keyboards_);

  output_keyboards = controller_->GetConnectedKeyboards();
  AssertKeyboardListsEqual(sample_keyboards_, output_keyboards);
  EXPECT_EQ(sample_keyboards_.size(), observer_->num_keyboards_connected());
  EXPECT_EQ(pref_manager_->num_keyboard_settings_initialized(),
            sample_keyboards_.size());
}

}  // namespace ash
