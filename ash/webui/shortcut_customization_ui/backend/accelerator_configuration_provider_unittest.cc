// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <memory>
#include <vector>

#include "ash/accelerators/accelerator_layout_table.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/session/session_controller_impl.h"
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

void SetTopRowKeysAsFunctionKeysEnabled(bool enabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kSendFunctionKeys, enabled);
  prefs->CommitPendingWrite();
}

bool TopRowKeysAreFunctionKeys() {
  const PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!pref_service)
    return false;
  return pref_service->GetBoolean(prefs::kSendFunctionKeys);
}

bool CompareAccelerators(const ash::AcceleratorData& expected_data,
                         const mojom::AcceleratorInfoPtr& actual_info) {
  ui::Accelerator expected_accelerator(expected_data.keycode,
                                       expected_data.modifiers);

  const bool accelerator_equals =
      expected_accelerator == actual_info->accelerator;
  const bool key_display_equals =
      KeycodeToKeyString(expected_accelerator.key_code()) ==
      actual_info->key_display;
  return accelerator_equals && key_display_equals;
}

void CompareInputDevices(const ui::InputDevice& expected,
                         const ui::InputDevice& actual) {
  EXPECT_EQ(expected.type, actual.type);
  EXPECT_EQ(expected.id, actual.id);
  EXPECT_EQ(expected.name, actual.name);
}

void ExpectMojomAcceleratorsEqual(
    ash::mojom::AcceleratorSource source,
    const base::span<const ash::AcceleratorData>& expected,
    ash::shortcut_ui::AcceleratorConfigurationProvider::
        AcceleratorConfigurationMap actual_config) {
  for (const auto& [action_id, actual_accels] : actual_config[source]) {
    for (const auto& actual_info : actual_accels) {
      bool found_match = false;
      for (const auto& expected_data : expected) {
        found_match =
            CompareAccelerators(expected_data, mojo::Clone(actual_info));
        if (found_match) {
          break;
        }
      }
      EXPECT_TRUE(found_match);
    }
  }
}

// Validates that the passed in layout infos have matching accelerator layouts
// in `kAcceleratorLayouts`. If this throws an expectation error it means that
// the there is a inconsistency between the layouts in `kAcceleratorLayouts` and
// the data provided by `AcceleratorConfigurationProvider`.
void ValidateAcceleratorLayouts(
    const std::vector<ash::mojom::AcceleratorLayoutInfoPtr>&
        actual_layout_infos,
    mojom::AcceleratorSource expected_source) {
  for (const auto& actual : actual_layout_infos) {
    EXPECT_TRUE(ash::kAcceleratorLayouts.contains(actual->action));

    const ash::AcceleratorLayoutDetails& expected =
        ash::kAcceleratorLayouts.at(actual->action);

    EXPECT_EQ(expected.category, actual->category);
    EXPECT_EQ(expected.sub_category, actual->sub_category);
    EXPECT_EQ(expected.layout_style, actual->style);
    EXPECT_EQ(expected_source, actual->source);
  }
}

void ValidateLayoutsHaveMatchingStrings(
    const std::vector<ash::mojom::AcceleratorLayoutInfoPtr>&
        actual_layout_infos) {
  for (const auto& layout : actual_layout_infos) {
    // kAcceleratorActionToStringIdMap should contain all actions in
    // AcceleratorAction. Adding a new accelerator must add a new entry to this
    // map.
    EXPECT_TRUE(ash::kAcceleratorActionToStringIdMap.contains(layout->action))
        << "Unknown accelerator action id: " << layout->action;
  }
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
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { AshTestBase::TearDown(); }

 protected:
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
  // Observers are notified again after a new set of accelerators are provided.
  EXPECT_EQ(2, observer.num_times_notified());
  // Verify observer has been updated with the new set of accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               updated_test_data, observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, ConnectedKeyboardsUpdated) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);

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

TEST_F(AcceleratorConfigurationProviderTest,
       ValidateAllLayoutsHaveMatchingStrings) {
  // Initialize with all default accelerators.
  Shell::Get()->ash_accelerator_configuration()->Initialize();
  base::RunLoop().RunUntilIdle();

  // Get all default accelerator layout infos and verify that they correctly
  // mapped the ash::kAcceleratorActionToStringIdMap. This makes sure the map
  // won't be out of date.
  provider_->GetAcceleratorLayoutInfos(base::BindLambdaForTesting(
      [&](std::vector<mojom::AcceleratorLayoutInfoPtr> actual_layout_infos) {
        ValidateLayoutsHaveMatchingStrings(actual_layout_infos);
      }));
}

TEST_F(AcceleratorConfigurationProviderTest, ValidateAllAcceleratorLayouts) {
  // Initialize with all default accelerators.
  Shell::Get()->ash_accelerator_configuration()->Initialize();
  base::RunLoop().RunUntilIdle();

  // Get all default accelerator layout infos and verify that they have the
  // correctly mapped layout details
  provider_->GetAcceleratorLayoutInfos(base::BindLambdaForTesting(
      [&](std::vector<mojom::AcceleratorLayoutInfoPtr> actual_layout_infos) {
        ValidateAcceleratorLayouts(actual_layout_infos,
                                   mojom::AcceleratorSource::kAsh);
      }));
}

TEST_F(AcceleratorConfigurationProviderTest, TopRowKeyAcceleratorRemapped) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);
  EXPECT_EQ(0, observer.num_times_notified());

  // Top row keys are not function keys by default.
  EXPECT_FALSE(TopRowKeysAreFunctionKeys());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_SHIFT_DOWN,
       TOGGLE_FULLSCREEN},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_NONE,
       TOGGLE_FULLSCREEN},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE,
       BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_UP},
  };

  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Notified once after instantiating the accelerators.
  EXPECT_EQ(1, observer.num_times_notified());
  // Verify observer received the correct accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               observer.config());

  // Enable TopRowKeysAsFunctionKeys
  SetTopRowKeysAsFunctionKeysEnabled(true);
  EXPECT_TRUE(TopRowKeysAreFunctionKeys());

  // Initialize the same test_data again, but with
  // TopRowKeysAsFunctionKeysEnabled.
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // When TopRowKeysAsFunctionKeys enabled, top row shortcut will become [Fkey]
  // + [search] + [modifier].
  const AcceleratorData expected_test_data[] = {
      // alt + tab = alt + tab
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      // alt + shift + tab = alt + shift + tab
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      // search + esc = search + esc
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
      // shift + zoom = shift + search + F4
      {/*trigger_on_press=*/true, ui::VKEY_F4,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN, TOGGLE_FULLSCREEN},
      // zoom = search + F4
      {/*trigger_on_press=*/true, ui::VKEY_F4, ui::EF_COMMAND_DOWN,
       TOGGLE_FULLSCREEN},
      // brightness_up = search + F7
      {/*trigger_on_press=*/true, ui::VKEY_F7, ui::EF_COMMAND_DOWN,
       BRIGHTNESS_UP},
      // alt + brightness_up = alt + search + F7
      {/*trigger_on_press=*/true, ui::VKEY_F7,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, KEYBOARD_BRIGHTNESS_UP},
  };

  EXPECT_EQ(2, observer.num_times_notified());
  // Verify observer received the top-row-remapped accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               expected_test_data, observer.config());
}

}  // namespace shortcut_ui

}  // namespace ash
