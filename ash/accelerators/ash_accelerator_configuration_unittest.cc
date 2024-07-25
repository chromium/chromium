// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <memory>
#include <string>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_configuration.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/optional_ref.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::ash::mojom::AcceleratorConfigResult;

constexpr char kAcceleratorModifiersKey[] = "modifiers";
constexpr char kAcceleratorKeyCodeKey[] = "key";
constexpr char kAcceleratorModificationActionKey[] = "action";
constexpr char kAcceleratorStateKey[] = "state";
constexpr char kAcceleratorTypeKey[] = "type";
constexpr char kAcceleratorKeyStateKey[] = "key_state";
constexpr char kFakeUserEmail[] = "fakeuser@gmail.com";
constexpr char kFakeUserEmail2[] = "fakeuser2@gmail.com";

class UpdatedAcceleratorsObserver
    : public ash::AshAcceleratorConfiguration::Observer {
 public:
  UpdatedAcceleratorsObserver() = default;
  UpdatedAcceleratorsObserver(const UpdatedAcceleratorsObserver&) = delete;
  UpdatedAcceleratorsObserver& operator=(const UpdatedAcceleratorsObserver&) =
      delete;
  ~UpdatedAcceleratorsObserver() override = default;

  // ash::AshAcceleratorConfiguration::Observer:
  void OnAcceleratorsUpdated() override {
    ++num_times_accelerator_updated_called_;
  }

  int num_times_accelerator_updated_called() {
    return num_times_accelerator_updated_called_;
  }

 private:
  int num_times_accelerator_updated_called_ = 0;
};

base::Value AcceleratorModificationDataToValue(
    const ui::Accelerator& accelerator,
    AcceleratorModificationAction action) {
  base::Value::Dict accelerator_values;
  accelerator_values.Set(kAcceleratorModifiersKey, accelerator.modifiers());
  accelerator_values.Set(kAcceleratorKeyCodeKey,
                         static_cast<int>(accelerator.key_code()));
  accelerator_values.Set(
      kAcceleratorTypeKey,
      static_cast<int>(ash::mojom::AcceleratorType::kDefault));
  accelerator_values.Set(
      kAcceleratorStateKey,
      static_cast<int>(ash::mojom::AcceleratorState::kEnabled));
  accelerator_values.Set(kAcceleratorModificationActionKey,
                         static_cast<int>(action));
  return base::Value(std::move(accelerator_values));
}

base::Value::Dict GetOverridePref() {
  return ash::Shell::Get()
      ->session_controller()
      ->GetActivePrefService()
      ->GetDict(ash::prefs::kShortcutCustomizationOverrides)
      .Clone();
}

void SetOverridePref(const ui::Accelerator& accelerator,
                     AcceleratorModificationAction action,
                     uint32_t action_id) {
  base::Value::List override_list;
  override_list.Append(AcceleratorModificationDataToValue(accelerator, action));

  base::Value::Dict overrides;
  overrides.Set(base::NumberToString(action_id), std::move(override_list));
  ash::Shell::Get()->session_controller()->GetActivePrefService()->SetDict(
      ash::prefs::kShortcutCustomizationOverrides, std::move(overrides));
}

AcceleratorModificationData ValueToAcceleratorModificationData(
    const base::Value::Dict& value) {
  std::optional<int> keycode = value.FindInt(kAcceleratorKeyCodeKey);
  std::optional<int> modifier = value.FindInt(kAcceleratorModifiersKey);
  std::optional<int> modification_action =
      value.FindInt(kAcceleratorModificationActionKey);
  std::optional<int> key_state = value.FindInt(kAcceleratorKeyStateKey);
  CHECK(keycode.has_value());
  CHECK(modifier.has_value());
  CHECK(modification_action.has_value());
  CHECK(key_state.has_value());
  ui::Accelerator accelerator(
      static_cast<ui::KeyboardCode>(*keycode), static_cast<int>(*modifier),
      static_cast<ui::Accelerator::KeyState>(*key_state));
  return {accelerator,
          static_cast<AcceleratorModificationAction>(*modification_action)};
}

bool CompareAccelerators(const ash::AcceleratorData& expected_data,
                         const ui::Accelerator& actual_accelerator) {
  ui::Accelerator expected_accel(expected_data.keycode, expected_data.modifiers,
                                 expected_data.trigger_on_press
                                     ? ui::Accelerator::KeyState::PRESSED
                                     : ui::Accelerator::KeyState::RELEASED);
  return expected_accel.key_code() == actual_accelerator.key_code() &&
         expected_accel.modifiers() == actual_accelerator.modifiers() &&
         expected_accel.key_state() == actual_accelerator.key_state();
}

void ExpectAllAcceleratorsEqual(
    const base::span<const ash::AcceleratorData> expected,
    const std::vector<ui::Accelerator>& actual) {
  EXPECT_EQ(std::size(expected), actual.size());

  for (const auto& actual_accelerator : actual) {
    bool found_match = false;
    for (const auto& expected_data : expected) {
      found_match = CompareAccelerators(expected_data, actual_accelerator);
      if (found_match) {
        break;
      }
    }
    EXPECT_TRUE(found_match);
  }
}
}  // namespace

namespace ash {

class AshAcceleratorConfigurationTest : public AshTestBase {
 public:
  AshAcceleratorConfigurationTest() = default;

  ~AshAcceleratorConfigurationTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kShortcutCustomization);
    AshTestBase::SetUp();
    config_ = std::make_unique<AshAcceleratorConfiguration>();
    config_->AddObserver(&observer_);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    config_->RemoveObserver(&observer_);
    AshTestBase::TearDown();
    histogram_tester_.reset();
  }

 protected:
  base::optional_ref<const std::vector<ui::Accelerator>>
  GetAcceleratorsForAction(AcceleratorActionId action_id) {
    return config_->GetAcceleratorsForAction(action_id);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  UpdatedAcceleratorsObserver observer_;
  std::unique_ptr<AshAcceleratorConfiguration> config_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(AshAcceleratorConfigurationTest, VerifyAcceleratorMappingPopulated) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {true, ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowEmojiPicker, true},
      {true, ui::VKEY_EMOJI_PICKER, ui::EF_NONE,
       AcceleratorAction::kShowEmojiPicker, true},
  };

  config_->Initialize(test_data);
  const std::vector<ui::Accelerator>& accelerators =
      config_->GetAllAccelerators();

  ExpectAllAcceleratorsEqual(test_data, accelerators);

  ui::Accelerator accelerator(ui::VKEY_EMOJI_PICKER, ui::EF_NONE);
  EXPECT_TRUE(config_->IsAcceleratorLocked(accelerator));
}

