// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";

constexpr char kKbdTopRowLayoutUnspecified[] = "";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr char kKbdTopRowLayout2Tag[] = "2";
constexpr char kKbdTopRowLayoutWilcoTag[] = "3";
constexpr char kKbdTopRowLayoutDrallionTag[] = "4";

struct AcceleratorAliasConverterTestData {
  ui::Accelerator accelerator_;
  std::optional<ui::Accelerator> expected_accelerators_;
};

struct TopRowAcceleratorAliasConverterTestData {
  // All currently connected keyboards' connection type, e.g.
  // INPUT_DEVICE_INTERNAL.
  std::vector<ui::InputDeviceType> keyboard_connection_type_;
  // All currently connected keyboards' layout types.
  std::vector<std::string> keyboard_layout_types_;
  ui::Accelerator accelerator_;
  std::vector<ui::Accelerator> expected_accelerators_;
};

struct ActionKeyAliasConverterTestData {
  // All currently connected keyboards' connection type, e.g.
  // INPUT_DEVICE_INTERNAL.
  std::vector<ui::InputDeviceType> keyboard_connection_type_;
  // All currently connected keyboards' layout types.
  std::vector<std::string> keyboard_layout_types_;
  std::vector<bool> top_row_are_fkeys_;
  ui::Accelerator accelerator_;
  std::vector<ui::Accelerator> expected_accelerators_;
};

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() = default;

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeKeyboard(const ui::KeyboardDevice& fake_keyboard,
                       const std::string& layout) {
    fake_keyboard_devices_.push_back(fake_keyboard);

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties[kKbdTopRowPropertyName] = layout;
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/std::nullopt,
                             /*devtype=*/std::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));
  }

  void RemoveAllDevices() {
    fake_udev_.Reset();
    fake_keyboard_devices_.clear();
  }

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
};

}  // namespace

class AcceleratorAliasConverterTest : public AshTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kInputDeviceSettingsSplit);
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    scoped_feature_list_.Reset();
  }

  void CheckFKeyAddToSixPackKeyAlias() {
    for (auto entry : ui::kSixPackKeyToFnKeyMap) {
      AcceleratorAliasConverter accelerator_alias_converter_;
      const ui::Accelerator accelerator{entry.first, ui::EF_ALT_DOWN};
      std::vector<ui::Accelerator> accelerator_aliases =
          accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

      EXPECT_EQ(1u, accelerator_aliases.size());
      const ui::Accelerator expected_accelerator{
          entry.second, ui::EF_ALT_DOWN | ui::EF_FUNCTION_DOWN,
          accelerator.key_state()};
      EXPECT_EQ(expected_accelerator, accelerator_aliases[0]);
    }

    AcceleratorAliasConverter accelerator_alias_converter_;
    const ui::Accelerator accelerator{ui::KeyboardCode::VKEY_INSERT,
                                      ui::EF_ALT_DOWN};
    std::vector<ui::Accelerator> accelerator_aliases =
        accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

    EXPECT_EQ(0u, accelerator_aliases.size());
  }

  std::optional<ui::KeyboardCode> GetActionVKey(
      ui::KeyboardDevice& keyboard,
      const ui::Accelerator accelerator) {
    std::optional<ui::TopRowActionKey> action_key =
        Shell::Get()->keyboard_capability()->GetCorrespondingActionKeyForFKey(
            keyboard, accelerator.key_code());

    // Convert the `ui::TopRowActionKey` to the corresponding `ui::KeyboardCode`
    std::optional<ui::KeyboardCode> action_vkey =
        ui::KeyboardCapability::ConvertToKeyboardCode(*action_key);
    return action_vkey;
  }

  void SetTopRowAsFKeysForKeyboard(const ui::InputDevice& keyboard,
                                   bool enabled) {
    if (!features::IsInputDeviceSettingsSplitEnabled()) {
      // Top row keys not fKeys prevents remapping.
      Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
          prefs::kSendFunctionKeys, enabled);
      EXPECT_EQ(
          enabled,
          Shell::Get()->keyboard_controller()->AreTopRowKeysFunctionKeys());
      return;
    }

    auto settings = Shell::Get()
                        ->input_device_settings_controller()
                        ->GetKeyboardSettings(keyboard.id)
                        ->Clone();
    settings->top_row_are_fkeys = enabled;
    Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
        keyboard.id, std::move(settings));
  }

  void SetKeyboardInfo(
      const ui::KeyboardDevice& keyboard,
      ui::KeyboardCapability::DeviceType device_type,
      ui::KeyboardCapability::KeyboardTopRowLayout top_row_layout) {
    ui::KeyboardCapability::KeyboardInfo keyboard_info;
    keyboard_info.device_type = device_type;
    keyboard_info.top_row_layout = top_row_layout;
    keyboard_info.top_row_action_keys = std::vector<ui::TopRowActionKey>(
        std::begin(ui::kLayout1TopRowActionKeys),
        std::end(ui::kLayout1TopRowActionKeys));
    switch (top_row_layout) {
      case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1:
        keyboard_info.top_row_action_keys = std::vector<ui::TopRowActionKey>(
            std::begin(ui::kLayout1TopRowActionKeys),
            std::end(ui::kLayout1TopRowActionKeys));
        break;

      case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2:
        keyboard_info.top_row_action_keys = std::vector<ui::TopRowActionKey>(
            std::begin(ui::kLayout2TopRowActionKeys),
            std::end(ui::kLayout2TopRowActionKeys));
        break;

      case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
      case ui::KeyboardCapability::KeyboardTopRowLayout::
          kKbdTopRowLayoutDrallion:
        keyboard_info.top_row_action_keys = std::vector<ui::TopRowActionKey>(
            std::begin(ui::kLayoutWilcoDrallionTopRowActionKeys),
            std::end(ui::kLayoutWilcoDrallionTopRowActionKeys));
        break;
      case ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
        break;
    }
    Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
        keyboard, std::move(keyboard_info));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AcceleratorAliasConverterTest, UpdateSixPackKeyAliasWithFkey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kModifierSplit);
  auto ignore_modifier_split_secret_key =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag, /*has_assistant_key=*/true,
      /*has_function_key=*/true);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  CheckFKeyAddToSixPackKeyAlias();
}

