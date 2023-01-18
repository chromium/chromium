// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/public/mojom/accelerator_keys.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
using NonConfigurableActionToParts =
    const std::map<NonConfigurableActions,
                   const std::vector<mojom::TextAcceleratorPartPtr&>>;

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

  void clear_num_times_notified() { num_times_notified_ = 0; }

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

bool AreAcceleratorsEqual(const ui::Accelerator& expected_accelerator,
                          const mojom::AcceleratorInfoPtr& actual_info) {
  const bool accelerator_equals =
      expected_accelerator ==
      actual_info->layout_properties->get_standard_accelerator()->accelerator;
  const bool key_display_equals =
      shortcut_ui::GetKeyDisplay(expected_accelerator.key_code()) ==
      actual_info->layout_properties->get_standard_accelerator()->key_display;
  return accelerator_equals && key_display_equals;
}

bool CompareAccelerators(const ash::AcceleratorData& expected_data,
                         const mojom::AcceleratorInfoPtr& actual_info) {
  ui::Accelerator expected_accelerator(expected_data.keycode,
                                       expected_data.modifiers);
  return AreAcceleratorsEqual(expected_accelerator, actual_info);
}

bool CompareAccelerators(const ui::Accelerator& expected_accelerator,
                         const mojom::AcceleratorInfoPtr& actual_info) {
  return AreAcceleratorsEqual(expected_accelerator, actual_info);
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
        actual_layout_infos) {
  for (const auto& actual : actual_layout_infos) {
    // Iterate through `kAcceleratorLayouts` to find the matching action.
    bool found_match = false;
    for (const auto& expected_layout : kAcceleratorLayouts) {
      if (expected_layout.action_id == actual->action &&
          expected_layout.source == actual->source) {
        EXPECT_EQ(expected_layout.category, actual->category);
        EXPECT_EQ(expected_layout.sub_category, actual->sub_category);
        EXPECT_EQ(expected_layout.layout_style, actual->style);
        EXPECT_EQ(expected_layout.source, actual->source);
        EXPECT_EQ(
            l10n_util::GetStringUTF16(expected_layout.description_string_id),
            actual->description);
        found_match = true;
        break;
      }
    }
    EXPECT_TRUE(found_match);
  }
}

void ValidateTextAccelerators(const TextAcceleratorPart& lhs,
                              const mojom::TextAcceleratorPartPtr& rhs) {
  EXPECT_EQ(lhs.text, rhs->text);
  EXPECT_EQ(lhs.type, rhs->type);
}

std::vector<mojom::TextAcceleratorPartPtr> RemovePlainTextParts(
    const std::vector<mojom::TextAcceleratorPartPtr>& parts) {
  std::vector<mojom::TextAcceleratorPartPtr> res;
  for (const auto& part : parts) {
    if (part->type == mojom::TextAcceleratorPartType::kPlainText) {
      continue;
    }
    res.push_back(mojo::Clone(part));
  }
  return res;
}

}  // namespace

namespace shortcut_ui {

class AcceleratorConfigurationProviderTest : public AshTestBase {
 public:
  AcceleratorConfigurationProviderTest() = default;
  ~AcceleratorConfigurationProviderTest() override = default;

  class TestInputMethodManager : public input_method::MockInputMethodManager {
   public:
    void AddObserver(
        input_method::InputMethodManager::Observer* observer) override {
      observers_.AddObserver(observer);
    }

    void RemoveObserver(
        input_method::InputMethodManager::Observer* observer) override {
      observers_.RemoveObserver(observer);
    }

    // Calls all observers with Observer::InputMethodChanged
    void NotifyInputMethodChanged() {
      for (auto& observer : observers_) {
        observer.InputMethodChanged(
            /*manager=*/this, /*profile=*/nullptr, /*show_message=*/false);
      }
    }

