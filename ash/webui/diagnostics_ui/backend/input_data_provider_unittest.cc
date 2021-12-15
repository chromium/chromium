// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"

#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace ash {
namespace diagnostics {

namespace {

constexpr mojom::TopRowKey kClassicTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kForward,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp};

constexpr mojom::TopRowKey kInternalJinlonTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenshot,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kPrivacyScreenToggle,
    mojom::TopRowKey::kKeyboardBacklightDown,
    mojom::TopRowKey::kKeyboardBacklightUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp};

// One possible variant of a Dell configuration
constexpr mojom::TopRowKey kInternalDellTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp,
    mojom::TopRowKey::kNone,
    mojom::TopRowKey::kNone,
    mojom::TopRowKey::kScreenMirror,
    mojom::TopRowKey::kDelete};

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayoutAttributeName[] = "function_row_physmap";

constexpr char kSillyDeviceName[] = "eventWithoutANumber";

constexpr char kInvalidMechnicalLayout[] = "Not ANSI, JIS, or ISO";

// NOTE: This is only creates a simple ui::InputDevice based on a device
// capabilities report; it is not suitable for subclasses of ui::InputDevice.
ui::InputDevice InputDeviceFromCapabilities(
    int device_id,
    const ui::DeviceCapabilities& capabilities) {
  ui::EventDeviceInfo device_info = {};
  ui::CapabilitiesToDeviceInfo(capabilities, &device_info);

  const std::string sys_path =
      base::StringPrintf("/dev/input/event%d-%s", device_id, capabilities.path);

  return ui::InputDevice(device_id, device_info.device_type(),
                         device_info.name(), device_info.phys(),
                         base::FilePath(sys_path), device_info.vendor_id(),
                         device_info.product_id(), device_info.version());
}

}  // namespace

class FakeDeviceManager : public ui::DeviceManager {
 public:
  FakeDeviceManager() {}
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() override {}

  // DeviceManager:
  void ScanDevices(ui::DeviceEventObserver* observer) override {}
  void AddObserver(ui::DeviceEventObserver* observer) override {}
  void RemoveObserver(ui::DeviceEventObserver* observer) override {}
};

class FakeConnectedDevicesObserver : public mojom::ConnectedDevicesObserver {
 public:
  // mojom::ConnectedDevicesObserver:
  void OnTouchDeviceConnected(
      mojom::TouchDeviceInfoPtr new_touch_device) override {
    touch_devices_connected.push_back(std::move(new_touch_device));
  }
  void OnTouchDeviceDisconnected(uint32_t id) override {
    touch_devices_disconnected.push_back(id);
  }
  void OnKeyboardConnected(mojom::KeyboardInfoPtr new_keyboard) override {
    keyboards_connected.push_back(std::move(new_keyboard));
  }
  void OnKeyboardDisconnected(uint32_t id) override {
    keyboards_disconnected.push_back(id);
  }

  std::vector<mojom::TouchDeviceInfoPtr> touch_devices_connected;
  std::vector<uint32_t> touch_devices_disconnected;
  std::vector<mojom::KeyboardInfoPtr> keyboards_connected;
  std::vector<uint32_t> keyboards_disconnected;

  mojo::Receiver<mojom::ConnectedDevicesObserver> receiver{this};
};

class FakeInputDeviceInfoHelper : public InputDeviceInfoHelper {
 public:
  FakeInputDeviceInfoHelper() {}

  ~FakeInputDeviceInfoHelper() override {}

