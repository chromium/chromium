// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace ash {
namespace diagnostics {

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

class TestInputDataProvider : public InputDataProvider {
 public:
  TestInputDataProvider(std::unique_ptr<ui::DeviceManager> device_manager)
      : InputDataProvider(std::move(device_manager)) {}
  explicit TestInputDataProvider(const TestInputDataProvider&) = delete;
  TestInputDataProvider& operator=(const TestInputDataProvider&) = delete;

 protected:
  std::unique_ptr<ui::EventDeviceInfo> GetDeviceInfo(
      base::FilePath path) override {
    std::unique_ptr<ui::EventDeviceInfo> dev_info =
        std::make_unique<ui::EventDeviceInfo>();
    ui::DeviceCapabilities device_caps;
    std::string base_name = path.BaseName().value();
    if (base_name == "event0") {
      device_caps = ui::kLinkKeyboard;
    } else if (base_name == "event1") {
      device_caps = ui::kLinkTouchpad;
    } else if (base_name == "event2") {
      device_caps = ui::kKohakuTouchscreen;
    } else if (base_name == "event3") {
      device_caps = ui::kKohakuStylus;
    } else if (base_name == "event4") {
      device_caps = ui::kHpUsbKeyboard;
    } else if (base_name == "event5") {
      device_caps = ui::kSarienKeyboard;
    } else if (base_name == "event6") {
      device_caps = ui::kEveKeyboard;
    }

    EXPECT_TRUE(ui::CapabilitiesToDeviceInfo(device_caps, dev_info.get()));
    return dev_info;
  }
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
    provider_ = std::make_unique<TestInputDataProvider>(std::move(manager));
  }

  ~InputDataProviderTest() override {
    provider_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  FakeDeviceManager* manager_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<InputDataProvider> provider_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(InputDataProviderTest, GetConnectedDevices_DeviceInfoMapping) {
  base::RunLoop run_loop;
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

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(1ul, keyboards.size());
        // The stylus device should be filtered out, hence only 2 touch devices.
        EXPECT_EQ(2ul, touch_devices.size());

        mojom::KeyboardInfoPtr keyboard = keyboards[0].Clone();
        EXPECT_EQ(0u, keyboard->id);
        EXPECT_EQ(mojom::ConnectionType::kInternal, keyboard->connection_type);
        EXPECT_EQ("AT Translated Set 2 keyboard", keyboard->name);

        mojom::TouchDeviceInfoPtr touchpad = touch_devices[0].Clone();
        EXPECT_EQ(1u, touchpad->id);
        EXPECT_EQ(mojom::ConnectionType::kInternal, touchpad->connection_type);
        EXPECT_EQ(mojom::TouchDeviceType::kPointer, touchpad->type);
        EXPECT_EQ("Atmel maXTouch Touchpad", touchpad->name);

        mojom::TouchDeviceInfoPtr touchscreen = touch_devices[1].Clone();
        EXPECT_EQ(2u, touchscreen->id);
        EXPECT_EQ(mojom::ConnectionType::kInternal,
                  touchscreen->connection_type);
        EXPECT_EQ(mojom::TouchDeviceType::kDirect, touchscreen->type);
        EXPECT_EQ("Atmel maXTouch Touchscreen", touchscreen->name);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(InputDataProviderTest, GetConnectedDevices_AddEventAfterFirstCall) {
  base::RunLoop run_loop;
  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(0ul, keyboards.size());
        EXPECT_EQ(0ul, touch_devices.size());
      }));

  ui::DeviceEvent event(ui::DeviceEvent::DeviceType::INPUT,
                        ui::DeviceEvent::ActionType::ADD,
                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(event);
  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(1ul, keyboards.size());
        mojom::KeyboardInfoPtr keyboard = keyboards[0].Clone();
        EXPECT_EQ(4u, keyboard->id);
        EXPECT_EQ(mojom::ConnectionType::kUsb, keyboard->connection_type);
        EXPECT_EQ("Chicony HP Elite USB Keyboard", keyboard->name);

        EXPECT_EQ(0ul, touch_devices.size());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(InputDataProviderTest, GetConnectedDevices_Remove) {
  base::RunLoop run_loop;
  ui::DeviceEvent add_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                  ui::DeviceEvent::ActionType::ADD,
                                  base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_touch_event);
  ui::DeviceEvent add_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                ui::DeviceEvent::ActionType::ADD,
                                base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_kbd_event);
  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(1ul, keyboards.size());
        EXPECT_EQ(4u, keyboards[0]->id);