TEST_F(AshAcceleratorConfigurationTest, DeprecatedAccelerators) {
  // Test deprecated accelerators, in this case
  // `AcceleratorAction::kShowTaskManager` has two associated accelerators:
  // (deprecated) ESCAPE + SHIFT and (active) ESCAPE + COMMAND.
  const AcceleratorData initial_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  const AcceleratorData expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  const DeprecatedAcceleratorData deprecated_data[] = {
      {AcceleratorAction::kShowTaskManager,
       /*uma_histogram_name=*/"deprecated.showTaskManager",
       /*notification_message_id=*/1,
       /*new_shortcut_id=*/2,
       ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN),
       /*deprecated_enabled=*/true},
  };

  const AcceleratorData test_deprecated_accelerators[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  config_->Initialize(initial_test_data);
  config_->InitializeDeprecatedAccelerators(deprecated_data,
                                            test_deprecated_accelerators);

  const std::vector<ui::Accelerator>& accelerators =
      config_->GetAllAccelerators();

  // When initializing deprecated accelerators, expect them to be added to the
  // overall accelerators list too.
  ExpectAllAcceleratorsEqual(expected_test_data, accelerators);

  // Verify that the fetched deprecated accelerators are correct.
  std::vector<ui::Accelerator> deprecated_accelerators;
  for (const auto& accel : accelerators) {
    if (config_->IsDeprecated(accel)) {
      deprecated_accelerators.push_back(accel);
    }
  }
  ExpectAllAcceleratorsEqual(test_deprecated_accelerators,
                             deprecated_accelerators);

  // Verify ESCAPE + SHIFT is deprecated.
  const ui::Accelerator deprecated_accelerator(ui::VKEY_ESCAPE,
                                               ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(config_->IsDeprecated(deprecated_accelerator));
  // Verify fetching a deprecated accelerator works.
  EXPECT_EQ(deprecated_data, config_->GetDeprecatedAcceleratorData(
                                 AcceleratorAction::kShowTaskManager));
  // AcceleratorAction::kCycleBackwardMru is not a deprecated action, expect
  // nullptr.
  EXPECT_EQ(nullptr, config_->GetDeprecatedAcceleratorData(
                         AcceleratorAction::kCycleBackwardMru));

  // Verify that ESCAPE + COMMAND is not deprecated.
  const ui::Accelerator active_accelerator(ui::VKEY_ESCAPE,
                                           ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(config_->IsDeprecated(active_accelerator));
}

TEST_F(AshAcceleratorConfigurationTest,
       RemoveAndRestoreDeprecatedAccelerators) {
  // Test deprecated accelerators, in this case
  // `AcceleratorAction::kShowTaskManager` has two associated accelerators:
  // (deprecated) ESCAPE + SHIFT and (active) ESCAPE + COMMAND.
  const AcceleratorData initial_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  const AcceleratorData expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  const DeprecatedAcceleratorData deprecated_data[] = {
      {AcceleratorAction::kShowTaskManager,
       /*uma_histogram_name=*/"deprecated.showTaskManager",
       /*notification_message_id=*/1,
       /*new_shortcut_id=*/2,
       ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN),
       /*deprecated_enabled=*/true},
  };

  const AcceleratorData test_deprecated_accelerators[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  config_->Initialize(initial_test_data);
  config_->InitializeDeprecatedAccelerators(deprecated_data,
                                            test_deprecated_accelerators);

  const ui::Accelerator deprecated_accelerator(ui::VKEY_ESCAPE,
                                               ui::EF_SHIFT_DOWN);

  // Remove the deprecated accelerator ESCAPE + SHIFT.
  const AcceleratorData updated_expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kShowTaskManager},
  };
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kShowTaskManager, deprecated_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  // Verify that the accelerator is no longer deprecated.
  EXPECT_FALSE(config_->IsDeprecated(deprecated_accelerator));
  EXPECT_FALSE(config_->GetDeprecatedAcceleratorData(
      AcceleratorAction::kShowTaskManager));
  ExpectAllAcceleratorsEqual(updated_expected_test_data,
                             config_->GetAllAccelerators());

  // Attempt to restore AcceleratorAction::kShowTaskManager, expect deprecated
  // accelerator to NOT be re-added.
  result = config_->RestoreDefault(AcceleratorAction::kShowTaskManager);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  EXPECT_FALSE(config_->IsDeprecated(deprecated_accelerator));
  EXPECT_FALSE(config_->GetDeprecatedAcceleratorData(
      AcceleratorAction::kShowTaskManager));
  ExpectAllAcceleratorsEqual(updated_expected_test_data,
                             config_->GetAllAccelerators());

  // Now restore all accelerators, this time deprecated accelerators should be
  // restored.
  result = config_->RestoreAllDefaults();
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  EXPECT_TRUE(config_->IsDeprecated(deprecated_accelerator));
  EXPECT_EQ(deprecated_data, config_->GetDeprecatedAcceleratorData(
                                 AcceleratorAction::kShowTaskManager));
  ExpectAllAcceleratorsEqual(expected_test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, IsDefaultAccelerator) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       AcceleratorAction::kSwapPrimaryDisplay},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, AcceleratorAction::kTakeScreenshot},
      {/*trigger_on_press=*/true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
  };

  // `Initialize()` sets up the default accelerators.
  config_->Initialize(test_data);
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());

  // Verify that Control + Zoom is the default for
  // AcceleratorAction::kToggleMirrorMode.
  ui::Accelerator expected_default =
      ui::Accelerator(ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN);
  std::optional<AcceleratorAction> accelerator_id =
      config_->GetIdForDefaultAccelerator(expected_default);
  EXPECT_TRUE(accelerator_id.has_value());
  EXPECT_EQ(AcceleratorAction::kToggleMirrorMode, accelerator_id.value());
  std::vector<ui::Accelerator> default_accelerators =
      config_->GetDefaultAcceleratorsForId(
          AcceleratorAction::kToggleMirrorMode);
  EXPECT_EQ(1u, default_accelerators.size());
  EXPECT_EQ(expected_default, default_accelerators[0]);

  std::vector<ui::Accelerator> nonexistent_defaults =
      config_->GetDefaultAcceleratorsForId(
          /*id=*/789987);
  EXPECT_EQ(0u, nonexistent_defaults.size());
}