  std::unique_ptr<InputDeviceInformation> GetDeviceInfo(
      int id,
      base::FilePath path) override {
    ui::DeviceCapabilities device_caps;
    const std::string base_name = path.BaseName().value();

    if (base_name == "event0") {
      device_caps = ui::kLinkKeyboard;
      EXPECT_EQ(0, id);
    } else if (base_name == "event1") {
      device_caps = ui::kLinkTouchpad;
      EXPECT_EQ(1, id);
    } else if (base_name == "event2") {
      device_caps = ui::kKohakuTouchscreen;
      EXPECT_EQ(2, id);
    } else if (base_name == "event3") {
      device_caps = ui::kKohakuStylus;
      EXPECT_EQ(3, id);
    } else if (base_name == "event4") {
      device_caps = ui::kHpUsbKeyboard;
      EXPECT_EQ(4, id);
    } else if (base_name == "event5") {
      device_caps = ui::kSarienKeyboard;  // Wilco
      EXPECT_EQ(5, id);
    } else if (base_name == "event6") {
      device_caps = ui::kEveKeyboard;
      EXPECT_EQ(6, id);
    } else if (base_name == "event7") {
      device_caps = ui::kJinlonKeyboard;
      EXPECT_EQ(7, id);
    } else if (base_name == "event8") {
      device_caps = ui::kMicrosoftBluetoothNumberPad;
      EXPECT_EQ(8, id);
    } else if (base_name == "event9") {
      device_caps = ui::kLogitechTouchKeyboardK400;
      EXPECT_EQ(9, id);
    } else if (base_name == kSillyDeviceName) {
      // Simulate a device that is properly described, but has a malformed
      // device name.
      EXPECT_EQ(98, id);
      device_caps = ui::kLinkKeyboard;
    } else if (base_name == "event99") {
      EXPECT_EQ(99, id);
      // Simulate a device that couldn't be opened or have its info determined
      // for whatever reason.
      return nullptr;
    }

    auto info = std::make_unique<InputDeviceInformation>();

    EXPECT_TRUE(
        ui::CapabilitiesToDeviceInfo(device_caps, &info->event_device_info));
    info->evdev_id = id;
    info->path = path;
    info->input_device =
        InputDeviceFromCapabilities(info->evdev_id, device_caps);
    info->connection_type =
        InputDataProvider::ConnectionTypeFromInputDeviceType(
            info->event_device_info.device_type());

    return info;
  }
};

class TestInputDataProvider : public InputDataProvider {
 public:
  TestInputDataProvider(std::unique_ptr<ui::DeviceManager> device_manager)
      : InputDataProvider(std::move(device_manager)) {
    info_helper_ = base::SequenceBound<FakeInputDeviceInfoHelper>(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }
  explicit TestInputDataProvider(const TestInputDataProvider&) = delete;
  TestInputDataProvider& operator=(const TestInputDataProvider&) = delete;
};

class InputDataProviderTest : public testing::Test {
 public:
  InputDataProviderTest() {
    statistics_provider_.SetMachineStatistic(
        chromeos::system::kKeyboardMechanicalLayoutKey, "ANSI");
    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    auto manager = std::make_unique<FakeDeviceManager>();
    manager_ = manager.get();
    fake_udev_ = std::make_unique<testing::FakeUdevLoader>();
    provider_ = std::make_unique<TestInputDataProvider>(std::move(manager));

    // Apply these early; delaying until
    // FakeInputDeviceInfoHelper::GetDeviceInfo() is not appropriate, as
    // fake_udev is not thread safe. (If multiple devices are constructed in a
    // row, then GetDeviceInfo() invocation can overlap with
    // ProcessInputDataProvider::ProcessDeviceInfo() which reads from udev).
    UdevAddFakeDeviceCapabilities("/dev/input/event5", ui::kSarienKeyboard);
    UdevAddFakeDeviceCapabilities("/dev/input/event6", ui::kEveKeyboard);
    UdevAddFakeDeviceCapabilities("/dev/input/event7", ui::kJinlonKeyboard);
  }