    base::ObserverList<InputMethodManager::Observer>::Unchecked observers_;
  };

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {::features::kImprovedKeyboardShortcuts}, {});
    input_method_manager_ = new TestInputMethodManager();
    input_method::InputMethodManager::Initialize(input_method_manager_);

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    provider_ = std::make_unique<AcceleratorConfigurationProvider>();
    non_configurable_actions_map_ =
        provider_->GetNonConfigurableAcceleratorsForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // `provider_` has a dependency on `input_method_manager_`.
    provider_.reset();
    AshTestBase::TearDown();
    input_method::InputMethodManager::Shutdown();
    input_method_manager_ = nullptr;
  }

 protected:
  const std::vector<ui::InputDevice>& GetConnectedKeyboards() {
    return provider_->connected_keyboards_;
  }

  void SetUpObserver(FakeAcceleratorsUpdatedObserver* observer) {
    provider_->AddObserver(observer->pending_remote());
    base::RunLoop().RunUntilIdle();
  }

  const std::vector<ui::Accelerator>& GetAcceleratorsForAction(int action_id) {
    return non_configurable_actions_map_
        .find(static_cast<ash::NonConfigurableActions>(action_id))
        ->second.accelerators.value();
  }

  const std::vector<TextAcceleratorPart>& GetReplacementsForAction(
      int action_id) {
    return non_configurable_actions_map_
        .find(static_cast<ash::NonConfigurableActions>(action_id))
        ->second.replacements.value();
  }

  bool TextAccelContainsReplacements(int action_id) {
    return non_configurable_actions_map_
        .find(static_cast<ash::NonConfigurableActions>(action_id))
        ->second.replacements.has_value();
  }

  int GetMessageIdForTextAccel(int action_id) {
    return non_configurable_actions_map_
        .find(static_cast<ash::NonConfigurableActions>(action_id))
        ->second.message_id.value();
  }

  std::unique_ptr<AcceleratorConfigurationProvider> provider_;
  NonConfigurableActionsMap non_configurable_actions_map_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Test global singleton. Delete is handled by InputMethodManager::Shutdown().
  base::raw_ptr<TestInputMethodManager> input_method_manager_;
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

TEST_F(AcceleratorConfigurationProviderTest, InitialAccelInitCalls) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);
  EXPECT_EQ(0, observer.num_times_notified());

  Shell::Get()->ash_accelerator_configuration()->Initialize();
  base::RunLoop().RunUntilIdle();

  // Observer is initially notified twice, one for ash accelerators and the
  // other for deprecated accelerators.
  EXPECT_EQ(2, observer.num_times_notified());
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

TEST_F(AcceleratorConfigurationProviderTest, ValidateAllAcceleratorLayouts) {
  // Initialize with all default accelerators.
  Shell::Get()->ash_accelerator_configuration()->Initialize();
  base::RunLoop().RunUntilIdle();

  // Get all default accelerator layout infos and verify that they have the
  // correctly mapped layout details
  provider_->GetAcceleratorLayoutInfos(base::BindLambdaForTesting(
      [&](std::vector<mojom::AcceleratorLayoutInfoPtr> actual_layout_infos) {
        ValidateAcceleratorLayouts(actual_layout_infos);
      }));
}