TEST_F(AshAcceleratorConfigurationTest, MultipleDefaultAccelerators) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, AcceleratorAction::kTakeScreenshot},
      {/*trigger_on_press=*/true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
  };

  // `Initialize()` sets up the default accelerators.
  config_->Initialize(test_data);
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());

  // Verify that Control + Zoom and Alt + Zoom are defaults for
  // AcceleratorAction::kToggleMirrorMode.
  ui::Accelerator expected_default =
      ui::Accelerator(ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN);
  ui::Accelerator expected_default_2 =
      ui::Accelerator(ui::VKEY_ZOOM, ui::EF_ALT_DOWN);

  std::optional<AcceleratorAction> accelerator_id =
      config_->GetIdForDefaultAccelerator(expected_default);
  EXPECT_TRUE(accelerator_id.has_value());
  EXPECT_EQ(AcceleratorAction::kToggleMirrorMode, accelerator_id.value());

  accelerator_id = config_->GetIdForDefaultAccelerator(expected_default_2);
  EXPECT_TRUE(accelerator_id.has_value());
  EXPECT_EQ(AcceleratorAction::kToggleMirrorMode, accelerator_id.value());

  std::vector<ui::Accelerator> default_accelerators =
      config_->GetDefaultAcceleratorsForId(
          AcceleratorAction::kToggleMirrorMode);

  EXPECT_EQ(2u, default_accelerators.size());

  EXPECT_TRUE(base::Contains(default_accelerators, expected_default));
  EXPECT_TRUE(base::Contains(default_accelerators, expected_default_2));
}
TEST_F(AshAcceleratorConfigurationTest, DefaultNotFound) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       AcceleratorAction::kSwapPrimaryDisplay},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, AcceleratorAction::kTakeScreenshot},
      {/*trigger_on_press=*/true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
  };

  // `Initialize()` sets up the default accelerators.
  config_->Initialize(test_data);
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());

  // Verify that Ctrl + U is not a default accelerator in this test set.
  ui::Accelerator fake_default =
      ui::Accelerator(ui::VKEY_U, ui::EF_CONTROL_DOWN);
  std::optional<AcceleratorAction> accelerator_id =
      config_->GetIdForDefaultAccelerator(fake_default);
  EXPECT_FALSE(accelerator_id.has_value());
}

TEST_F(AshAcceleratorConfigurationTest, GetAcceleratorsFromActionId) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleMirrorMode},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       AcceleratorAction::kSwapPrimaryDisplay},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, AcceleratorAction::kTakeScreenshot},
      {/*trigger_on_press=*/true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
       AcceleratorAction::kKeyboardBrightnessUp},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       AcceleratorAction::kKeyboardBrightnessUp},
  };
  config_->Initialize(test_data);

  // Create expected id_to_accelerator_data map.
  std::map<AcceleratorActionId, std::vector<AcceleratorData>>
      id_to_accelerator_data;
  for (const auto& data : test_data) {
    id_to_accelerator_data[static_cast<uint32_t>(data.action)].push_back(data);
  }

  // Verify that expected and actual are equal.
  for (const auto& data : test_data) {
    std::vector<AcceleratorData> expected =
        id_to_accelerator_data.at(data.action);
    base::optional_ref<const std::vector<ui::Accelerator>> actual =
        GetAcceleratorsForAction(data.action);
    ASSERT_TRUE(actual.has_value());
    ExpectAllAcceleratorsEqual(expected, *actual);
  }
}