  void UdevAddFakeDeviceCapabilities(
      const std::string& device_name,
      const ui::DeviceCapabilities& device_caps) {
    std::map<std::string, std::string>
        sysfs_properties;  // Old style numeric tags
    std::map<std::string, std::string>
        sysfs_attributes;  // New style vivaldi scancode layouts

    if (device_caps.kbd_function_row_physmap &&
        strlen(device_caps.kbd_function_row_physmap) > 0) {
      sysfs_attributes[kKbdTopRowLayoutAttributeName] =
          device_caps.kbd_function_row_physmap;
    }

    if (device_caps.kbd_top_row_layout &&
        strlen(device_caps.kbd_top_row_layout) > 0) {
      sysfs_properties[kKbdTopRowPropertyName] = device_caps.kbd_top_row_layout;
    }

    // Each device needs a unique sys path
    const std::string sys_path = device_name + "-" + device_caps.path;

    fake_udev_->AddFakeDevice(device_caps.name, sys_path.c_str(),
                              /*subsystem=*/"input", /*devnode=*/absl::nullopt,
                              /*devtype=*/absl::nullopt,
                              std::move(sysfs_attributes),
                              std::move(sysfs_properties));
  }

  ~InputDataProviderTest() override {
    provider_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeDeviceManager* manager_;
  std::unique_ptr<testing::FakeUdevLoader> fake_udev_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<InputDataProvider> provider_;
};

TEST_F(InputDataProviderTest, GetConnectedDevices_DeviceInfoMapping) {
  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event1"));
  ui::DeviceEvent event2(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event2"));
  ui::DeviceEvent event3(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event3"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  provider_->OnDeviceEvent(event2);
  provider_->OnDeviceEvent(event3);
  task_environment_.RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();
  const auto& touch_devices = future.Get<1>();

  ASSERT_EQ(1ul, keyboards.size());
  // The stylus device should be filtered out, hence only 2 touch devices.
  ASSERT_EQ(2ul, touch_devices.size());

  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(0u, keyboard->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, keyboard->connection_type);
  EXPECT_EQ("AT Translated Set 2 keyboard", keyboard->name);

  const mojom::TouchDeviceInfoPtr& touchpad = touch_devices[0];
  EXPECT_EQ(1u, touchpad->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, touchpad->connection_type);
  EXPECT_EQ(mojom::TouchDeviceType::kPointer, touchpad->type);
  EXPECT_EQ("Atmel maXTouch Touchpad", touchpad->name);

  const mojom::TouchDeviceInfoPtr& touchscreen = touch_devices[1];
  EXPECT_EQ(2u, touchscreen->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, touchscreen->connection_type);
  EXPECT_EQ(mojom::TouchDeviceType::kDirect, touchscreen->type);
  EXPECT_EQ("Atmel maXTouch Touchscreen", touchscreen->name);
}

TEST_F(InputDataProviderTest, GetConnectedDevices_AddEventAfterFirstCall) {
  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();
    ASSERT_EQ(0ul, keyboards.size());
    ASSERT_EQ(0ul, touch_devices.size());
  }

  ui::DeviceEvent event(ui::DeviceEvent::DeviceType::INPUT,
                        ui::DeviceEvent::ActionType::ADD,
                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(event);
  task_environment_.RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    ASSERT_EQ(1ul, keyboards.size());
    const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
    EXPECT_EQ(4u, keyboard->id);
    EXPECT_EQ(mojom::ConnectionType::kUsb, keyboard->connection_type);
    EXPECT_EQ("Chicony HP Elite USB Keyboard", keyboard->name);

    EXPECT_EQ(0ul, touch_devices.size());
  }
}

TEST_F(InputDataProviderTest, GetConnectedDevices_AddUnusualDevices) {
  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event8"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event9"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  task_environment_.RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();
  const auto& touch_devices = future.Get<1>();

  ASSERT_EQ(2ul, keyboards.size());
  // The stylus device should be filtered out, hence only 2 touch devices.
  ASSERT_EQ(0ul, touch_devices.size());

  const mojom::KeyboardInfoPtr& keyboard1 = keyboards[0];
  EXPECT_EQ(8u, keyboard1->id);
  EXPECT_EQ(mojom::ConnectionType::kBluetooth, keyboard1->connection_type);
  EXPECT_EQ(ui::kMicrosoftBluetoothNumberPad.name, keyboard1->name);

  const mojom::KeyboardInfoPtr& keyboard2 = keyboards[1];
  EXPECT_EQ(9u, keyboard2->id);
  EXPECT_EQ(mojom::ConnectionType::kUnknown, keyboard2->connection_type);
  EXPECT_EQ(ui::kLogitechTouchKeyboardK400.name, keyboard2->name);
}

TEST_F(InputDataProviderTest, GetConnectedDevices_Remove) {
  ui::DeviceEvent add_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                  ui::DeviceEvent::ActionType::ADD,
                                  base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_touch_event);
  ui::DeviceEvent add_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                ui::DeviceEvent::ActionType::ADD,
                                base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_kbd_event);
  task_environment_.RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    ASSERT_EQ(1ul, keyboards.size());
    EXPECT_EQ(4u, keyboards[0]->id);