TEST_F(AcceleratorConfigurationProviderTest, TopRowKeyAcceleratorRemapped) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);
  EXPECT_EQ(0, observer.num_times_notified());

  // Top row keys are not function keys by default.
  EXPECT_FALSE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());

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
      // Fake accelerator data - [search] is part of the original accelerator.
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN,
       TOGGLE_FULLSCREEN},
  };

  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // Notified once after instantiating the accelerators.
  EXPECT_EQ(1, observer.num_times_notified());
  // Verify observer received the correct accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, test_data,
                               observer.config());

  // Enable TopRowKeysAreFKeys.
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSendFunctionKeys, true);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());
  EXPECT_EQ(2, observer.num_times_notified());

  // Initialize the same test_data again, but with
  // TopRowKeysAsFunctionKeysEnabled.
  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  // When TopRowKeysAsFunctionKeys enabled, top row shortcut will become [Fkey]
  // + [search] + [modifier].
  const AcceleratorData expected_test_data[] = {
      // alt + tab -> alt + tab
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      // alt + shift + tab -> alt + shift + tab
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      // search + esc -> search + esc
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
      // shift + zoom -> shift + search + F3
      {/*trigger_on_press=*/true, ui::VKEY_F3,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN, TOGGLE_FULLSCREEN},
      // zoom -> search + F3
      {/*trigger_on_press=*/true, ui::VKEY_F3, ui::EF_COMMAND_DOWN,
       TOGGLE_FULLSCREEN},
      // brightness_up -> search + F6
      {/*trigger_on_press=*/true, ui::VKEY_F6, ui::EF_COMMAND_DOWN,
       BRIGHTNESS_UP},
      // alt + brightness_up -> alt + search + F6
      {/*trigger_on_press=*/true, ui::VKEY_F6,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, KEYBOARD_BRIGHTNESS_UP},
      // When [search] is part of the original accelerator, no remapping is
      // done.
      // search + alt + brightness_up -> search + alt + brightness_up
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, KEYBOARD_BRIGHTNESS_UP},
      // search + zoom -> search + zoom
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN,
       TOGGLE_FULLSCREEN},
  };

  EXPECT_EQ(3, observer.num_times_notified());
  // Verify observer received the top-row-remapped accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh,
                               expected_test_data, observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, SixPackKeyAcceleratorRemapped) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);
  EXPECT_EQ(0, observer.num_times_notified());

  // kImprovedKeyboardShortcuts is enabled.
  EXPECT_TRUE(::features::IsImprovedKeyboardShortcutsEnabled());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      // Below are fake shortcuts, only used for testing.
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_NONE,
       CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_NONE,
       TAKE_WINDOW_SCREENSHOT},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_END, ui::EF_SHIFT_DOWN,
       DISABLE_CAPS_LOCK},
      {/*trigger_on_press=*/true, ui::VKEY_NEXT, ui::EF_ALT_DOWN, NEW_TAB},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_NONE, NEW_TAB},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_ALT_DOWN, NEW_TAB},
      // When [search] is part of the original accelerator.
      {/*trigger_on_press=*/true, ui::VKEY_HOME,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_END,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN, DISABLE_CAPS_LOCK},
      //  Edge case: [Shift] + [Delete].
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_SHIFT_DOWN,
       DESKS_NEW_DESK},
  };

  const AcceleratorData expected_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      // Below are fake shortcuts, only used for testing.
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_NONE,
       CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_NONE,
       TAKE_WINDOW_SCREENSHOT},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_END, ui::EF_SHIFT_DOWN,
       DISABLE_CAPS_LOCK},
      {/*trigger_on_press=*/true, ui::VKEY_NEXT, ui::EF_ALT_DOWN, NEW_TAB},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_NONE, NEW_TAB},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_ALT_DOWN, NEW_TAB},

      // When [search] is part of the original accelerator. No remapping is
      // done. Search+Alt+Home -> Search+Alt+Home.
      {/*trigger_on_press=*/true, ui::VKEY_HOME,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, KEYBOARD_BRIGHTNESS_UP},
      // Search+Shift+End -> Search+Shift+End.
      {/*trigger_on_press=*/true, ui::VKEY_END,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN, DISABLE_CAPS_LOCK},

      // Edge case: [Shift] + [Delete]. It should not remapped to
      // [Shift]+[Search]+[Back](aka, Insert).
      //  Shift+Delete -> Shift+Delete
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_SHIFT_DOWN,
       DESKS_NEW_DESK},

      // Additional six-pack remapped accelerators.
      // Delete -> Search+Backspace
      {/*trigger_on_press=*/true, ui::VKEY_BACK, ui::EF_COMMAND_DOWN,
       CYCLE_BACKWARD_MRU},
      // Home -> Search+Left
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_COMMAND_DOWN,
       TAKE_WINDOW_SCREENSHOT},
      // Alt+Home -> Search+Alt+Left
      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, KEYBOARD_BRIGHTNESS_UP},
      // Shift+End -> Search+Shift+Right
      {/*trigger_on_press=*/true, ui::VKEY_RIGHT,
       ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN, DISABLE_CAPS_LOCK},
      // Alt+Next -> Search+Alt+Down
      {/*trigger_on_press=*/true, ui::VKEY_DOWN,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, NEW_TAB},
      // Insert -> Search+Shift+Backspace
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, NEW_TAB},
      // Alt+Insert -> Search+Shift+Alt+Backspace
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, NEW_TAB},
  };

  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.num_times_notified());
  // Verify observer received the correct remapped accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, expected_data,
                               observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest,
       ReversedSixPackKeyAcceleratorRemapped) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);
  EXPECT_EQ(0, observer.num_times_notified());

  // kImprovedKeyboardShortcuts is enabled.
  EXPECT_TRUE(::features::IsImprovedKeyboardShortcutsEnabled());

  const AcceleratorData test_data[] = {
      // Below are fake shortcuts, only used for testing.
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_ALT_DOWN,
       CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_COMMAND_DOWN, NEW_TAB},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, DISABLE_CAPS_LOCK},

      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
       TAKE_WINDOW_SCREENSHOT},
      {/*trigger_on_press=*/true, ui::VKEY_PRIOR,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, DESKS_NEW_DESK},
      {/*trigger_on_press=*/true, ui::VKEY_RIGHT,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
       TOGGLE_FULLSCREEN},
      {/*trigger_on_press=*/true, ui::VKEY_DOWN,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_DOWN},

      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       SHOW_TASK_MANAGER},
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, BRIGHTNESS_UP},
  };

  const AcceleratorData expected_data[] = {
      // When [Search] is not part of original accelerator, no remapping is
      // done. [Left]+[Alt]>[Left]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_ALT_DOWN,
       CYCLE_BACKWARD_MRU},
      // When [Search] is the only modifier, no remapping is done.
      // [Left]+[Search]->[Left]+[Search].
      {/*trigger_on_press=*/true, ui::VKEY_LEFT, ui::EF_COMMAND_DOWN, NEW_TAB},
      // When key code is not reversed six pack key, no remapping is done.
      // [Tab]+[Search]+[Alt]->[Tab]+[Search]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, DISABLE_CAPS_LOCK},

      // [Left]+[Search]+[Alt]->[Home]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_HOME, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_UP},
      // [Left]+[Search]+[Shift]+[Alt]->[Home]+[Shift]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_LEFT,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       TAKE_WINDOW_SCREENSHOT},
      {/*trigger_on_press=*/true, ui::VKEY_HOME,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, TAKE_WINDOW_SCREENSHOT},
      // [Prior]+[Search]+[Alt]->[Up]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_PRIOR,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, DESKS_NEW_DESK},
      {/*trigger_on_press=*/true, ui::VKEY_UP, ui::EF_ALT_DOWN, DESKS_NEW_DESK},
      // [Right]+[Search]+[Shift]+[Alt]->[End]+[Shift]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_RIGHT,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       TOGGLE_FULLSCREEN},
      {/*trigger_on_press=*/true, ui::VKEY_END,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, TOGGLE_FULLSCREEN},
      // [Down]+[Search]+[Alt]->[Next]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_DOWN,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_DOWN},
      {/*trigger_on_press=*/true, ui::VKEY_NEXT, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_DOWN},

      // [Back]+[Search]+[Alt]->[Delete]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_DELETE, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      // [Back]+[Search]+[Shift]+[Alt]->[Insert]+[Alt].
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       SHOW_TASK_MANAGER},
      {/*trigger_on_press=*/true, ui::VKEY_INSERT, ui::EF_ALT_DOWN,
       SHOW_TASK_MANAGER},

      // If the accelerator is just the reverse of [Insert], no remapping is
      // done. [Back]+[Search]+[Shift] -> [Back]+[Search]+[Shift].
      {/*trigger_on_press=*/true, ui::VKEY_BACK,
       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, BRIGHTNESS_UP},
  };

  Shell::Get()->ash_accelerator_configuration()->Initialize(test_data);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.num_times_notified());
  // Verify observer received the correct remapped accelerators.
  ExpectMojomAcceleratorsEqual(mojom::AcceleratorSource::kAsh, expected_data,
                               observer.config());
}