TEST_F(AshAcceleratorConfigurationTest, VerifyObserversAreNotified) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);
  const std::vector<ui::Accelerator>& accelerators =
      config_->GetAllAccelerators();
  ExpectAllAcceleratorsEqual(test_data, accelerators);
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Now update accelerators with a different set of accelerators.
  const AcceleratorData test_data_updated[] = {
      {/*trigger_on_press=*/true, ui::VKEY_J, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleFullscreen},
  };

  config_->Initialize(test_data_updated);
  const std::vector<ui::Accelerator>& accelerators_updated =
      config_->GetAllAccelerators();
  ExpectAllAcceleratorsEqual(test_data_updated, accelerators_updated);
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  // Adding an additional kSwitchToLastUsedIme with trigger_on_press = false
  // here to verify it will be also removed together when removing
  // kSwitchToLastUsedIme with trigger_on_press = true.
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/false, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  // Remove `SWITCH_TO_LAST_USE_IME`.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There should now be an entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `kSwitchToLastUsedIme`.
  const base::Value::List* accelerator_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  // Removing one accelerator in `kSwitchToLastUsedIme` will result in
  // the removed accelerator in the override with `kRemove`.
  EXPECT_EQ(1u, accelerator_overrides->size());
  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          accelerator_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kRemove, override_data.action);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());

  // Attempt to remove the accelerator again, expect to return error.
  AcceleratorConfigResult re_remove_result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kNotFound, re_remove_result);

  // Expect no changes to be made.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAcceleratorIDThatDoesntExist) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Attempt to remove an accelerator with an action ID that doesn't exist.
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kCycleBackwardMru,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kNotFound, result);

  // Nothing should change.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAcceleratorThatDoesntExist) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove an accelerator that doesn't exist, but with an
  // existing action ID. Expect no change.
  AcceleratorConfigResult updated_result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme,
      ui::Accelerator(ui::VKEY_M, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kNotFound, updated_result);

  // Nothing should change.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveDefaultAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kBrightnessDown},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove `SWITCH_TO_LAST_USE_IME`.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  ui::Accelerator removed_accelerator =
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, removed_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // We removed a default accelerator, it should still be cached as a default.
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme,
            config_->GetIdForDefaultAccelerator(removed_accelerator));
  std::vector<ui::Accelerator> default_accelerators =
      config_->GetDefaultAcceleratorsForId(
          AcceleratorAction::kSwitchToLastUsedIme);
  EXPECT_EQ(1u, default_accelerators.size());
  EXPECT_EQ(removed_accelerator, default_accelerators[0]);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAndRestoreDefault) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove `SWITCH_TO_LAST_USE_IME`.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());

  // Restore all defaults.
  result = config_->RestoreAllDefaults();
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Expect accelerators to revert back to the default state and observer
  // is called.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RestoreAllConsecutively) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Restore all defaults, even though no change was made.
  AcceleratorConfigResult reset_result = config_->RestoreAllDefaults();
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, reset_result);

  // Nothing should have changed, but the observer is called.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());

  // Restore all defaults again, even though no change was made.
  reset_result = config_->RestoreAllDefaults();
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, reset_result);

  // Nothing should have changed, but the observer is called.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAndRestoreOneAction) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove `SWITCH_TO_LAST_USE_IME`.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());

  // Reset the `AcceleratorAction::kSwitchToLastUsedIme` action..
  result = config_->RestoreDefault(AcceleratorAction::kSwitchToLastUsedIme);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Expect accelerators to revert back to the default state and observer
  // is called.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());

  // Reset one more time, no changes should be made.
  result = config_->RestoreDefault(AcceleratorAction::kSwitchToLastUsedIme);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // No changes to be made, but observer should be called.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(4, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RestoreInvalidAction) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // `BRIGHTNESS_DOWN` is not a valid accelerator, so this should return an
  // error.
  const AcceleratorConfigResult result =
      config_->RestoreDefault(AcceleratorAction::kBrightnessDown);
  EXPECT_EQ(AcceleratorConfigResult::kNotFound, result);

  // Expect nothing to change.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());
}

// Add accelerator with no conflict.
TEST_F(AshAcceleratorConfigurationTest, AddAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Add CTRL + SPACE to AcceleratorAction::kSwitchToLastUsedIme.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  const ui::Accelerator new_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // adding an accelerator.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);
}

// Add accelerator that conflict with default accelerator.
TEST_F(AshAcceleratorConfigurationTest, AddAcceleratorDefaultConflict) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Add ALT + SHIFT + TAB to AcceleratorAction::kSwitchToLastUsedIme, which
  // conflicts with AcceleratorAction::kCycleBackwardMru.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
  };

  const ui::Accelerator new_accelerator(ui::VKEY_TAB,
                                        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);

  // Confirm that conflicting accelerator was removed.
  base::optional_ref<const std::vector<ui::Accelerator>>
      backward_mru_accelerators =
          GetAcceleratorsForAction(AcceleratorAction::kCycleBackwardMru);
  ASSERT_TRUE(backward_mru_accelerators->empty());
}

// Add accelerator that conflicts with a deprecated accelerator.
TEST_F(AshAcceleratorConfigurationTest, AddAcceleratorDeprecatedConflict) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  const DeprecatedAcceleratorData deprecated_data[] = {
      {AcceleratorAction::kShowTaskManager,
       /*uma_histogram_name=*/"deprecated.showTaskManager",
       /*notification_message_id=*/1,
       /*new_shortcut_id=*/2,
       ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN),
       /*deprecated_enabled=*/true},
  };

  const AcceleratorData test_deprecated_accelerators[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kShowTaskManager},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  const AcceleratorData initial_expected_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kShowTaskManager},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  config_->Initialize(test_data);
  config_->InitializeDeprecatedAccelerators(deprecated_data,
                                            test_deprecated_accelerators);

  ExpectAllAcceleratorsEqual(initial_expected_data,
                             config_->GetAllAccelerators());
  // Initializing deprecated accelerators will also trigger the observer.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());

  const ui::Accelerator deprecated_accelerator(ui::VKEY_ESCAPE,
                                               ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(config_->IsDeprecated(deprecated_accelerator));

  const ui::Accelerator deprecated_accelerator_2(
      ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_TRUE(config_->IsDeprecated(deprecated_accelerator_2));

  // Add SHIFT + ESCAPE to AcceleratorAction::kSwitchToLastUsedIme, which
  // conflicts with a deprecated accelerator.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kShowTaskManager},
  };

  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, deprecated_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(deprecated_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);

  // Confirm that the deprecated accelerator was removed.
  EXPECT_FALSE(config_->IsDeprecated(deprecated_accelerator));

  // Ensure that the second deprecated accelerator still maps to a deprecated
  // action.
  const DeprecatedAcceleratorData* task_manager_deprecated_data =
      config_->GetDeprecatedAcceleratorData(
          AcceleratorAction::kShowTaskManager);
  EXPECT_EQ(AcceleratorAction::kShowTaskManager,
            task_manager_deprecated_data->action);
  EXPECT_TRUE(config_->IsDeprecated(deprecated_accelerator_2));
}

// Add and then remove an accelerator.
TEST_F(AshAcceleratorConfigurationTest, AddRemoveAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Add CTRL + SPACE to AcceleratorAction::kSwitchToLastUsedIme.
  const AcceleratorData added_updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  const ui::Accelerator new_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // adding an accelerator.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(added_updated_test_data,
                             config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);

  // Remove CTRL + SPACE from AcceleratorAction::kSwitchToLastUsedIme.
  const AcceleratorData removed_updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  // Remove the accelerator now.
  result = config_->RemoveAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                                      new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(removed_updated_test_data,
                             config_->GetAllAccelerators());
  EXPECT_FALSE(config_->FindAcceleratorAction(new_accelerator));
}