TEST_F(AcceleratorAliasConverterTest, UpdateTopRowKeysAliasWithFkey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kModifierSplit);
  auto ignore_modifier_split_secret_key =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag, /*has_assistant_key=*/true,
      /*has_function_key=*/true);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;
  const ui::Accelerator accelerator{ui::VKEY_ZOOM, ui::EF_NONE};
  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/true);
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

  EXPECT_EQ(1u, accelerator_aliases.size());
  const ui::Accelerator expected_accelerator{
      ui::VKEY_ZOOM, ui::EF_FUNCTION_DOWN, accelerator.key_state()};
  EXPECT_EQ(expected_accelerator, accelerator_aliases[0]);

  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/false);
  std::vector<ui::Accelerator> updated_accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

  EXPECT_EQ(1u, updated_accelerator_aliases.size());
  const ui::Accelerator updated_expected_accelerator{ui::VKEY_ZOOM, ui::EF_NONE,
                                                     accelerator.key_state()};
  EXPECT_EQ(updated_expected_accelerator, updated_accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, UpdateFunctionAlias) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kModifierSplit);
  auto ignore_modifier_split_secret_key =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag, /*has_assistant_key=*/true,
      /*has_function_key=*/true);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;
  const ui::Accelerator accelerator{ui::VKEY_F1, ui::EF_NONE};
  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/false);
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

  auto action_vkey_1 = GetActionVKey(fake_keyboard, accelerator);
  ASSERT_TRUE(action_vkey_1);
  EXPECT_EQ(1u, accelerator_aliases.size());
  const ui::Accelerator expected_accelerator{
      action_vkey_1.value(), ui::EF_FUNCTION_DOWN, accelerator.key_state()};
  EXPECT_EQ(expected_accelerator, accelerator_aliases[0]);

  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/true);
  std::vector<ui::Accelerator> updated_accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

  auto action_vkey_2 = GetActionVKey(fake_keyboard, accelerator);
  ASSERT_TRUE(action_vkey_2);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(1u, updated_accelerator_aliases.size());
  const ui::Accelerator updated_expected_accelerator{
      action_vkey_2.value(), ui::EF_NONE, accelerator.key_state()};
  EXPECT_EQ(updated_expected_accelerator, updated_accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, CheckTopRowAliasNoAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/false);
  const ui::Accelerator accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, CheckGlobeKeyAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  AcceleratorAliasConverter accelerator_alias_converter_;

  // Globe key is not available in non-wilco/drallion internal devices.
  ui::KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  internal_keyboard.sys_path = base::FilePath("internal_path");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);

  const ui::Accelerator accelerator{ui::VKEY_MODECHANGE, ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(0u, accelerator_aliases.size());

  // Globe key is available in external keyboard.
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);

  // Globe key is available in wilco/drallion keyboards.
  ui::KeyboardDevice wilco_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  wilco_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(wilco_keyboard,
                                          kKbdTopRowLayoutWilcoTag);
  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, CheckCalculatorKeyAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  const ui::Accelerator accelerator{ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(0u, accelerator_aliases.size());

  ui::KeyboardDevice wilco_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayout1Tag);
  wilco_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(wilco_keyboard,
                                          kKbdTopRowLayoutWilcoTag);
  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, CheckBrowserSearchKeyAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  const ui::Accelerator accelerator{ui::VKEY_BROWSER_SEARCH, ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(0u, accelerator_aliases.size());

  ui::KeyboardDevice wilco_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayoutWilcoTag);
  wilco_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(wilco_keyboard,
                                          kKbdTopRowLayoutWilcoTag);
  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, CheckHelpKeyAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  const ui::Accelerator accelerator{ui::VKEY_HELP, ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(0u, accelerator_aliases.size());

  ui::KeyboardDevice wilco_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayoutWilcoTag);
  wilco_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(wilco_keyboard,
                                          kKbdTopRowLayoutWilcoTag);
  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, CheckSettingsKeyAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  const ui::Accelerator accelerator{ui::VKEY_SETTINGS, ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(0u, accelerator_aliases.size());

  ui::KeyboardDevice wilco_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayoutWilcoTag);
  wilco_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(wilco_keyboard,
                                          kKbdTopRowLayoutWilcoTag);
  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, SplitModifierKeyboardCapsLockAlias) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kModifierSplit);
  auto ignore_modifier_split_secret_key =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice split_modifier_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag, /*has_assistant_key=*/true,
      /*has_function_key=*/true);
  split_modifier_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(split_modifier_keyboard,
                                          kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;
  const ui::Accelerator capslock_accelerator{ui::VKEY_CAPITAL, ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(capslock_accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(ui::Accelerator(ui::VKEY_RIGHT_ALT, ui::EF_FUNCTION_DOWN),
            accelerator_aliases[0]);
  const ui::Accelerator capslock_accelerator_2{ui::VKEY_LWIN, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> new_accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(
          capslock_accelerator_2);
  EXPECT_EQ(0u, new_accelerator_aliases.size());
}

TEST_F(AcceleratorAliasConverterTest, ExternalKeyboardCapsLockAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();

  ui::KeyboardDevice external_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayout1Tag);
  SetKeyboardInfo(
      external_keyboard,
      ui::KeyboardCapability::DeviceType::kDeviceExternalUnknown,
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1);
  external_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;
  const ui::Accelerator capslock_accelerator{ui::VKEY_CAPITAL, ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(capslock_accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(capslock_accelerator, accelerator_aliases[0]);
  const ui::Accelerator capslock_accelerator_2{ui::VKEY_LWIN, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> new_accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(
          capslock_accelerator_2);
  EXPECT_EQ(1u, new_accelerator_aliases.size());
  EXPECT_EQ(capslock_accelerator_2, new_accelerator_aliases[0]);
}

TEST_F(AcceleratorAliasConverterTest, CheckCapsLockAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  const ui::Accelerator good_accelerator{ui::VKEY_LWIN, ui::EF_ALT_DOWN};
  const ui::Accelerator bad_accelerator{ui::VKEY_MENU, ui::EF_COMMAND_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(good_accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(good_accelerator, accelerator_aliases[0]);

  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(bad_accelerator);
  EXPECT_EQ(0u, accelerator_aliases.size());
}

TEST_F(AcceleratorAliasConverterTest, CheckRightAltInList) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kModifierSplit);
  auto ignore_modifier_split_secret_key =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag, /*has_assistant_key=*/true,
      /*has_function_key=*/true);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  const ui::Accelerator right_alt_accelerator{ui::VKEY_RIGHT_ALT, ui::EF_NONE};

  AcceleratorAliasConverter accelerator_alias_converter;

  EXPECT_EQ(std::vector<ui::Accelerator>{right_alt_accelerator},
            accelerator_alias_converter.CreateAcceleratorAlias(
                right_alt_accelerator));
}

TEST_F(AcceleratorAliasConverterTest, CheckRightAltNotInList) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kModifierSplit);
  auto ignore_modifier_split_secret_key =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();

  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  const ui::Accelerator right_alt_accelerator{ui::VKEY_RIGHT_ALT, ui::EF_NONE};

  AcceleratorAliasConverter accelerator_alias_converter;
  EXPECT_TRUE(
      accelerator_alias_converter.CreateAcceleratorAlias(right_alt_accelerator)
          .empty());
}

TEST_F(AcceleratorAliasConverterTest, MetaFKeyRewritesSuppressed) {
  // Needs to be in user session to edit input device settings.
  SimulateGuestLogin();

  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayoutUnspecified, /*phys=*/"",
      /*sys_path=*/base::FilePath(),
      /*vendor=*/0x2222, /*product=*/0x2222, /*version=*/0x0);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard,
                                          kKbdTopRowLayoutUnspecified);

  mojom::KeyboardSettings settings;
  settings.six_pack_key_remappings = mojom::SixPackKeyInfo::New();
  settings.top_row_are_fkeys = true;
  settings.suppress_meta_fkey_rewrites = false;
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, settings.Clone());

  AcceleratorAliasConverter accelerator_alias_converter_;

  const ui::Accelerator refresh_accelerator{ui::VKEY_BROWSER_REFRESH,
                                            ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(refresh_accelerator);
  ASSERT_EQ(2u, accelerator_aliases.size());
  EXPECT_EQ(ui::Accelerator(ui::VKEY_F3, ui::EF_COMMAND_DOWN),
            accelerator_aliases[0]);
  EXPECT_EQ(refresh_accelerator, accelerator_aliases[1]);

  settings.suppress_meta_fkey_rewrites = true;
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, settings.Clone());

  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(refresh_accelerator);
  ASSERT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(refresh_accelerator, accelerator_aliases[0]);

  ui::KeyboardDevice fake_internal_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout2Tag, /*phys=*/"", /*sys_path=*/base::FilePath(),
      /*vendor=*/0x1111, /*product=*/0x1111, /*version=*/0x0);
  fake_internal_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_internal_keyboard,
                                          kKbdTopRowLayout2Tag);

  settings.top_row_are_fkeys = true;
  settings.suppress_meta_fkey_rewrites = false;
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_internal_keyboard.id, settings.Clone());

  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(refresh_accelerator);
  ASSERT_EQ(2u, accelerator_aliases.size());
  EXPECT_EQ(refresh_accelerator, accelerator_aliases[0]);
  EXPECT_EQ(ui::Accelerator(ui::VKEY_BROWSER_REFRESH, ui::EF_COMMAND_DOWN),
            accelerator_aliases[1]);
}