TEST_F(AcceleratorConfigurationProviderTest, InputMethodChanged) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);
  EXPECT_EQ(0, observer.num_times_notified());
  Shell::Get()->ash_accelerator_configuration()->Initialize();
  base::RunLoop().RunUntilIdle();
  // Clear extraneous observer calls.
  observer.clear_num_times_notified();
  EXPECT_EQ(0, observer.num_times_notified());

  // Change input method, expect observer to be called.
  input_method_manager_->NotifyInputMethodChanged();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.num_times_notified());
}

TEST_F(AcceleratorConfigurationProviderTest, TestGetKeyDisplay) {
  EXPECT_EQ(u"c", GetKeyDisplay(ui::VKEY_C));
  EXPECT_EQ(u"MicrophoneMuteToggle",
            GetKeyDisplay(ui::VKEY_MICROPHONE_MUTE_TOGGLE));
  EXPECT_EQ(u"ToggleWifi", GetKeyDisplay(ui::VKEY_WLAN));
  EXPECT_EQ(u"tab", GetKeyDisplay(ui::VKEY_TAB));
  EXPECT_EQ(u"esc", GetKeyDisplay(ui::VKEY_ESCAPE));
  EXPECT_EQ(u"backspace", GetKeyDisplay(ui::VKEY_BACK));
  EXPECT_EQ(u"enter", GetKeyDisplay(ui::VKEY_RETURN));
  EXPECT_EQ(u"space", GetKeyDisplay(ui::VKEY_SPACE));
}

