// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"

#include <vector>

#include "base/command_line.h"
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

class FakeInputDeviceInfoHelper : public InputDeviceInfoHelper {
 public:
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
    } else if (base_name == "event7") {
      // Simulate a device that couldn't be opened or have its info determined
      // for whatever reason.
      return nullptr;
    }

    EXPECT_TRUE(ui::CapabilitiesToDeviceInfo(device_caps, dev_info.get()));
    return dev_info;
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
    provider_ = std::make_unique<TestInputDataProvider>(std::move(manager));
  }

  ~InputDataProviderTest() override {
    provider_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeDeviceManager* manager_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<InputDataProvider> provider_;
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
  task_environment_.RunUntilIdle();

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        ASSERT_EQ(1ul, keyboards.size());
        // The stylus device should be filtered out, hence only 2 touch devices.
        ASSERT_EQ(2ul, touch_devices.size());

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
        ASSERT_EQ(0ul, keyboards.size());
        ASSERT_EQ(0ul, touch_devices.size());
      }));

  ui::DeviceEvent event(ui::DeviceEvent::DeviceType::INPUT,
                        ui::DeviceEvent::ActionType::ADD,
                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(event);
  task_environment_.RunUntilIdle();

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        ASSERT_EQ(1ul, keyboards.size());
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
  task_environment_.RunUntilIdle();

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        ASSERT_EQ(1ul, keyboards.size());
        EXPECT_EQ(4u, keyboards[0]->id);

        ASSERT_EQ(1ul, touch_devices.size());
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
  task_environment_.RunUntilIdle();

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
  task_environment_.RunUntilIdle();

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        ASSERT_EQ(3ul, keyboards.size());

        mojom::KeyboardInfoPtr builtin_keyboard = keyboards[0].Clone();
        EXPECT_EQ(0u, builtin_keyboard->id);
        EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
                  builtin_keyboard->physical_layout);
        EXPECT_EQ(mojom::MechanicalLayout::kIso,
                  builtin_keyboard->mechanical_layout);
        EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
                  builtin_keyboard->number_pad_present);

        mojom::KeyboardInfoPtr external_keyboard = keyboards[1].Clone();
        EXPECT_EQ(4u, external_keyboard->id);
        EXPECT_EQ(mojom::PhysicalLayout::kUnknown,
                  external_keyboard->physical_layout);
        EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
                  external_keyboard->mechanical_layout);
        EXPECT_EQ(mojom::NumberPadPresence::kUnknown,
                  external_keyboard->number_pad_present);

        mojom::KeyboardInfoPtr dell_internal_keyboard = keyboards[2].Clone();
        EXPECT_EQ(5u, dell_internal_keyboard->id);
        EXPECT_EQ(mojom::PhysicalLayout::kChromeOSDellEnterprise,
                  dell_internal_keyboard->physical_layout);
        EXPECT_EQ(mojom::MechanicalLayout::kIso,
                  dell_internal_keyboard->mechanical_layout);
        EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
                  dell_internal_keyboard->number_pad_present);

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
  task_environment_.RunUntilIdle();

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        ASSERT_EQ(2ul, keyboards.size());

        mojom::KeyboardInfoPtr link_keyboard = keyboards[0].Clone();
        EXPECT_EQ(0u, link_keyboard->id);
        EXPECT_FALSE(link_keyboard->has_assistant_key);
        mojom::KeyboardInfoPtr eve_keyboard = keyboards[1].Clone();
        EXPECT_EQ(6u, eve_keyboard->id);
        EXPECT_TRUE(eve_keyboard->has_assistant_key);
      }));
}

TEST_F(InputDataProviderTest, KeyboardNumberPadDetection) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--has-number-pad"});
  base::RunLoop run_loop;
  ui::DeviceEvent link_event(ui::DeviceEvent::DeviceType::INPUT,
                             ui::DeviceEvent::ActionType::ADD,
                             base::FilePath("/dev/input/event0"));
  provider_->OnDeviceEvent(link_event);
  task_environment_.RunUntilIdle();

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        ASSERT_EQ(1ul, keyboards.size());

        mojom::KeyboardInfoPtr builtin_keyboard = keyboards[0].Clone();
        EXPECT_EQ(0u, builtin_keyboard->id);
        EXPECT_EQ(mojom::NumberPadPresence::kPresent,
                  builtin_keyboard->number_pad_present);

        run_loop.Quit();
      }));
  run_loop.Run();
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

TEST_F(InputDataProviderTest, BadDeviceDoesntCrash) {
  ui::DeviceEvent add_bad_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                       ui::DeviceEvent::ActionType::ADD,
                                       base::FilePath("/dev/input/event7"));
  provider_->OnDeviceEvent(add_bad_device_event);
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

  base::RunLoop run_loop;
  provider_->GetKeyboardVisualLayout(
      6, base::BindLambdaForTesting(
             [&](base::flat_map<uint32_t, mojom::KeyGlyphSetPtr> layout) {
               ASSERT_FALSE(layout[KEY_Q].is_null());
               EXPECT_EQ("q", layout[KEY_Q]->main_glyph);
               EXPECT_FALSE(layout[KEY_Q]->shift_glyph.has_value());

               ASSERT_FALSE(layout[KEY_3].is_null());
               EXPECT_EQ("3", layout[KEY_3]->main_glyph);
               EXPECT_EQ("#", layout[KEY_3]->shift_glyph);

               // Check all of the essential keys (at least on US QWERTY) have
               // glyphs.
               for (auto const& entry : layout) {
                 EXPECT_FALSE(entry.second.is_null())
                     << "No glyphs for evdev code " << entry.first;
               }

               run_loop.Quit();
             }));
  run_loop.Run();
}

TEST_F(InputDataProviderTest, GetKeyboardVisualLayout_FrenchFrench) {
  statistics_provider_.SetMachineStatistic(chromeos::system::kKeyboardLayoutKey,
                                           "xkb:fr::fra");

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  provider_->GetKeyboardVisualLayout(
      6, base::BindLambdaForTesting(
             [&](base::flat_map<uint32_t, mojom::KeyGlyphSetPtr> layout) {
               ASSERT_FALSE(layout[KEY_Q].is_null());
               EXPECT_EQ("a", layout[KEY_Q]->main_glyph);
               EXPECT_FALSE(layout[KEY_Q]->shift_glyph.has_value());

               ASSERT_FALSE(layout[KEY_3].is_null());
               EXPECT_EQ("\"", layout[KEY_3]->main_glyph);
               EXPECT_EQ("3", layout[KEY_3]->shift_glyph);

               // Check all of the essential keys have glyphs.
               for (auto const& entry : layout) {
                 EXPECT_FALSE(entry.second.is_null())
                     << "No glyphs for evdev code " << entry.first;
               }

               run_loop.Quit();
             }));
  run_loop.Run();
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