class TopRowAliasTest : public AcceleratorAliasConverterTest,
                        public testing::WithParamInterface<
                            TopRowAcceleratorAliasConverterTestData> {
 public:
  void SetUp() override {
    AcceleratorAliasConverterTest::SetUp();
    TopRowAcceleratorAliasConverterTestData test_data = GetParam();
    keyboard_connection_type_ = test_data.keyboard_connection_type_;
    keyboard_layout_types_ = test_data.keyboard_layout_types_;
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  std::vector<ui::InputDeviceType> keyboard_connection_type_;
  std::vector<std::string> keyboard_layout_types_;
  ui::Accelerator accelerator_;
  std::vector<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    TopRowAliasTest,
    testing::ValuesIn(std::vector<TopRowAcceleratorAliasConverterTestData>{
        // [Search] as original modifier prevents remapping, regardless of
        // keyboard connection type.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN},
         {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN}}},

        // key_code not as a top row key prevents remapping, regardless of
        // keyboard connection type.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
          ui::InputDeviceType::INPUT_DEVICE_USB},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_TAB, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_TAB, ui::EF_ALT_DOWN}}},

        // Below are testing each layout type.

        // For TopRowLayout1: [Alt] + [Back] -> [Alt] + [Search] + [F1].
        // TopRowKeysAreFKeys() remains true. This statement applies to all
        // tests in TopRowAliasTest class.
        {{ui::InputDeviceType::INPUT_DEVICE_USB},
         {kKbdTopRowLayoutUnspecified},
         ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F1, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
          ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN}}},

        // For internal keyboard only, we shows icon + meta.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_BROWSER_BACK,
                          ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For layout 1, remap F1 to back key.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_F1, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN}}},

        // For internal keyboard only, layout1 doesn't have
        // VKEY_MEDIA_PLAY_PAUSE key.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_ALT_DOWN},
         {}},

        // For internal keyboard only, layout2 doesn't have
        // VKEY_BROWSER_FORWARD key.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN},
         {}},

        // For layout2, remap F2 to refresh key.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_F2, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_ALT_DOWN}}},

        // Layout2 doesn't have VKEY_ALL_APPLICATIONS key.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_ALL_APPLICATIONS, ui::EF_ALT_DOWN},
         {}},

        // Layout1 doesn't have VKEY_SNAPSHOT key.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_SNAPSHOT, ui::EF_ALT_DOWN},
         {}},

        // Layout1 doesn't have VKEY_SNAPSHOT key, but since its external, it
        // should always be shown.
        {{ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH},
         {kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_SNAPSHOT, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_SNAPSHOT, ui::EF_ALT_DOWN}}},

        // LayoutWilco doesn't have VKEY_MICROPHONE_MUTE_TOGGLE key.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayoutWilcoTag},
         ui::Accelerator{ui::VKEY_MICROPHONE_MUTE_TOGGLE, ui::EF_ALT_DOWN},
         {}},

        // LayoutWilco doesn't have VKEY_MICROPHONE_MUTE_TOGGLE key, but since
        // it is external, it should always be shown.
        {{ui::InputDeviceType::INPUT_DEVICE_USB},
         {kKbdTopRowLayoutWilcoTag},
         ui::Accelerator{ui::VKEY_MICROPHONE_MUTE_TOGGLE, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_MICROPHONE_MUTE_TOGGLE, ui::EF_ALT_DOWN}}},

        // For TopRowLayout1: [Alt] + [Forward] -> [Alt] + [Search] + [F2].
        {{ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH},
         {kKbdTopRowLayoutUnspecified},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F2, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
          ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN}}},

        // For TopRowLayout1: [Alt] + [Zoom] -> [Alt] + [Search] + [F4].
        {{ui::InputDeviceType::INPUT_DEVICE_USB},
         {kKbdTopRowLayoutUnspecified},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F4, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
          {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN}}}},

        // For TopRowLayout2: [Alt] + [Shift] + [Back] -> [Alt] + [Shift] +
        // [Search] + [Back].
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_BROWSER_BACK,
                         ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN},
         {ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_COMMAND_DOWN |
                                                     ui::EF_ALT_DOWN |
                                                     ui::EF_SHIFT_DOWN}}},

        // For TopRowLayout2: [Alt] + [Zoom] -> [Alt] + [Search] + [F3].
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_ZOOM,
                          ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout2: [Alt] + [Pause] -> [Alt] + [Search] + [F7].
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_MEDIA_PLAY_PAUSE,
                          ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayoutWilco: [Alt] + [Zoom] -> [Alt] + [Search] + [F3].
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayoutWilcoTag},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_ZOOM,
                          ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayoutWilco: [Alt] + [VolumeUp] -> [Alt] + [Search] + [F9].
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayoutWilcoTag},
         ui::Accelerator{ui::VKEY_VOLUME_UP, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_VOLUME_UP,
                          ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For kKbdTopRowLayoutDrallionTag: [Alt] + [Mute] -> [Alt] + [Search] +
        // [F7].
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayoutDrallionTag},
         ui::Accelerator{ui::VKEY_VOLUME_MUTE, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_VOLUME_MUTE,
                          ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // Aliasing should be for the most recently connected external keyboard
        // (last item in the list).
        // In this case, the most recently connected external keyboard does not
        // have a browser forward key.
        // As the most recently connected keyboard and the internal keyboards
        // are both ChromeOS keyboards, no alias is generated.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
          ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
          ui::InputDeviceType::INPUT_DEVICE_USB},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayoutUnspecified,
          kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_NONE},
         {}},

        // Since the external keyboard uses Layout1 by default, it should map to
        // F2.
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
          ui::InputDeviceType::INPUT_DEVICE_USB},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayoutUnspecified},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_F2, ui::EF_COMMAND_DOWN},
          ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_COMMAND_DOWN},
          ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_NONE}}},

        // Since the external keyboard uses Layout1 by default, it should map to
        // F2, even if the most recently connected keyboard (which is internal)
        // does not have the key.
        {{ui::InputDeviceType::INPUT_DEVICE_USB,
          ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayoutUnspecified, kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_F2, ui::EF_COMMAND_DOWN},
          ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_NONE}}},
    }));