TEST_F(AcceleratorConfigurationProviderTest, NonConfigurableActions) {
  FakeAcceleratorsUpdatedObserver observer;
  SetUpObserver(&observer);
  base::RunLoop().RunUntilIdle();
  // Reinitialize the non-configurable accelerators to trigger the observer.
  provider_->InitializeNonConfigurableAccelerators(
      non_configurable_actions_map_);
  base::RunLoop().RunUntilIdle();
  auto config = observer.config();
  for (const auto& [id, accel_infos] :
       config[mojom::AcceleratorSource::kAmbient]) {
    for (const auto& info : accel_infos) {
      if (info->layout_properties->is_standard_accelerator()) {
        bool found_match = false;
        for (const auto& expected_data : GetAcceleratorsForAction(id)) {
          found_match = CompareAccelerators(expected_data, mojo::Clone(info));
          if (found_match) {
            break;
          }
        }
        // Matching Accelerator was found.
        EXPECT_TRUE(found_match);
      } else {
        const auto& text_accel =
            info->layout_properties->get_text_accelerator()->parts;
        if (!TextAccelContainsReplacements(id)) {
          // Ambient accelerators that contain no replacements e.g., Drag the
          // link to the tab's address bar
          EXPECT_EQ(text_accel[0]->text,
                    l10n_util::GetStringUTF16(GetMessageIdForTextAccel(id)));
          continue;
        }
        // We're only concerned with validating the replacements
        // (keys/modifiers). Validating the plain text parts is handled by the
        // paramaterized tests below.
        const auto& text_accel_parts = RemovePlainTextParts(text_accel);
        const auto& replacement_parts = GetReplacementsForAction(id);
        for (size_t i = 0; i < replacement_parts.size(); i++) {
          ValidateTextAccelerators(replacement_parts[i], text_accel_parts[i]);
        }
      }
    }
  }
}