// Add accelerator and restore its default.
TEST_F(AshAcceleratorConfigurationTest, AddRestoreAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Add CTRL + SPACE to AcceleratorAction::kSwitchToLastUsedIme.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  const ui::Accelerator new_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // adding an accelerator.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);

  // Restore default, expect to be back to default state.
  result = config_->RestoreDefault(AcceleratorAction::kSwitchToLastUsedIme);
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  EXPECT_FALSE(config_->FindAcceleratorAction(new_accelerator));
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, RestoreWithDefaultConflicts) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Add ALT + SHIFT + TAB to AcceleratorAction::kSwitchToLastUsedIme, which
  // conflicts with AcceleratorAction::kCycleBackwardMru.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
  };

  const ui::Accelerator new_accelerator(ui::VKEY_C, ui::EF_ALT_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);

  // Confirm that conflicting accelerator was removed.
  base::optional_ref<const std::vector<ui::Accelerator>>
      forward_mru_accelerators =
          GetAcceleratorsForAction(AcceleratorAction::kCycleForwardMru);
  ASSERT_TRUE(forward_mru_accelerators.has_value());

  EXPECT_EQ(1u, forward_mru_accelerators->size());

  // Now restore the default of `kCycleForwardMru`, this will effectively be a
  // no-opt since one of its default is a used by `kSwitchToLastUsedIme`.
  result = config_->RestoreDefault(AcceleratorAction::kCycleForwardMru);
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());
  EXPECT_EQ(AcceleratorConfigResult::kRestoreSuccessWithConflicts, result);
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
}

// Add an accelerator, then add the same accelerator to another action.
TEST_F(AshAcceleratorConfigurationTest, ReAddAcceleratorToAnotherAction) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Add CTRL + SPACE to AcceleratorAction::kSwitchToLastUsedIme.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  const ui::Accelerator new_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // adding an accelerator.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);

  // Add CTRL + SPACE to AcceleratorAction::kCycleBackwardMru.
  const AcceleratorData reupdated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };
  result = config_->AddUserAccelerator(AcceleratorAction::kCycleBackwardMru,
                                       new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  ExpectAllAcceleratorsEqual(reupdated_test_data,
                             config_->GetAllAccelerators());
  const AcceleratorAction* found_action2 =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action2);
  EXPECT_EQ(AcceleratorAction::kCycleBackwardMru, *found_action2);
}

TEST_F(AshAcceleratorConfigurationTest, ReplaceAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Replace  CTRL + ALT + SPACE -> CTRL + VKEY_M.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  const ui::Accelerator old_accelerator(ui::VKEY_SPACE,
                                        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  const ui::Accelerator new_accelerator(ui::VKEY_M, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result =
      config_->ReplaceAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                                  old_accelerator, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // adding an accelerator.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);
}

TEST_F(AshAcceleratorConfigurationTest, ReplaceNonExistentAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  const ui::Accelerator old_accelerator(ui::VKEY_J, ui::EF_CONTROL_DOWN);
  const ui::Accelerator new_accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN);

  AcceleratorConfigResult result =
      config_->ReplaceAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                                  old_accelerator, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kNotFound, result);

  // Compare expected accelerators and that the observer was fired after
  // adding an accelerator.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, ReplaceThenRestoreAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Replace  CTRL + ALT + SPACE -> CTRL + VKEY_M.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_M, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  const ui::Accelerator old_accelerator(ui::VKEY_SPACE,
                                        ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  const ui::Accelerator new_accelerator(ui::VKEY_M, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result =
      config_->ReplaceAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                                  old_accelerator, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // adding an accelerator.
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(new_accelerator);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme, *found_action);

  // Restore defaults, expect everything to be back to default state.
  config_->RestoreAllDefaults();
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAcceleratorPref) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There should now be an entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* accelerator_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  // Removing one accelerator in `AcceleratorAction::kSwitchToLastUsedIme` will
  // result in one default accelerator remaining.
  EXPECT_EQ(1u, accelerator_overrides->size());
  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          accelerator_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kRemove, override_data.action);

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  // Verify pref overrides were applied correctly.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAcceleratorThenResetAllPref) {
  SimulateNewUserFirstLogin(kFakeUserEmail);

  // Check histogram. There are two counts initially since there has been
  // two separate logins in this test.
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 0, 2);

  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsBeforeResetAll", 1, 0);

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There should now be an entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* accelerator_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  EXPECT_EQ(1u, accelerator_overrides->size());
  // Removing one accelerator in `AcceleratorAction::kSwitchToLastUsedIme` will
  // result in one entry with the `kRemove` tag.
  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          accelerator_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kRemove, override_data.action);

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  // Verify pref overrides were applied correctly.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  // `SimuateUserLogin` triggers the metric twice in tests.
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 1, 2);

  // Now reset all to default.
  result = config_->RestoreAllDefaults();
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  EXPECT_TRUE(GetOverridePref().empty());

  // Relogin, expect shortcuts to be back to default.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& reset_pref_overrides = GetOverridePref();
  EXPECT_TRUE(reset_pref_overrides.empty());
  // `test_data` is the default state of accelerators.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  // Expect increases in the `0` bucket, it gets incremented by 2 due to
  // `SimulateUserLogin`.
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 0, 4);

  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsBeforeResetAll", 1, 1);
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAcceleratorThenResetPref) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There should now be an entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* accelerator_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  // Removing one accelerator in `AcceleratorAction::kSwitchToLastUsedIme` will
  // result in one entry with the `kRemove` tag.
  EXPECT_EQ(1u, accelerator_overrides->size());
  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          accelerator_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kRemove, override_data.action);

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  // Verify pref overrides were applied correctly.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleForwardMru},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kCycleBackwardMru},
  };
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());

  // Now reset the action to default.
  result = config_->RestoreDefault(AcceleratorAction::kSwitchToLastUsedIme);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Relogin, expect shortcuts to be back to default.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& reset_pref_overrides = GetOverridePref();
  EXPECT_TRUE(reset_pref_overrides.empty());
  // `test_data` is the default state of accelerators.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, AddAcceleratorWithPrefs) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 0, 2);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  const ui::Accelerator new_accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There should now be an entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* accelerator_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  // Expect 1 override accelerator for
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  EXPECT_EQ(1u, accelerator_overrides->size());

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          accelerator_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, override_data.action);

  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_EQ(1u, relogin_overrides.size());
  // Verify pref overrides were loaded correctly.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 1, 2);
}

