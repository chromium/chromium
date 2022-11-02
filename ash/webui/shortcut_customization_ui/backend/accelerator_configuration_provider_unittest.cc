// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <memory>
#include <vector>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {

class FakeAcceleratorsUpdatedObserver
    : public shortcut_customization::mojom::AcceleratorsUpdatedObserver {
 public:
  void OnAcceleratorsUpdated(
      shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
          config) override {
    config_ = std::move(config);
    ++num_times_notified_;
  }

  mojo::PendingRemote<
      shortcut_customization::mojom::AcceleratorsUpdatedObserver>
  pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  int num_times_notified() { return num_times_notified_; }

  shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
  config() {
    return mojo::Clone(config_);
  }

 private:
  mojo::Receiver<shortcut_customization::mojom::AcceleratorsUpdatedObserver>
      receiver_{this};
  shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
      config_;
  int num_times_notified_ = 0;
};

bool CompareAccelerators(const ash::AcceleratorData& expected_data,
                         const ash::AcceleratorInfo& actual_info) {
  ui::Accelerator expected_accel(expected_data.keycode,
                                 expected_data.modifiers);
  ash::AcceleratorInfo expected_info(
      actual_info.type, expected_accel,
      ash::KeycodeToKeyString(expected_data.keycode),
      /*has_key_event=*/true,
      /*locked=*/true);

  const bool type_equals = expected_info.type == actual_info.type;
  const bool accelerator_equals =
      expected_info.accelerator == actual_info.accelerator;
  const bool locked_equals = expected_info.locked == actual_info.locked;
  const bool key_display_equals =
      expected_info.key_display == actual_info.key_display;
  const bool has_key_event_equals =
      expected_info.has_key_event == actual_info.has_key_event;
  return type_equals && accelerator_equals && locked_equals &&
         key_display_equals && has_key_event_equals;
}

void CompareInputDevices(const ui::InputDevice& expected,
                         const ui::InputDevice& actual) {
  EXPECT_EQ(expected.type, actual.type);
  EXPECT_EQ(expected.id, actual.id);
  EXPECT_EQ(expected.name, actual.name);
}

void ExpectAllAcceleratorsEqual(
    const base::span<const ash::AcceleratorData>& expected,
    const std::vector<ash::AcceleratorInfo>& actual) {
  EXPECT_EQ(std::size(expected), actual.size());

  for (const auto& actual_info : actual) {
    bool found_match = false;
    for (const auto& expected_data : expected) {
      found_match = CompareAccelerators(expected_data, actual_info);
      if (found_match) {
        break;
      }
    }
    EXPECT_TRUE(found_match);
  }
}

void ExpectMojomAcceleratorsEqual(
    ash::mojom::AcceleratorSource source,
    const base::span<const ash::AcceleratorData>& expected,
    ash::shortcut_ui::AcceleratorConfigurationProvider::
        AcceleratorConfigurationMap actual_config) {
  // Flatten the map into a vector of `AcceleratorInfo`'s and verify it against
  // the expected data.
  std::vector<AcceleratorInfo> actual_infos;
  for (const auto& iter : actual_config[source]) {
    for (const auto& mojo_info : iter.second) {
      AcceleratorInfo accelerator(mojo_info->type, mojo_info->accelerator,
                                  mojo_info->key_display,
                                  mojo_info->has_key_event, mojo_info->locked);
      actual_infos.push_back(std::move(accelerator));
    }
  }
  ExpectAllAcceleratorsEqual(expected, actual_infos);
}

}  // namespace

namespace shortcut_ui {

class AcceleratorConfigurationProviderTest : public AshTestBase {
 public:
  AcceleratorConfigurationProviderTest() = default;
  ~AcceleratorConfigurationProviderTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    provider_ = std::make_unique<AcceleratorConfigurationProvider>();
  }

  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  std::vector<AcceleratorInfo> GetAshAcceleratorInfos() {
    const std::map<AcceleratorActionId, std::vector<AcceleratorInfo>>&
        ash_accel_map = provider_->id_to_accelerator_info_;
    std::vector<AcceleratorInfo> accelerators;
    for (const auto& iter : ash_accel_map) {
      for (const auto& accel : iter.second) {
        accelerators.push_back(accel);
      }
    }
    return accelerators;
  }