TEST_P(TopRowAliasTest, CheckTopRowAlias) {
  // Add fake keyboards based on layout type.
  fake_keyboard_manager_->RemoveAllDevices();
  for (int i = 0; const std::string& layout : keyboard_layout_types_) {
    ui::KeyboardDevice fake_keyboard(
        /*id=*/i, /*type=*/keyboard_connection_type_[i],
        /*name=*/layout);
    fake_keyboard.sys_path = base::FilePath("path" + layout);
    fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, layout);
    SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/true);
    i++;
  }

  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);
  base::ranges::sort(accelerator_alias);
  base::ranges::sort(expected_accelerators_);

  ASSERT_EQ(expected_accelerators_.size(), accelerator_alias.size());
  for (size_t i = 0; i < expected_accelerators_.size(); i++) {
    EXPECT_EQ(expected_accelerators_[i], accelerator_alias[i]);
  }
}

class SixPackAliasTestWithExternalKeyboard
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
  void SetUp() override {
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  ui::Accelerator accelerator_;
  std::optional<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    SixPackAliasTestWithExternalKeyboard,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{
        // [Search] as original modifier prevents remapping.
        {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN}, std::nullopt},
        // key_code not as six pack key prevents remapping.
        {ui::Accelerator{ui::VKEY_TAB, ui::EF_ALT_DOWN}, std::nullopt},
        // [Shift] + [Delete] should not be remapped.
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_SHIFT_DOWN}, std::nullopt},
        // [Shift] + [Insert] should not be remapped.
        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_SHIFT_DOWN}, std::nullopt},
        // For Insert: [modifiers] -> [Search] + [Shift] + [original_modifiers].
        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN |
                                            ui::EF_SHIFT_DOWN |
                                            ui::EF_ALT_DOWN}},
        // For other six-pack-keys: [modifiers] -> [Search] +
        // [original_modifiers].
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}},
    }));