TEST_F(AshAcceleratorConfigurationTest, AddAcceleratorWithConflictWithPrefs) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  // Search + C exists in `AcceleratorAction::kToggleCalendar`, so this should
  // result in adding a new accelerator to
  // `AcceleratorAction::kSwitchToLastUsedIme` and removing an accelerator from
  // `AcceleratorAction::kToggleCalendar`.
  const ui::Accelerator new_accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There two entries in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* switch_to_last_used_ime_overrides =
      updated_overrides.FindList(
          base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));

  // Confirm that prefs are stored correctly.
  EXPECT_EQ(1u, switch_to_last_used_ime_overrides->size());
  AcceleratorModificationData switch_ime_override_data =
      ValueToAcceleratorModificationData(
          switch_to_last_used_ime_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      switch_ime_override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd,
            switch_ime_override_data.action);

  const AcceleratorData expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };
  // `AcceleratorAction::kSwitchToLastUsedIme_data` has all the available
  // accelerators.
  ExpectAllAcceleratorsEqual(expected_test_data, config_->GetAllAccelerators());

  // Lock screen and sign into another user.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_EQ(1u, relogin_overrides.size());

  // Verify pref overrides were applied correctly.
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme,
            *config_->FindAcceleratorAction(new_accelerator));

  // `AcceleratorAction::kSwitchToLastUsedIme_data` has all the available
  // accelerators.
  ExpectAllAcceleratorsEqual(expected_test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest,
       MultipleActionsAddSameAcceleratorWithPrefs) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
  };

  config_->Initialize(test_data);
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 0, 2);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  // Search + C exists in `AcceleratorAction::kToggleCalendar`, so this should
  // result in adding a new accelerator to
  // `AcceleratorAction::kSwitchToLastUsedIme` and removing an accelerator from
  // `AcceleratorAction::kToggleCalendar`.
  const ui::Accelerator new_accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There is one entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* switch_to_last_used_ime_overrides =
      updated_overrides.FindList(
          base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));

  // Confirm that prefs are stored correctly.
  EXPECT_EQ(1u, switch_to_last_used_ime_overrides->size());
  AcceleratorModificationData switch_ime_override_data =
      ValueToAcceleratorModificationData(
          switch_to_last_used_ime_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      switch_ime_override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd,
            switch_ime_override_data.action);

  const AcceleratorData expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
  };
  // `AcceleratorAction::kSwitchToLastUsedIme_data` has all the available
  // accelerators.
  ExpectAllAcceleratorsEqual(expected_test_data, config_->GetAllAccelerators());

  // Now have `AcceleratorAction::kEnableOrToggleDictation` add Search + C,
  // removing it from `AcceleratorAction::kSwitchToLastUsedIme`.
  result = config_->AddUserAccelerator(
      AcceleratorAction::kEnableOrToggleDictation, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  // Expect just one entry, since `AcceleratorAction::kSwitchToLastUsedIme` no
  // longer holds the Search + C accelerator.
  const base::Value::Dict& updated_overrides_2 = GetOverridePref();
  EXPECT_EQ(1u, updated_overrides_2.size());

  const base::Value::List* toggle_dictation_overrides =
      updated_overrides_2.FindList(
          base::NumberToString(AcceleratorAction::kEnableOrToggleDictation));
  // Confirm that prefs are stored correctly.
  EXPECT_EQ(1u, toggle_dictation_overrides->size());
  AcceleratorModificationData toggle_dictation_data =
      ValueToAcceleratorModificationData(
          toggle_dictation_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      toggle_dictation_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, toggle_dictation_data.action);

  const AcceleratorData expected_test_data_2[] = {
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };
  ExpectAllAcceleratorsEqual(expected_test_data_2,
                             config_->GetAllAccelerators());

  // Lock screen and sign into another user.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 0, 4);

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_EQ(1u, relogin_overrides.size());

  // Verify pref overrides were applied correctly.
  EXPECT_EQ(AcceleratorAction::kEnableOrToggleDictation,
            *config_->FindAcceleratorAction(new_accelerator));

  // `AcceleratorAction::kSwitchToLastUsedIme_data` has all the available
  // accelerators.
  ExpectAllAcceleratorsEqual(expected_test_data_2,
                             config_->GetAllAccelerators());
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 1, 2);
}