        EXPECT_EQ(1ul, touch_devices.size());
        EXPECT_EQ(1u, touch_devices[0]->id);
      }));

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_kbd_event);
  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(0ul, keyboards.size());
        EXPECT_EQ(0ul, touch_devices.size());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(InputDataProviderTest, KeyboardPhysicalLayoutDetection) {
  base::RunLoop run_loop;
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
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  provider_->OnDeviceEvent(event2);

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(3ul, keyboards.size());

        mojom::KeyboardInfoPtr builtin_keyboard = keyboards[0].Clone();
        EXPECT_EQ(0u, builtin_keyboard->id);
        EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
                  builtin_keyboard->physical_layout);
        EXPECT_EQ(mojom::MechanicalLayout::kIso,
                  builtin_keyboard->mechanical_layout);

        mojom::KeyboardInfoPtr external_keyboard = keyboards[1].Clone();
        EXPECT_EQ(4u, external_keyboard->id);
        EXPECT_EQ(mojom::PhysicalLayout::kUnknown,
                  external_keyboard->physical_layout);
        EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
                  external_keyboard->mechanical_layout);

        mojom::KeyboardInfoPtr dell_internal_keyboard = keyboards[2].Clone();
        EXPECT_EQ(5u, dell_internal_keyboard->id);
        EXPECT_EQ(mojom::PhysicalLayout::kChromeOSDellEnterprise,
                  dell_internal_keyboard->physical_layout);
        EXPECT_EQ(mojom::MechanicalLayout::kIso,
                  dell_internal_keyboard->mechanical_layout);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(InputDataProviderTest, KeyboardAssistantKeyDetection) {
  base::RunLoop run_loop;
  ui::DeviceEvent link_event(ui::DeviceEvent::DeviceType::INPUT,
                             ui::DeviceEvent::ActionType::ADD,
                             base::FilePath("/dev/input/event0"));
  ui::DeviceEvent eve_event(ui::DeviceEvent::DeviceType::INPUT,
                            ui::DeviceEvent::ActionType::ADD,
                            base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(link_event);
  provider_->OnDeviceEvent(eve_event);

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(2ul, keyboards.size());

        mojom::KeyboardInfoPtr link_keyboard = keyboards[0].Clone();
        EXPECT_EQ(0u, link_keyboard->id);
        EXPECT_FALSE(link_keyboard->has_assistant_key);
        mojom::KeyboardInfoPtr eve_keyboard = keyboards[1].Clone();
        EXPECT_EQ(6u, eve_keyboard->id);
        EXPECT_TRUE(eve_keyboard->has_assistant_key);
      }));
}

TEST_F(InputDataProviderTest, ObserveConnectedDevices_Keyboards) {
  FakeConnectedDevicesObserver fake_observer;
  provider_->ObserveConnectedDevices(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_keyboard_event);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1ul, fake_observer.keyboards_connected.size());
  EXPECT_EQ(4u, fake_observer.keyboards_connected[0]->id);

  ui::DeviceEvent remove_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                        ui::DeviceEvent::ActionType::REMOVE,
                                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_keyboard_event);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1ul, fake_observer.keyboards_disconnected.size());
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
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1ul, fake_observer.touch_devices_connected.size());
  EXPECT_EQ(1u, fake_observer.touch_devices_connected[0]->id);

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1ul, fake_observer.touch_devices_disconnected.size());
  EXPECT_EQ(1u, fake_observer.touch_devices_disconnected[0]);
}

}  // namespace diagnostics
}  // namespace ash