    ASSERT_EQ(1ul, touch_devices.size());
    EXPECT_EQ(1u, touch_devices[0]->id);
  }

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_kbd_event);
  task_environment_.RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    EXPECT_EQ(0ul, keyboards.size());
    EXPECT_EQ(0ul, touch_devices.size());
  }
}

TEST_F(InputDataProviderTest, KeyboardPhysicalLayoutDetection) {
  statistics_provider_.SetMachineStatistic(
      chromeos::system::kKeyboardMechanicalLayoutKey, "ISO");

  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event4"));
  ui::DeviceEvent event2(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event5"));
  ui::DeviceEvent event3(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event7"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  provider_->OnDeviceEvent(event2);
  provider_->OnDeviceEvent(event3);
  task_environment_.RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(4ul, keyboards.size());

  const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
  EXPECT_EQ(0u, builtin_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
            builtin_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso, builtin_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            builtin_keyboard->number_pad_present);
  EXPECT_EQ(
      std::vector(std::begin(kClassicTopRowKeys), std::end(kClassicTopRowKeys)),
      builtin_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& external_keyboard = keyboards[1];
  EXPECT_EQ(4u, external_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kUnknown,
            external_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
            external_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kUnknown,
            external_keyboard->number_pad_present);
  EXPECT_EQ(
      std::vector(std::begin(kClassicTopRowKeys), std::end(kClassicTopRowKeys)),
      external_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& dell_internal_keyboard = keyboards[2];
  EXPECT_EQ(5u, dell_internal_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOSDellEnterprise,
            dell_internal_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso,
            dell_internal_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            dell_internal_keyboard->number_pad_present);
  EXPECT_EQ(std::vector(std::begin(kInternalDellTopRowKeys),
                        std::end(kInternalDellTopRowKeys)),
            dell_internal_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& jinlon_internal_keyboard = keyboards[3];
  EXPECT_EQ(7u, jinlon_internal_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
            jinlon_internal_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso,
            jinlon_internal_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            jinlon_internal_keyboard->number_pad_present);
  EXPECT_EQ(std::vector(std::begin(kInternalJinlonTopRowKeys),
                        std::end(kInternalJinlonTopRowKeys)),
            jinlon_internal_keyboard->top_row_keys);

  // TODO(b/208729519): We should check a Drallion keyboard, however that
  // invokes a check through the global Shell that does not operate in
  // this test.
}

TEST_F(InputDataProviderTest, KeyboardAssistantKeyDetection) {
  ui::DeviceEvent link_event(ui::DeviceEvent::DeviceType::INPUT,
                             ui::DeviceEvent::ActionType::ADD,
                             base::FilePath("/dev/input/event0"));
  ui::DeviceEvent eve_event(ui::DeviceEvent::DeviceType::INPUT,
                            ui::DeviceEvent::ActionType::ADD,
                            base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(link_event);
  provider_->OnDeviceEvent(eve_event);
  task_environment_.RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());
  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(2ul, keyboards.size());

  const mojom::KeyboardInfoPtr& link_keyboard = keyboards[0];
  EXPECT_EQ(0u, link_keyboard->id);
  EXPECT_FALSE(link_keyboard->has_assistant_key);
  const mojom::KeyboardInfoPtr& eve_keyboard = keyboards[1];
  EXPECT_EQ(6u, eve_keyboard->id);
  EXPECT_TRUE(eve_keyboard->has_assistant_key);
}

TEST_F(InputDataProviderTest, KeyboardNumberPadDetectionInternal) {
  // Detection of internal number pad depends on command-line
  // argument, and is not a property of the keyboard device.

  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--has-number-pad"});
  ui::DeviceEvent link_event(ui::DeviceEvent::DeviceType::INPUT,
                             ui::DeviceEvent::ActionType::ADD,
                             base::FilePath("/dev/input/event0"));
  provider_->OnDeviceEvent(link_event);
  task_environment_.RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());
  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
  EXPECT_EQ(0u, builtin_keyboard->id);
  EXPECT_EQ(mojom::NumberPadPresence::kPresent,
            builtin_keyboard->number_pad_present);
}

TEST_F(InputDataProviderTest, ObserveConnectedDevices_Keyboards) {
  FakeConnectedDevicesObserver fake_observer;
  provider_->ObserveConnectedDevices(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_keyboard_event);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.keyboards_connected.size());
  EXPECT_EQ(4u, fake_observer.keyboards_connected[0]->id);

  ui::DeviceEvent remove_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                        ui::DeviceEvent::ActionType::REMOVE,
                                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_keyboard_event);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.keyboards_disconnected.size());
  EXPECT_EQ(4u, fake_observer.keyboards_disconnected[0]);
}

TEST_F(InputDataProviderTest, ObserveConnectedDevices_TouchDevices) {
  FakeConnectedDevicesObserver fake_observer;
  provider_->ObserveConnectedDevices(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ui::DeviceEvent add_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                  ui::DeviceEvent::ActionType::ADD,
                                  base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_touch_event);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.touch_devices_connected.size());
  EXPECT_EQ(1u, fake_observer.touch_devices_connected[0]->id);

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.touch_devices_disconnected.size());
  EXPECT_EQ(1u, fake_observer.touch_devices_disconnected[0]);
}