TEST_F(AshAcceleratorConfigurationTest,
       AddCustomThenReAddAfterRemovedWithPrefs) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
  };

  config_->Initialize(test_data);
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 0, 2);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  // Add a new custom accelerator, Search + Alt + M to `kSwitchToLastUsedIme`.
  const ui::Accelerator new_accelerator(ui::VKEY_M,
                                        ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There is one entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* switch_to_last_used_ime_overrides =
      updated_overrides.FindList(
          base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));

  // Confirm that prefs are stored correctly.
  EXPECT_EQ(1u, switch_to_last_used_ime_overrides->size());
  AcceleratorModificationData switch_ime_override_data =
      ValueToAcceleratorModificationData(
          switch_to_last_used_ime_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators({/*trigger_on_press=*/true, ui::VKEY_M,
                                   ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
                                   AcceleratorAction::kSwitchToLastUsedIme},
                                  switch_ime_override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd,
            switch_ime_override_data.action);

  const AcceleratorData expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_M,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
  };
  ExpectAllAcceleratorsEqual(expected_test_data, config_->GetAllAccelerators());

  // Now have `AcceleratorAction::kEnableOrToggleDictation` add
  // Search + Alt + M, removing it from
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  result = config_->AddUserAccelerator(
      AcceleratorAction::kEnableOrToggleDictation, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  // Expect just one entry, since `AcceleratorAction::kSwitchToLastUsedIme` no
  // longer holds the Search + Alt + M accelerator.
  const base::Value::Dict& updated_overrides_2 = GetOverridePref();
  EXPECT_EQ(1u, updated_overrides_2.size());

  const base::Value::List* toggle_dictation_overrides =
      updated_overrides_2.FindList(
          base::NumberToString(AcceleratorAction::kEnableOrToggleDictation));
  // Confirm that prefs are stored correctly.
  EXPECT_EQ(1u, toggle_dictation_overrides->size());
  AcceleratorModificationData toggle_dictation_data =
      ValueToAcceleratorModificationData(
          toggle_dictation_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators({/*trigger_on_press=*/true, ui::VKEY_M,
                                   ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
                                   AcceleratorAction::kEnableOrToggleDictation},
                                  toggle_dictation_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, toggle_dictation_data.action);

  const AcceleratorData expected_test_data_2[] = {
      {/*trigger_on_press=*/true, ui::VKEY_M,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
  };
  ExpectAllAcceleratorsEqual(expected_test_data_2,
                             config_->GetAllAccelerators());

  // Re-add a new custom accelerator, Search + Alt + M to
  // `kSwitchToLastUsedIme`.
  result = config_->AddUserAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                                       new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides_3 = GetOverridePref();
  // There is one entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides_3.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* switch_to_last_used_ime_overrides_2 =
      updated_overrides_3.FindList(
          base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));

  // Confirm that prefs are stored correctly.
  EXPECT_EQ(1u, switch_to_last_used_ime_overrides_2->size());
  switch_ime_override_data = ValueToAcceleratorModificationData(
      switch_to_last_used_ime_overrides_2->front().GetDict());
  EXPECT_TRUE(CompareAccelerators({/*trigger_on_press=*/true, ui::VKEY_M,
                                   ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
                                   AcceleratorAction::kSwitchToLastUsedIme},
                                  switch_ime_override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd,
            switch_ime_override_data.action);

  const AcceleratorData expected_test_data_3[] = {
      {/*trigger_on_press=*/true, ui::VKEY_M,
       ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kEnableOrToggleDictation},
  };
  ExpectAllAcceleratorsEqual(expected_test_data_3,
                             config_->GetAllAccelerators());
  // Lock screen and sign into another user.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_EQ(1u, relogin_overrides.size());

  // Verify pref overrides were applied correctly.
  EXPECT_EQ(AcceleratorAction::kSwitchToLastUsedIme,
            *config_->FindAcceleratorAction(new_accelerator));

  // `AcceleratorAction::kSwitchToLastUsedIme_data` has all the available
  // accelerators.
  ExpectAllAcceleratorsEqual(expected_test_data_3,
                             config_->GetAllAccelerators());
  histogram_tester_->ExpectBucketCount(
      "Ash.ShortcutCustomization.CustomizationsLoadedOnStartup", 1, 2);
}

TEST_F(AshAcceleratorConfigurationTest, RemoveThenAddAcceleratorWithPrefs) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  // Remove Ctrl + space from `AcceleratorAction::kSwitchToLastUsedIme`.
  ui::Accelerator new_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Now add Ctrl + Space to `AcceleratorAction::kToggleCalendar`.
  result = config_->AddUserAccelerator(AcceleratorAction::kToggleCalendar,
                                       new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  const base::Value::Dict& remove_add_overrides = GetOverridePref();
  EXPECT_EQ(2u, remove_add_overrides.size());

  // Verify prefs are populated correctly.
  const base::Value::Dict& updated_overrides = GetOverridePref();
  const base::Value::List* last_used_ime_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          last_used_ime_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kRemove, override_data.action);

  // Now verify add pref is present.
  const base::Value::List* toggle_calendar_overrides =
      updated_overrides.FindList(
          base::NumberToString(AcceleratorAction::kToggleCalendar));
  override_data = ValueToAcceleratorModificationData(
      toggle_calendar_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleCalendar},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, override_data.action);

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kToggleCalendar},
  };
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_EQ(2u, relogin_overrides.size());
  // Verify pref overrides were loaded correctly.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, ReplaceAcceleratorWithPrefs) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  // Replace Ctrl + Space with Meta + A in
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  ui::Accelerator old_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  ui::Accelerator new_accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN);
  AcceleratorConfigResult result =
      config_->ReplaceAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                                  old_accelerator, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Verify prefs are populated correctly.
  const base::Value::Dict& updated_overrides = GetOverridePref();
  const base::Value::List* last_used_ime_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  AcceleratorModificationData remove_override_data =
      ValueToAcceleratorModificationData(
          last_used_ime_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      remove_override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kRemove,
            remove_override_data.action);

  AcceleratorModificationData add_override_data =
      ValueToAcceleratorModificationData(
          last_used_ime_overrides->back().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      add_override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, add_override_data.action);

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_C, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kToggleCalendar},
  };
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_EQ(1u, relogin_overrides.size());
  // Verify pref overrides were loaded correctly.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, IgnoreBadActionIdPrefs) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  // Simulate setting a pref with bad values (invalid action_id).
  const ui::Accelerator bad_accelerator(ui::VKEY_B, ui::EF_ALT_DOWN);
  SetOverridePref(bad_accelerator, AcceleratorModificationAction::kAdd,
                  /*action_id*/ 7777777);

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile, expect that no prefs are available
  // since the bad pref should've been removed.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_TRUE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_TRUE(relogin_overrides.empty());
  // Verify pref overrides were loaded correctly.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, IgnoreBadAcceleratorPrefs) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  // Simulate setting a pref with bad values (invalid action_id).
  const ui::Accelerator bad_accelerator(ui::VKEY_B, ui::EF_ALT_DOWN);
  SetOverridePref(bad_accelerator, AcceleratorModificationAction::kRemove,
                  kSwitchToLastUsedIme);

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile, expect that no prefs are available
  // since the bad pref should've been removed.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_TRUE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_TRUE(relogin_overrides.empty());
  // Verify pref overrides were loaded correctly.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, AddAcceleratorWithPrefReleasedState) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  const ui::Accelerator released_accelerator(
      ui::VKEY_A, ui::EF_COMMAND_DOWN, ui::Accelerator::KeyState::RELEASED);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, released_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const ui::Accelerator pressed_accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN,
                                            ui::Accelerator::KeyState::PRESSED);
  result = config_->AddUserAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                                       pressed_accelerator);

  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There should now be an entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* accelerator_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  // Expect 2 overrides accelerator for
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  EXPECT_EQ(2u, accelerator_overrides->size());

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/false, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          accelerator_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/false, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, override_data.action);

  override_data = ValueToAcceleratorModificationData(
      accelerator_overrides->back().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, override_data.action);

  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_EQ(1u, relogin_overrides.size());
  // Verify pref overrides were loaded correctly.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, SwitchUserPrefsAreSeparate) {
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_D,
       ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
       AcceleratorAction::kMagnifierZoomIn},
  };

  config_->Initialize(test_data);

  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  const ui::Accelerator new_accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kMagnifierZoomIn, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There should now be an entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* accelerator_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kMagnifierZoomIn));
  // Expect 1 override accelerator for
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  EXPECT_EQ(1u, accelerator_overrides->size());

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_D,
       ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
       AcceleratorAction::kMagnifierZoomIn},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kMagnifierZoomIn},
  };

  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          accelerator_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kMagnifierZoomIn},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, override_data.action);

  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Expect the second user to have all defaults.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());

  // Now re-login to the original profile.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_FALSE(original_pref_overrides.empty());

  const base::Value::Dict& relogin_overrides = GetOverridePref();
  EXPECT_EQ(1u, relogin_overrides.size());
  // Verify pref overrides were loaded correctly.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, PrefsResetWithFlag) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{::features::kShortcutCustomization,
                            features::kResetShortcutCustomizations},
      /*disabled_features=*/{});
  SimulateNewUserFirstLogin(kFakeUserEmail);
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);
  // Expect that there are no entries stored in the override pref.
  const base::Value::Dict& pref_overrides = GetOverridePref();
  EXPECT_TRUE(pref_overrides.empty());

  const ui::Accelerator new_accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN);
  AcceleratorConfigResult result = config_->AddUserAccelerator(
      AcceleratorAction::kSwitchToLastUsedIme, new_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  const base::Value::Dict& updated_overrides = GetOverridePref();
  // There should now be an entry in the pref overrides.
  EXPECT_EQ(1u, updated_overrides.size());
  // Expect the pref to have one entry that has the key of
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  const base::Value::List* accelerator_overrides = updated_overrides.FindList(
      base::NumberToString(AcceleratorAction::kSwitchToLastUsedIme));
  // Expect 1 override accelerator for
  // `AcceleratorAction::kSwitchToLastUsedIme`.
  EXPECT_EQ(1u, accelerator_overrides->size());

  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  AcceleratorModificationData override_data =
      ValueToAcceleratorModificationData(
          accelerator_overrides->front().GetDict());
  EXPECT_TRUE(CompareAccelerators(
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_COMMAND_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
      override_data.accelerator));
  EXPECT_EQ(AcceleratorModificationAction::kAdd, override_data.action);

  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());

  // Simulate login on another account, expect the pref to not be present.
  GetSessionControllerClient()->LockScreen();
  SimulateNewUserFirstLogin(kFakeUserEmail2);
  const base::Value::Dict& other_user_pref_overrides = GetOverridePref();
  EXPECT_TRUE(other_user_pref_overrides.empty());

  // Now re-login to the original profile. Since #reset-shortcut-customizations
  // is enabled, expect that no prefs were saved.
  GetSessionControllerClient()->LockScreen();
  config_->Initialize(test_data);
  SimulateUserLogin(kFakeUserEmail);
  const base::Value::Dict& original_pref_overrides = GetOverridePref();
  EXPECT_TRUE(original_pref_overrides.empty());
}