using FlagsKeyboardCodesVariant =
    std::variant<ui::EventFlags, ui::KeyboardCode, TextAcceleratorDelimiter>;
using FlagsKeyboardCodeStringVariant = std::variant<ui::EventFlags,
                                                    ui::KeyboardCode,
                                                    std::u16string,
                                                    TextAcceleratorDelimiter>;

class TextAcceleratorParsingTest
    : public AcceleratorConfigurationProviderTest,
      public testing::WithParamInterface<
          std::tuple<std::u16string,
                     std::vector<FlagsKeyboardCodesVariant>,
                     std::vector<FlagsKeyboardCodeStringVariant>>> {
 public:
  void SetUp() override {
    AcceleratorConfigurationProviderTest::SetUp();
    std::vector<FlagsKeyboardCodesVariant> replacements_parts;
    std::vector<FlagsKeyboardCodeStringVariant> variants;
    std::tie(replacement_string_, replacements_parts, variants) = GetParam();

    for (const auto& r : replacements_parts) {
      if (std::holds_alternative<ui::KeyboardCode>(r)) {
        replacements_.emplace_back(std::get<ui::KeyboardCode>(r));
      } else if (std::holds_alternative<ui::EventFlags>(r)) {
        replacements_.emplace_back(std::get<ui::EventFlags>(r));
      } else {
        replacements_.emplace_back(std::get<TextAcceleratorDelimiter>(r));
      }
    }

    for (const auto& v : variants) {
      if (std::holds_alternative<std::u16string>(v)) {
        expected_parts_.emplace_back(std::get<std::u16string>(v));
      } else if (std::holds_alternative<ui::KeyboardCode>(v)) {
        expected_parts_.emplace_back(std::get<ui::KeyboardCode>(v));
      } else if (std::holds_alternative<ui::EventFlags>(v)) {
        expected_parts_.emplace_back(std::get<ui::EventFlags>(v));
      } else {
        expected_parts_.emplace_back(std::get<TextAcceleratorDelimiter>(v));
      }
    }
  }

 protected:
  std::u16string replacement_string_;
  std::vector<TextAcceleratorPart> replacements_;
  std::vector<TextAcceleratorPart> expected_parts_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    TextAcceleratorParsingTest,
    // The tuple contains an example replacement string, the replacements to
    // use in the string (keys, modifiers, plain text), and the result we
    // expect to receive given these inputs.
    testing::ValuesIn(
        std::vector<std::tuple<std::u16string,
                               std::vector<FlagsKeyboardCodesVariant>,
                               std::vector<FlagsKeyboardCodeStringVariant>>>{
            {
                u"$1 $2 $3 through $4",
                {ui::EF_CONTROL_DOWN, TextAcceleratorDelimiter::kPlusSign,
                 ui::VKEY_1, ui::VKEY_8},
                {ui::EF_CONTROL_DOWN, u" ", TextAcceleratorDelimiter::kPlusSign,
                 u" ", ui::VKEY_1, u" through ", ui::VKEY_8},
            },
            {
                u"Press $1 and $2",
                {ui::EF_CONTROL_DOWN, ui::VKEY_C},
                {u"Press ", ui::EF_CONTROL_DOWN, u" and ", ui::VKEY_C},
            },
            {
                u"Press $1 $2 $3",
                {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C},
                {u"Press ", ui::VKEY_A, u" ", ui::VKEY_B, u" ", ui::VKEY_C},
            },
            {
                u"$1 $2 $3 Press",
                {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C},
                {ui::VKEY_A, u" ", ui::VKEY_B, u" ", ui::VKEY_C, u" Press"},
            },
            {
                u"$1$2$3",
                {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C},
                {ui::VKEY_A, ui::VKEY_B, ui::VKEY_C},
            },
            {
                u"$1 and $2",
                {ui::VKEY_A, ui::VKEY_B},
                {ui::VKEY_A, u" and ", ui::VKEY_B},
            },
            {
                u"A $1 $2 D",
                {ui::VKEY_B, ui::VKEY_C},
                {u"A ", ui::VKEY_B, u" ", ui::VKEY_C, u" D"},
            },
            {
                u"$1",
                {ui::VKEY_B},
                {ui::VKEY_B},
            },
            {
                u"$1 ",
                {ui::VKEY_B},
                {ui::VKEY_B, u" "},
            },
            {
                u" $1",
                {ui::VKEY_B},
                {u" ", ui::VKEY_B},
            },
            {
                u"$1",
                {TextAcceleratorDelimiter::kPlusSign},
                {TextAcceleratorDelimiter::kPlusSign},
            },
            {
                u"$1 ",
                {TextAcceleratorDelimiter::kPlusSign},
                {TextAcceleratorDelimiter::kPlusSign, u" "},
            },
            {
                u" $1",
                {TextAcceleratorDelimiter::kPlusSign},
                {u" ", TextAcceleratorDelimiter::kPlusSign},
            },
            {
                u"Drag the link to a blank area on the tab strip",
                {},
                {u"Drag the link to a blank area on the tab strip"},
            },
            {
                u"$1a$2$3bc",
                {ui::EF_SHIFT_DOWN, ui::VKEY_B, ui::VKEY_C},
                {ui::EF_SHIFT_DOWN, u"a", ui::VKEY_B, ui::VKEY_C, u"bc"},
            }}));