TEST_F(InputDataProviderTest, ChangeDeviceDoesNotCrash) {
  ui::DeviceEvent add_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::ADD,
                                   base::FilePath("/dev/input/event1"));
  ui::DeviceEvent change_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                      ui::DeviceEvent::ActionType::CHANGE,
                                      base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_device_event);
  task_environment_.RunUntilIdle();
  provider_->OnDeviceEvent(change_device_event);
  task_environment_.RunUntilIdle();
}

TEST_F(InputDataProviderTest, BadDeviceDoesNotCrash) {
  // Try a device that specifically fails to be processed
  ui::DeviceEvent add_bad_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                       ui::DeviceEvent::ActionType::ADD,
                                       base::FilePath("/dev/input/event99"));
  provider_->OnDeviceEvent(add_bad_device_event);
  task_environment_.RunUntilIdle();
}

TEST_F(InputDataProviderTest, SillyDeviceDoesNotCrash) {
  // Try a device that has data, but has a non-parseable name.
  ui::DeviceEvent add_silly_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                         ui::DeviceEvent::ActionType::ADD,
                                         base::FilePath(kSillyDeviceName));
  provider_->OnDeviceEvent(add_silly_device_event);
  task_environment_.RunUntilIdle();
}

TEST_F(InputDataProviderTest, GetKeyboardVisualLayout_AmericanEnglish) {
  statistics_provider_.SetMachineStatistic(chromeos::system::kKeyboardLayoutKey,
                                           "xkb:us::eng,m17n:ar,t13n:ar");

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  task_environment_.RunUntilIdle();

  base::test::TestFuture<base::flat_map<uint32_t, mojom::KeyGlyphSetPtr>>
      future;
  provider_->GetKeyboardVisualLayout(6, future.GetCallback());
  const auto& layout = future.Get<0>();

  ASSERT_FALSE(layout.at(KEY_Q).is_null());
  EXPECT_EQ("q", layout.at(KEY_Q)->main_glyph);
  EXPECT_FALSE(layout.at(KEY_Q)->shift_glyph.has_value());

  ASSERT_FALSE(layout.at(KEY_3).is_null());
  EXPECT_EQ("3", layout.at(KEY_3)->main_glyph);
  EXPECT_EQ("#", layout.at(KEY_3)->shift_glyph);

  // Check all of the essential keys (at least on US QWERTY) have
  // glyphs.
  for (auto const& entry : layout) {
    EXPECT_FALSE(entry.second.is_null())
        << "No glyphs for evdev code " << entry.first;
  }
}