TEST_F(AshAcceleratorConfigurationTest, FindAcceleratorActionPositionalKeys) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_OEM_4, ui::EF_ALT_DOWN,
       AcceleratorAction::kWindowCycleSnapLeft},
  };

  config_->Initialize(test_data);
  config_->SetUsePositionalLookup(/*use_positional_lookup=*/true);

  // The the DE-de layout, alt + bracket left is mapped to alt + VKEY_OEM_1.
  // Performing a lookup should be able to remap the lookup correctly from
  // VKEY_OEM_1 -> VKEY_OEM_4.
  const ui::Accelerator de_alt_left_bracket(
      ui::VKEY_OEM_1, ui::DomCode::BRACKET_LEFT, ui::EF_ALT_DOWN);
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(de_alt_left_bracket);
  EXPECT_TRUE(found_action);
  EXPECT_EQ(AcceleratorAction::kWindowCycleSnapLeft, *found_action);

  // Now reset all accelerators and still expect the lookup to succeed.
  config_->RestoreAllDefaults();
  const AcceleratorAction* found_action2 =
      config_->FindAcceleratorAction(de_alt_left_bracket);
  EXPECT_TRUE(found_action2);
  EXPECT_EQ(AcceleratorAction::kWindowCycleSnapLeft, *found_action2);
}

TEST_F(AshAcceleratorConfigurationTest,
       FindAcceleratorActionPositionalDisabledKeys) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_OEM_4, ui::EF_ALT_DOWN,
       AcceleratorAction::kWindowCycleSnapLeft},
  };

  config_->Initialize(test_data);
  config_->SetUsePositionalLookup(/*use_positional_lookup=*/false);

  // The inputted accelerator here will not be positionally remapped.
  const ui::Accelerator de_alt_left_bracket(
      ui::VKEY_OEM_1, ui::DomCode::BRACKET_LEFT, ui::EF_ALT_DOWN);
  const AcceleratorAction* found_action =
      config_->FindAcceleratorAction(de_alt_left_bracket);
  EXPECT_FALSE(found_action);
}

}  // namespace ash