  void GetAshConfigAndExpectEquals(
      const base::span<const ash::AcceleratorData>& expected) {
    provider_->GetAccelerators(base::BindOnce(&ExpectMojomAcceleratorsEqual,
                                              mojom::AcceleratorSource::kAsh,
                                              expected));
  }

  const std::vector<ui::InputDevice>& GetConnectedKeyboards() {
    return provider_->connected_keyboards_;
  }

  void SetUpObserver(FakeAcceleratorsUpdatedObserver* observer) {
    provider_->AddObserver(observer->pending_remote());
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<AcceleratorConfigurationProvider> provider_;
};

TEST_F(AcceleratorConfigurationProviderTest, ResetReceiverOnBindInterface) {
  mojo::Remote<shortcut_customization::mojom::AcceleratorConfigurationProvider>
      remote;
  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  remote.reset();

  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, BrowserIsMutable) {
  // Verify that requesting IsMutable state for Browser accelerators returns
  // false.
  provider_->IsMutable(ash::mojom::AcceleratorSource::kBrowser,
                       base::BindLambdaForTesting([&](bool is_mutable) {
                         // Browser accelerators are not mutable.
                         EXPECT_FALSE(is_mutable);
                       }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, AshIsMutable) {
  // Verify that requesting IsMutable state for Ash accelerators returns true.
  provider_->IsMutable(ash::mojom::AcceleratorSource::kAsh,
                       base::BindLambdaForTesting([&](bool is_mutable) {
                         // Ash accelerators are mutable.
                         EXPECT_TRUE(is_mutable);
                       }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, AshAcceleratorsUpdated) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);
  EXPECT_EQ(0, observer.num_times_notified());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
  };
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();
  ExpectAllAcceleratorsEqual(test_data, GetAshAcceleratorInfos());
  // Notified once after instantiating the accelerators.
  EXPECT_EQ(1, observer.num_times_notified());
  // Verify observer received the correct accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               observer.config());

  // Initialize with a new set of accelerators.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       TOGGLE_MIRROR_MODE},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       SWAP_PRIMARY_DISPLAY},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT},
  };
  Shell::Get()->ash_accelerator_configuration()->Initialize(updated_test_data);
  base::RunLoop().RunUntilIdle();
  ExpectAllAcceleratorsEqual(updated_test_data, GetAshAcceleratorInfos());
  // Observers are notified again after a new set of accelerators are provided.
  EXPECT_EQ(2, observer.num_times_notified());
  // Verify observer has been updated with the new set of accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, GetAcceleratorConfigAsh) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
  };
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();
  ExpectAllAcceleratorsEqual(test_data, GetAshAcceleratorInfos());

  GetAshConfigAndExpectEquals(test_data);
  base::RunLoop().RunUntilIdle();
}

TEST_F(AcceleratorConfigurationProviderTest, ConnectedKeyboardsUpdated) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);

  const std::vector<ui::InputDevice>& devices = GetConnectedKeyboards();
  EXPECT_TRUE(devices.empty());
  EXPECT_EQ(0, observer.num_times_notified());

  ui::InputDevice expected_test_keyboard(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Keyboard");

  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(expected_test_keyboard);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);

  const std::vector<ui::InputDevice>& actual_devices = GetConnectedKeyboards();
  EXPECT_EQ(1u, actual_devices.size());
  CompareInputDevices(expected_test_keyboard, actual_devices[0]);

  base::RunLoop().RunUntilIdle();
  // Adding a new keyboard should trigger the UpdatedAccelerators observer.
  EXPECT_EQ(1, observer.num_times_notified());
}

}  // namespace shortcut_ui

}  // namespace ash