TEST_P(TextAcceleratorParsingTest, TextAcceleratorParsing) {
  auto& bundle = ui::ResourceBundle::GetSharedInstance();
  int FAKE_RESOURCE_ID = 1;
  bundle.OverrideLocaleStringResource(FAKE_RESOURCE_ID, replacement_string_);
  const auto text_accelerator = provider_->CreateTextAcceleratorProperties(
      {FAKE_RESOURCE_ID, replacements_});
  EXPECT_EQ(expected_parts_.size(), text_accelerator->parts.size());
  for (size_t i = 0; i < expected_parts_.size(); i++) {
    ValidateTextAccelerators(expected_parts_[i], text_accelerator->parts[i]);
  }
}

class GetPlainTextPartsTest : public AcceleratorConfigurationProviderTest,
                              public testing::WithParamInterface<
                                  std::tuple<std::u16string,
                                             std::vector<size_t>,
                                             std::vector<std::u16string>>> {
 public:
  void SetUp() override {
    AcceleratorConfigurationProviderTest::SetUp();
    std::tie(input_, offsets_, expected_output_) = GetParam();
  }

 protected:
  std::u16string input_;
  std::vector<size_t> offsets_;
  std::vector<std::u16string> expected_output_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    GetPlainTextPartsTest,
    // The tuple contains an example replacement string with the placeholders
    // removed, the expected list of offsets which must be sorted and contains
    // the start points of our replacements, and the plain text parts we expect
    // receive given these inputs.
    testing::ValuesIn(std::vector<std::tuple<std::u16string,
                                             std::vector<size_t>,
                                             std::vector<std::u16string>>>{

        {u"abc", {0, 1, 1}, {u"a", u"bc"}},
        {u"abc", {0, 1, 2}, {u"a", u"b", u"c"}},
        {u"a b", {0, 1}, {u"a", u" b"}},
        {u"a b", {0, 2}, {u"a ", u"b"}},
        {u"Press  and ", {6, 11}, {u"Press ", u" and "}},
        {u"", {0}, {}},
        {u"No replacements", {}, {u"No replacements"}},
        {u"a and bc", {0, 6}, {u"a and ", u"bc"}},

    }));

TEST_P(GetPlainTextPartsTest, GetPlainTextParts) {
  const auto parts = SplitStringOnOffsets(input_, offsets_);
  EXPECT_EQ(expected_output_, parts);
}
}  // namespace shortcut_ui

}  // namespace ash