TEST_P(SixPackAliasTestWithExternalKeyboard, CheckSixPackAlias) {
  fake_keyboard_manager_->RemoveAllDevices();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  if (expected_accelerators_.has_value()) {
    // Accelerator has valid a remapping.
    EXPECT_EQ(2u, accelerator_alias.size());
    EXPECT_EQ(expected_accelerators_, accelerator_alias[0]);
    EXPECT_EQ(accelerator_, accelerator_alias[1]);
  } else {
    // Accelerator doesn't have a valid remapping.
    EXPECT_EQ(1u, accelerator_alias.size());
    ASSERT_EQ(accelerator_, accelerator_alias[0]);
  }
}

class SixPackAliasTestWithInternalKeyboard
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
  void SetUp() override {
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  ui::Accelerator accelerator_;
  std::optional<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    SixPackAliasTestWithInternalKeyboard,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{
        // A keyboard without six pack keys should not affect the aliasing of
        // six pack key.

        // [Search] as original modifier prevents remapping.
        {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN}, std::nullopt},
        // key_code not as six pack key prevents remapping.
        {ui::Accelerator{ui::VKEY_TAB, ui::EF_ALT_DOWN}, std::nullopt},
        // [Shift] + [Delete] should not be remapped.
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_SHIFT_DOWN}, std::nullopt},
        // [Shift] + [Insert] should not be remapped.
        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_SHIFT_DOWN}, std::nullopt},
        // For Insert: [modifiers] -> [Search] + [Shift] + [original_modifiers].
        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN |
                                            ui::EF_SHIFT_DOWN |
                                            ui::EF_ALT_DOWN}},
        // For other six-pack-keys: [modifiers] -> [Search] +
        // [original_modifiers].
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}},
    }));

TEST_P(SixPackAliasTestWithInternalKeyboard, CheckSixPackAlias) {
  fake_keyboard_manager_->RemoveAllDevices();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout2Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  if (expected_accelerators_.has_value()) {
    // Accelerator has valid a remapping.
    EXPECT_EQ(1u, accelerator_alias.size());
    EXPECT_EQ(expected_accelerators_, accelerator_alias[0]);
  } else if (ui::KeyboardCapability::IsSixPackKey(accelerator_.key_code())) {
    // Original accelerator has six pack key, which is not supported by internal
    // keyboard.
    EXPECT_EQ(0u, accelerator_alias.size());
  } else {
    EXPECT_EQ(1u, accelerator_alias.size());
    EXPECT_EQ(accelerator_, accelerator_alias[0]);
  }
}

class SixPackAliasAltTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kInputDeviceSettingsSplit,
         ash::features::kAltClickAndSixPackCustomization},
        /*disabled_features=*/{});
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::Accelerator accelerator_;
  std::optional<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    SixPackAliasAltTest,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_ALT_DOWN}},

        {ui::Accelerator{ui::VKEY_HOME, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_UP, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},

        {ui::Accelerator{ui::VKEY_PRIOR, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_UP, ui::EF_ALT_DOWN}},

        {ui::Accelerator{ui::VKEY_END, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_DOWN, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},

        {ui::Accelerator{ui::VKEY_NEXT, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_DOWN, ui::EF_ALT_DOWN}},

        // The following should not perform an alias since Alt is part of the
        // original accelerator.
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_ALT_DOWN}, std::nullopt},

        {ui::Accelerator{ui::VKEY_HOME, ui::EF_ALT_DOWN}, std::nullopt},

        {ui::Accelerator{ui::VKEY_PRIOR, ui::EF_ALT_DOWN}, std::nullopt},

        {ui::Accelerator{ui::VKEY_END, ui::EF_ALT_DOWN}, std::nullopt},

        {ui::Accelerator{ui::VKEY_NEXT, ui::EF_ALT_DOWN}, std::nullopt},

        {ui::Accelerator{ui::VKEY_HOME, ui::EF_CONTROL_DOWN}, std::nullopt},

        {ui::Accelerator{ui::VKEY_END, ui::EF_CONTROL_DOWN}, std::nullopt},

        {ui::Accelerator{ui::VKEY_HOME, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN},
         std::nullopt},

        {ui::Accelerator{ui::VKEY_END, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN},
         std::nullopt},
    }));

TEST_P(SixPackAliasAltTest, CheckSixPackAliasAlt) {
  fake_keyboard_manager_->RemoveAllDevices();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout2Tag);
  auto settings = Shell::Get()
                      ->input_device_settings_controller()
                      ->GetKeyboardSettings(fake_keyboard.id)
                      ->Clone();

  mojom::SixPackKeyInfoPtr six_pack_key_info = mojom::SixPackKeyInfo::New();
  six_pack_key_info->del = ui::mojom::SixPackShortcutModifier::kAlt;
  six_pack_key_info->home = ui::mojom::SixPackShortcutModifier::kAlt;
  six_pack_key_info->end = ui::mojom::SixPackShortcutModifier::kAlt;
  six_pack_key_info->page_down = ui::mojom::SixPackShortcutModifier::kAlt;
  six_pack_key_info->page_up = ui::mojom::SixPackShortcutModifier::kAlt;

  settings->six_pack_key_remappings = six_pack_key_info.Clone();
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, std::move(settings));
  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  if (expected_accelerators_.has_value()) {
    // Accelerator has valid a remapping.
    EXPECT_EQ(1u, accelerator_alias.size());
    EXPECT_EQ(expected_accelerators_, accelerator_alias[0]);
  } else {
    EXPECT_EQ(0u, accelerator_alias.size());
  }
}

class SixPackAliasSearchTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kInputDeviceSettingsSplit,
         ash::features::kAltClickAndSixPackCustomization},
        /*disabled_features=*/{});
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::Accelerator accelerator_;
  std::optional<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    SixPackAliasSearchTest,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{

        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BACK,
                         ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN}},

        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN}},

        {ui::Accelerator{ui::VKEY_HOME, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_LEFT, ui::EF_COMMAND_DOWN}},

        {ui::Accelerator{ui::VKEY_PRIOR, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_UP, ui::EF_COMMAND_DOWN}},

        {ui::Accelerator{ui::VKEY_END, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN}},

        {ui::Accelerator{ui::VKEY_NEXT, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_DOWN, ui::EF_COMMAND_DOWN}},
    }));

TEST_P(SixPackAliasSearchTest, CheckSixPackAliasSearch) {
  fake_keyboard_manager_->RemoveAllDevices();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout2Tag);
  auto settings = Shell::Get()
                      ->input_device_settings_controller()
                      ->GetKeyboardSettings(fake_keyboard.id)
                      ->Clone();

  mojom::SixPackKeyInfoPtr six_pack_key_info = mojom::SixPackKeyInfo::New();
  settings->six_pack_key_remappings = six_pack_key_info.Clone();
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, std::move(settings));
  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  EXPECT_EQ(1u, accelerator_alias.size());
  EXPECT_EQ(expected_accelerators_, accelerator_alias[0]);
}

TEST_P(SixPackAliasSearchTest, CheckSixPackAliasNone) {
  fake_keyboard_manager_->RemoveAllDevices();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout2Tag);
  auto settings = Shell::Get()
                      ->input_device_settings_controller()
                      ->GetKeyboardSettings(fake_keyboard.id)
                      ->Clone();

  mojom::SixPackKeyInfoPtr six_pack_key_info = mojom::SixPackKeyInfo::New();
  six_pack_key_info->del = ui::mojom::SixPackShortcutModifier::kNone;
  six_pack_key_info->home = ui::mojom::SixPackShortcutModifier::kNone;
  six_pack_key_info->insert = ui::mojom::SixPackShortcutModifier::kNone;
  six_pack_key_info->end = ui::mojom::SixPackShortcutModifier::kNone;
  six_pack_key_info->page_down = ui::mojom::SixPackShortcutModifier::kNone;
  six_pack_key_info->page_up = ui::mojom::SixPackShortcutModifier::kNone;
  settings->six_pack_key_remappings = six_pack_key_info.Clone();
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, std::move(settings));
  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  EXPECT_EQ(0u, accelerator_alias.size());
}

class MediaKeyAliasTest : public AcceleratorAliasConverterTest,
                          public testing::WithParamInterface<ui::KeyboardCode> {
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaKeyAliasTest,
    testing::Values(ui::VKEY_OEM_103,  // Media Rewind
                    ui::VKEY_OEM_104,  // Media Fast Forward
                    ui::VKEY_MEDIA_STOP));

TEST_P(MediaKeyAliasTest, CheckMediaKeyAlias) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  AcceleratorAliasConverter accelerator_alias_converter_;

  const ui::Accelerator accelerator{GetParam(), ui::EF_NONE};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(0u, accelerator_aliases.size());

  ui::KeyboardDevice wilco_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/kKbdTopRowLayoutWilcoTag);
  wilco_keyboard.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(wilco_keyboard,
                                          kKbdTopRowLayoutWilcoTag);
  accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

class ExtendedFKeysAliasAltTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kInputDeviceSettingsSplit,
         ash::features::kAltClickAndSixPackCustomization,
         ::features::kSupportF11AndF12KeyShortcuts},
        /*disabled_features=*/{});
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::Accelerator accelerator_;
  std::optional<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    ExtendedFKeysAliasAltTest,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{
        // The following test cases all have `top_row_are_fkeys` enabled
        // and use an internal ChromeOS keyboard. The F11/F12 accelerators
        // will be aliased due to ChromeOS keyboards having fewer than 12 keys
        // in the top row. The `top_row_are_fkeys` setting determines whether
        // or not `ui::EF_COMMAND_DOWN` should be present in the aliased
        // accelerator.
        {ui::Accelerator{ui::VKEY_F11, ui::EF_CONTROL_DOWN},
         ui::Accelerator{ui::VKEY_BROWSER_BACK,
                         ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},

        {ui::Accelerator{ui::VKEY_F12, ui::EF_CONTROL_DOWN},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD,
                         ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN}},

        {ui::Accelerator{ui::VKEY_F11, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN}},

        {ui::Accelerator{ui::VKEY_F12, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN}},

        // The following should not perform an alias since Alt is part of the
        // original accelerator.
        {ui::Accelerator{ui::VKEY_F11, ui::EF_ALT_DOWN}, std::nullopt},
    }));

TEST_P(ExtendedFKeysAliasAltTest, CheckExtendedFKeysAliasAlt) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  SetKeyboardInfo(
      fake_keyboard,
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1);
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);
  auto settings = Shell::Get()
                      ->input_device_settings_controller()
                      ->GetKeyboardSettings(fake_keyboard.id)
                      ->Clone();

  settings->f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  settings->f12 = ui::mojom::ExtendedFkeysModifier::kAlt;
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, mojo::Clone(settings));
  AcceleratorAliasConverter accelerator_alias_converter_;
  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/true);
  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);
  if (expected_accelerators_.has_value()) {
    // Accelerator has valid a remapping.
    EXPECT_EQ(1u, accelerator_alias.size());
    EXPECT_EQ(expected_accelerators_, accelerator_alias[0]);
  } else {
    // Expect base accelerator.
    EXPECT_EQ(accelerator_, accelerator_alias[0]);
  }
}

class ExtendedFKeysAliasShiftTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kInputDeviceSettingsSplit,
         ash::features::kAltClickAndSixPackCustomization,
         ::features::kSupportF11AndF12KeyShortcuts},
        /*disabled_features=*/{});
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::Accelerator accelerator_;
  std::optional<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    ExtendedFKeysAliasShiftTest,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{

        {ui::Accelerator{ui::VKEY_F11, ui::EF_CONTROL_DOWN},
         ui::Accelerator{ui::VKEY_BROWSER_BACK,
                         ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}},

        {ui::Accelerator{ui::VKEY_F12, ui::EF_CONTROL_DOWN},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD,
                         ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}},

        {ui::Accelerator{ui::VKEY_F11, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN}},

        {ui::Accelerator{ui::VKEY_F12, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_SHIFT_DOWN}},

        // The following should not perform an alias since Shift is part of
        // the original accelerator.
        {ui::Accelerator{ui::VKEY_F11, ui::EF_SHIFT_DOWN}, std::nullopt},
    }));

TEST_P(ExtendedFKeysAliasShiftTest, CheckExtendedFKeysAliasShift) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  SetKeyboardInfo(
      fake_keyboard,
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1);
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);
  auto settings = Shell::Get()
                      ->input_device_settings_controller()
                      ->GetKeyboardSettings(fake_keyboard.id)
                      ->Clone();

  settings->f11 = ui::mojom::ExtendedFkeysModifier::kShift;
  settings->f12 = ui::mojom::ExtendedFkeysModifier::kShift;
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, mojo::Clone(settings));
  AcceleratorAliasConverter accelerator_alias_converter_;
  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/true);
  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);
  if (expected_accelerators_.has_value()) {
    // Accelerator has valid a remapping.
    EXPECT_EQ(1u, accelerator_alias.size());
    EXPECT_EQ(expected_accelerators_, accelerator_alias[0]);
  } else {
    // Expect base accelerator.
    EXPECT_EQ(accelerator_, accelerator_alias[0]);
  }
}

class ExtendedFKeysAliasCtrlShiftTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kInputDeviceSettingsSplit,
         ash::features::kAltClickAndSixPackCustomization,
         ::features::kSupportF11AndF12KeyShortcuts},
        /*disabled_features=*/{});
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::Accelerator accelerator_;
  std::optional<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    ExtendedFKeysAliasCtrlShiftTest,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{

        {ui::Accelerator{ui::VKEY_F11, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN |
                                                    ui::EF_CONTROL_DOWN |
                                                    ui::EF_ALT_DOWN}},

        {ui::Accelerator{ui::VKEY_F12, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_SHIFT_DOWN |
                                                       ui::EF_CONTROL_DOWN |
                                                       ui::EF_ALT_DOWN}},

        {ui::Accelerator{ui::VKEY_F11, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BROWSER_BACK,
                         ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}},

        {ui::Accelerator{ui::VKEY_F12, ui::EF_NONE},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD,
                         ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN}},

        // The following should not perform an alias since Ctrl and Shift
        // are part of the original accelerator.
        {ui::Accelerator{ui::VKEY_F11, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
         std::nullopt},

        // The following should not perform an alias since Ctrl is part of the
        // original accelerator.
        {ui::Accelerator{ui::VKEY_F11, ui::EF_CONTROL_DOWN}, std::nullopt},

        // The following should not perform an alias since Shift is part of the
        // original accelerator.
        {ui::Accelerator{ui::VKEY_F11, ui::EF_SHIFT_DOWN}, std::nullopt},
    }));

TEST_P(ExtendedFKeysAliasCtrlShiftTest, CheckExtendedFKeysAliasCtrlShift) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  SetKeyboardInfo(
      fake_keyboard,
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1);
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);
  auto settings = Shell::Get()
                      ->input_device_settings_controller()
                      ->GetKeyboardSettings(fake_keyboard.id)
                      ->Clone();

  settings->f11 = ui::mojom::ExtendedFkeysModifier::kCtrlShift;
  settings->f12 = ui::mojom::ExtendedFkeysModifier::kCtrlShift;
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, mojo::Clone(settings));
  AcceleratorAliasConverter accelerator_alias_converter_;
  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/true);
  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);
  if (expected_accelerators_.has_value()) {
    // Accelerator has valid a remapping.
    EXPECT_EQ(1u, accelerator_alias.size());
    EXPECT_EQ(expected_accelerators_, accelerator_alias[0]);
  } else {
    // Expect base accelerator.
    EXPECT_EQ(accelerator_, accelerator_alias[0]);
  }
}

class ExtendedFKeysAliasTest : public AcceleratorAliasConverterTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kInputDeviceSettingsSplit,
         ash::features::kAltClickAndSixPackCustomization,
         ::features::kSupportF11AndF12KeyShortcuts},
        /*disabled_features=*/{});
    AcceleratorAliasConverterTest::SetUp();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

TEST_F(ExtendedFKeysAliasTest, ExtendedFKeysAliasAltTopRowAreFKeys) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  SetKeyboardInfo(
      fake_keyboard,
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1);
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);
  auto settings = Shell::Get()
                      ->input_device_settings_controller()
                      ->GetKeyboardSettings(fake_keyboard.id)
                      ->Clone();

  settings->f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  settings->f12 = ui::mojom::ExtendedFkeysModifier::kAlt;
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard.id, mojo::Clone(settings));
  AcceleratorAliasConverter accelerator_alias_converter_;
  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/false);

  const ui::Accelerator accelerator{ui::VKEY_F11, ui::EF_CONTROL_DOWN};
  ui::Accelerator expected_accelerator{
      ui::VKEY_BROWSER_BACK,
      ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN};
  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_alias.size());
  EXPECT_EQ(expected_accelerator, accelerator_alias[0]);

  SetTopRowAsFKeysForKeyboard(fake_keyboard, /*enabled=*/true);
  expected_accelerator = {ui::VKEY_BROWSER_BACK,
                          ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN};
  accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(1u, accelerator_alias.size());
  EXPECT_EQ(expected_accelerator, accelerator_alias[0]);
}