TEST_F(InputDataProviderTest, GetKeyboardVisualLayout_FrenchFrench) {
  statistics_provider_.SetMachineStatistic(chromeos::system::kKeyboardLayoutKey,
                                           "xkb:fr::fra");

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  task_environment_.RunUntilIdle();

  base::test::TestFuture<base::flat_map<uint32_t, mojom::KeyGlyphSetPtr>>
      future;
  provider_->GetKeyboardVisualLayout(6, future.GetCallback());
  const auto& layout = future.Get<0>();

  ASSERT_FALSE(layout.at(KEY_Q).is_null());
  EXPECT_EQ("a", layout.at(KEY_Q)->main_glyph);
  EXPECT_FALSE(layout.at(KEY_Q)->shift_glyph.has_value());

  ASSERT_FALSE(layout.at(KEY_3).is_null());
  EXPECT_EQ("\"", layout.at(KEY_3)->main_glyph);
  EXPECT_EQ("3", layout.at(KEY_3)->shift_glyph);

  // Check all of the essential keys have glyphs.
  for (auto const& entry : layout) {
    EXPECT_FALSE(entry.second.is_null())
        << "No glyphs for evdev code " << entry.first;
  }
}

TEST_F(InputDataProviderTest, GetKeyboardMechanicalLayout_Unknown1) {
  statistics_provider_.ClearMachineStatistic(
      chromeos::system::kKeyboardMechanicalLayoutKey);

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  task_environment_.RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();

    ASSERT_EQ(1ul, keyboards.size());

    const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
    EXPECT_EQ(6u, builtin_keyboard->id);
    EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
              builtin_keyboard->physical_layout);
    EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
              builtin_keyboard->mechanical_layout);
    EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
              builtin_keyboard->number_pad_present);
  }
}

TEST_F(InputDataProviderTest, GetKeyboardMechanicalLayout_Unknown2) {
  statistics_provider_.SetMachineStatistic(
      chromeos::system::kKeyboardMechanicalLayoutKey, kInvalidMechnicalLayout);
  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  task_environment_.RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();

    ASSERT_EQ(1ul, keyboards.size());

    const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
    EXPECT_EQ(6u, builtin_keyboard->id);
    EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
              builtin_keyboard->physical_layout);
    EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
              builtin_keyboard->mechanical_layout);
    EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
              builtin_keyboard->number_pad_present);
  }
}

TEST_F(InputDataProviderTest, ResetReceiverOnDisconnect) {
  ASSERT_FALSE(provider_->ReceiverIsBound());
  mojo::Remote<mojom::InputDataProvider> remote;
  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(provider_->ReceiverIsBound());

  // Unbind remote to trigger disconnect and disconnect handler.
  remote.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(provider_->ReceiverIsBound());

  // Test intent is to ensure interface can be rebound when application is
  // reloaded using |CTRL + R|.  A disconnect should be signaled in which we
  // will reset the receiver to its unbound state.
  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(provider_->ReceiverIsBound());
}

}  // namespace diagnostics
}  // namespace ash