TEST_F(ExtendedFKeysAliasTest, ExtendedFKeysAliasInternalAndExternalKb) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard1(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard1.sys_path = base::FilePath("path");
  SetKeyboardInfo(
      fake_keyboard1,
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1);
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);
  ui::KeyboardDevice fake_keyboard2(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_USB,
      /*name=*/kKbdTopRowLayoutUnspecified);
  fake_keyboard2.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout2Tag);
  SetKeyboardInfo(
      fake_keyboard2,
      ui::KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard,
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2);

  auto settings = Shell::Get()
                      ->input_device_settings_controller()
                      ->GetKeyboardSettings(fake_keyboard1.id)
                      ->Clone();

  settings->f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  settings->f12 = ui::mojom::ExtendedFkeysModifier::kAlt;
  Shell::Get()->input_device_settings_controller()->SetKeyboardSettings(
      fake_keyboard1.id, mojo::Clone(settings));
  AcceleratorAliasConverter accelerator_alias_converter_;
  SetTopRowAsFKeysForKeyboard(fake_keyboard1, /*enabled=*/true);
  const ui::Accelerator accelerator{ui::VKEY_F11, ui::EF_CONTROL_DOWN};
  ui::Accelerator expected_accelerator{ui::VKEY_BROWSER_BACK,
                                       ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN};
  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);
  EXPECT_EQ(2u, accelerator_alias.size());
  EXPECT_EQ(accelerator, accelerator_alias[0]);
  EXPECT_EQ(expected_accelerator, accelerator_alias[1]);
}

class ActionKeyboardVariantsTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<ActionKeyAliasConverterTestData> {
 public:
  void SetUp() override {
    AcceleratorAliasConverterTest::SetUp();
    ActionKeyAliasConverterTestData test_data = GetParam();
    keyboard_connection_type_ = test_data.keyboard_connection_type_;
    keyboard_layout_types_ = test_data.keyboard_layout_types_;
    top_row_are_fkeys_ = test_data.top_row_are_fkeys_;
    accelerator_ = test_data.accelerator_;
    expected_accelerators_ = test_data.expected_accelerators_;
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  std::vector<ui::InputDeviceType> keyboard_connection_type_;
  std::vector<std::string> keyboard_layout_types_;
  std::vector<bool> top_row_are_fkeys_;
  ui::Accelerator accelerator_;
  std::vector<ui::Accelerator> expected_accelerators_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    ActionKeyboardVariantsTest,
    testing::ValuesIn(std::vector<ActionKeyAliasConverterTestData>{
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout1Tag},
         {/*treat_top_row_as_fkeys=*/false},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE}}},
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL},
         {kKbdTopRowLayout1Tag},
         {/*treat_top_row_as_fkeys=*/true},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_COMMAND_DOWN}}},
        {{ui::InputDeviceType::INPUT_DEVICE_USB},
         {kKbdTopRowLayoutUnspecified},
         {/*treat_top_row_as_fkeys=*/false},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
          ui::Accelerator{ui::VKEY_F3, ui::EF_NONE}}},
        {{ui::InputDeviceType::INPUT_DEVICE_USB},
         {kKbdTopRowLayoutUnspecified},
         {/*treat_top_row_as_fkeys=*/true},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
          ui::Accelerator{ui::VKEY_F3, ui::EF_COMMAND_DOWN}}},
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL, ui::INPUT_DEVICE_USB},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayoutUnspecified},
         {/*treat_top_row_as_fkeys=*/false, /*treat_top_row_as_fkeys=*/false},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
          ui::Accelerator{ui::VKEY_F3, ui::EF_NONE}}},
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL, ui::INPUT_DEVICE_USB},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayoutUnspecified},
         {/*treat_top_row_as_fkeys=*/false, /*treat_top_row_as_fkeys=*/true},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
          ui::Accelerator{ui::VKEY_F3, ui::EF_COMMAND_DOWN}}},
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL, ui::INPUT_DEVICE_USB},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayoutUnspecified},
         {/*treat_top_row_as_fkeys=*/true, /*treat_top_row_as_fkeys=*/false},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_F3, ui::EF_NONE},
          ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
          ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_COMMAND_DOWN}}},
        {{ui::InputDeviceType::INPUT_DEVICE_INTERNAL, ui::INPUT_DEVICE_USB},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayoutUnspecified},
         {/*treat_top_row_as_fkeys=*/true, /*treat_top_row_as_fkeys=*/true},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
         {ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_NONE},
          ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_COMMAND_DOWN},
          ui::Accelerator{ui::VKEY_F3, ui::EF_COMMAND_DOWN}}},
    }));

TEST_P(ActionKeyboardVariantsTest, CheckTopRowAlias) {
  // Add fake keyboards based on layout type.
  fake_keyboard_manager_->RemoveAllDevices();
  for (int i = 0; const std::string& layout : keyboard_layout_types_) {
    ui::KeyboardDevice fake_keyboard(
        /*id=*/i, /*type=*/keyboard_connection_type_[i],
        /*name=*/layout);
    fake_keyboard.sys_path = base::FilePath("path" + layout);
    fake_keyboard.vendor_id = i;
    fake_keyboard.product_id = i;
    fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, layout);
    SetTopRowAsFKeysForKeyboard(fake_keyboard, top_row_are_fkeys_[i]);
    i++;
  }

  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);
  base::ranges::sort(accelerator_alias);
  base::ranges::sort(expected_accelerators_);

  ASSERT_EQ(expected_accelerators_.size(), accelerator_alias.size());
  for (size_t i = 0; i < expected_accelerators_.size(); i++) {
    EXPECT_EQ(expected_accelerators_[i], accelerator_alias[i]);
  }
}

}  // namespace ash
