// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <memory>
#include <string>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/accelerator_configuration.mojom.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::ash::mojom::AcceleratorConfigResult;

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

bool CompareAccelerators(const ash::AcceleratorData& expected_data,
                         const ui::Accelerator& actual_accelerator) {
  ui::Accelerator expected_accel(expected_data.keycode,
                                 expected_data.modifiers);
  return expected_accel.key_code() == actual_accelerator.key_code() &&
         expected_accel.modifiers() == actual_accelerator.modifiers();
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

class AshAcceleratorConfigurationTest : public testing::Test {
 public:
  AshAcceleratorConfigurationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kShortcutCustomization);
    config_ = std::make_unique<AshAcceleratorConfiguration>();
    config_->AddObserver(&observer_);
  }

  ~AshAcceleratorConfigurationTest() override {
    config_->RemoveObserver(&observer_);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  UpdatedAcceleratorsObserver observer_;
  std::unique_ptr<AshAcceleratorConfiguration> config_;
};

TEST_F(AshAcceleratorConfigurationTest, VerifyAcceleratorMappingPopulated) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  config_->Initialize(test_data);
  const std::vector<ui::Accelerator>& accelerators =
      config_->GetAllAccelerators();

  ExpectAllAcceleratorsEqual(test_data, accelerators);
}

TEST_F(AshAcceleratorConfigurationTest, DeprecatedAccelerators) {
  // Test deprecated accelerators, in this case `SHOW_TASK_MANAGER` has two
  // associated accelerators: (deprecated) ESCAPE + SHIFT and
  // (active) ESCAPE + COMMAND.
  const AcceleratorData initial_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
  };

  const AcceleratorData expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       SHOW_TASK_MANAGER},
  };

  const DeprecatedAcceleratorData deprecated_data[] = {
      {SHOW_TASK_MANAGER, /*uma_histogram_name=*/"deprecated.showTaskManager",
       /*notification_message_id=*/1, /*old_shortcut_id=*/1,
       /*new_shortcut_id=*/2, /*deprecated_enabled=*/true},
  };

  const AcceleratorData test_deprecated_accelerators[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       SHOW_TASK_MANAGER},
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
  EXPECT_EQ(deprecated_data,
            config_->GetDeprecatedAcceleratorData(SHOW_TASK_MANAGER));
  // CYCLE_BACKWARD_MRU is not a deprecated action, expect nullptr.
  EXPECT_EQ(nullptr, config_->GetDeprecatedAcceleratorData(CYCLE_BACKWARD_MRU));

  // Verify that ESCAPE + COMMAND is not deprecated.
  const ui::Accelerator active_accelerator(ui::VKEY_ESCAPE,
                                           ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(config_->IsDeprecated(active_accelerator));
}

TEST_F(AshAcceleratorConfigurationTest,
       RemoveAndRestoreDeprecatedAccelerators) {
  // Test deprecated accelerators, in this case `SHOW_TASK_MANAGER` has two
  // associated accelerators: (deprecated) ESCAPE + SHIFT and
  // (active) ESCAPE + COMMAND.
  const AcceleratorData initial_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
  };

  const AcceleratorData expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       SHOW_TASK_MANAGER},
  };

  const DeprecatedAcceleratorData deprecated_data[] = {
      {SHOW_TASK_MANAGER, /*uma_histogram_name=*/"deprecated.showTaskManager",
       /*notification_message_id=*/1, /*old_shortcut_id=*/1,
       /*new_shortcut_id=*/2, /*deprecated_enabled=*/true},
  };

  const AcceleratorData test_deprecated_accelerators[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       SHOW_TASK_MANAGER},
  };

  config_->Initialize(initial_test_data);
  config_->InitializeDeprecatedAccelerators(deprecated_data,
                                            test_deprecated_accelerators);

  const ui::Accelerator deprecated_accelerator(ui::VKEY_ESCAPE,
                                               ui::EF_SHIFT_DOWN);

  // Remove the deprecated accelerator ESCAPE + SHIFT.
  const AcceleratorData updated_expected_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
  };
  AcceleratorConfigResult result =
      config_->RemoveAccelerator(SHOW_TASK_MANAGER, deprecated_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  // Verify that the accelerator is no longer deprecated.
  EXPECT_FALSE(config_->IsDeprecated(deprecated_accelerator));
  EXPECT_FALSE(config_->GetDeprecatedAcceleratorData(SHOW_TASK_MANAGER));
  ExpectAllAcceleratorsEqual(updated_expected_test_data,
                             config_->GetAllAccelerators());

  // Attempt to restore SHOW_TASK_MANAGER, expect deprecated accelerator to NOT
  // be re-added.
  result = config_->RestoreDefault(SHOW_TASK_MANAGER);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  EXPECT_FALSE(config_->IsDeprecated(deprecated_accelerator));
  EXPECT_FALSE(config_->GetDeprecatedAcceleratorData(SHOW_TASK_MANAGER));
  ExpectAllAcceleratorsEqual(updated_expected_test_data,
                             config_->GetAllAccelerators());

  // Now restore all accelerators, this time deprecated accelerators should be
  // restored.
  result = config_->RestoreAllDefaults();
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);
  EXPECT_TRUE(config_->IsDeprecated(deprecated_accelerator));
  EXPECT_EQ(deprecated_data,
            config_->GetDeprecatedAcceleratorData(SHOW_TASK_MANAGER));
  ExpectAllAcceleratorsEqual(expected_test_data, config_->GetAllAccelerators());
}

TEST_F(AshAcceleratorConfigurationTest, IsDefaultAccelerator) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       TOGGLE_MIRROR_MODE},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       SWAP_PRIMARY_DISPLAY},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT},
      {/*trigger_on_press=*/true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
       KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_UP},
  };

  // `Initialize()` sets up the default accelerators.
  config_->Initialize(test_data);
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());

  // Verify that Control + Zoom is the default for TOGGLE_MIRROR_MODE.
  ui::Accelerator expected_default =
      ui::Accelerator(ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN);
  absl::optional<AcceleratorAction> accelerator_id =
      config_->GetIdForDefaultAccelerator(expected_default);
  EXPECT_TRUE(accelerator_id.has_value());
  EXPECT_EQ(TOGGLE_MIRROR_MODE, accelerator_id.value());
  std::vector<ui::Accelerator> default_accelerators =
      config_->GetDefaultAcceleratorsForId(TOGGLE_MIRROR_MODE);
  EXPECT_EQ(1u, default_accelerators.size());
  EXPECT_EQ(expected_default, default_accelerators[0]);
}

TEST_F(AshAcceleratorConfigurationTest, MultipleDefaultAccelerators) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       TOGGLE_MIRROR_MODE},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       TOGGLE_MIRROR_MODE},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT},
      {/*trigger_on_press=*/true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
       KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_UP},
  };

  // `Initialize()` sets up the default accelerators.
  config_->Initialize(test_data);
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());

  // Verify that Control + Zoom and Alt + Zoom are defaults for
  // TOGGLE_MIRROR_MODE.
  ui::Accelerator expected_default =
      ui::Accelerator(ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN);
  ui::Accelerator expected_default_2 =
      ui::Accelerator(ui::VKEY_ZOOM, ui::EF_ALT_DOWN);

  absl::optional<AcceleratorAction> accelerator_id =
      config_->GetIdForDefaultAccelerator(expected_default);
  EXPECT_TRUE(accelerator_id.has_value());
  EXPECT_EQ(TOGGLE_MIRROR_MODE, accelerator_id.value());

  accelerator_id = config_->GetIdForDefaultAccelerator(expected_default_2);
  EXPECT_TRUE(accelerator_id.has_value());
  EXPECT_EQ(TOGGLE_MIRROR_MODE, accelerator_id.value());

  std::vector<ui::Accelerator> default_accelerators =
      config_->GetDefaultAcceleratorsForId(TOGGLE_MIRROR_MODE);

  EXPECT_EQ(2u, default_accelerators.size());

  EXPECT_TRUE(base::Contains(default_accelerators, expected_default));
  EXPECT_TRUE(base::Contains(default_accelerators, expected_default_2));
}
TEST_F(AshAcceleratorConfigurationTest, DefaultNotFound) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       TOGGLE_MIRROR_MODE},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       SWAP_PRIMARY_DISPLAY},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT},
      {/*trigger_on_press=*/true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
       KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_UP},
  };

  // `Initialize()` sets up the default accelerators.
  config_->Initialize(test_data);
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());

  // Verify that Ctrl + U is not a default accelerator in this test set.
  ui::Accelerator fake_default =
      ui::Accelerator(ui::VKEY_U, ui::EF_CONTROL_DOWN);
  absl::optional<AcceleratorAction> accelerator_id =
      config_->GetIdForDefaultAccelerator(fake_default);
  EXPECT_FALSE(accelerator_id.has_value());
}

TEST_F(AshAcceleratorConfigurationTest, GetAcceleratorsFromActionId) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN,
       TOGGLE_MIRROR_MODE},
      {/*trigger_on_press=*/true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN,
       SWAP_PRIMARY_DISPLAY},
      {/*trigger_on_press=*/true, ui::VKEY_MEDIA_LAUNCH_APP1,
       ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT},
      {/*trigger_on_press=*/true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE,
       KEYBOARD_BRIGHTNESS_UP},
      {/*trigger_on_press=*/true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN,
       KEYBOARD_BRIGHTNESS_UP},
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
    std::vector<ui::Accelerator> actual =
        config_->GetAcceleratorsForAction(data.action);
    ExpectAllAcceleratorsEqual(expected, actual);
  }
}

TEST_F(AshAcceleratorConfigurationTest, VerifyObserversAreNotified) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  config_->Initialize(test_data);
  const std::vector<ui::Accelerator>& accelerators =
      config_->GetAllAccelerators();
  ExpectAllAcceleratorsEqual(test_data, accelerators);
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Now update accelerators with a different set of accelerators.
  const AcceleratorData test_data_updated[] = {
      {/*trigger_on_press=*/true, ui::VKEY_J, ui::EF_CONTROL_DOWN,
       TOGGLE_FULLSCREEN},
  };

  config_->Initialize(test_data_updated);
  const std::vector<ui::Accelerator>& accelerators_updated =
      config_->GetAllAccelerators();
  ExpectAllAcceleratorsEqual(test_data_updated, accelerators_updated);
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAccelerator) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove `SWITCH_TO_LAST_USE_IME`.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      SWITCH_TO_LAST_USED_IME,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());

  // Attempt to remove the accelerator again, expect to return error.
  AcceleratorConfigResult re_remove_result = config_->RemoveAccelerator(
      SWITCH_TO_LAST_USED_IME,
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
       SWITCH_TO_LAST_USED_IME},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Attempt to remove an accelerator with an action ID that doesn't exist.
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      CYCLE_BACKWARD_MRU, ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kNotFound, result);

  // Nothing should change.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RemoveAcceleratorThatDoesntExist) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       SWITCH_TO_LAST_USED_IME},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove an accelerator that doesn't exist, but with an
  // existing action ID. Expect no change.
  AcceleratorConfigResult updated_result = config_->RemoveAccelerator(
      SWITCH_TO_LAST_USED_IME,
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
       SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, BRIGHTNESS_DOWN},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove `SWITCH_TO_LAST_USE_IME`.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  ui::Accelerator removed_accelerator =
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  AcceleratorConfigResult result =
      config_->RemoveAccelerator(SWITCH_TO_LAST_USED_IME, removed_accelerator);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // We removed a default accelerator, it should still be cached as a default.
  EXPECT_EQ(SWITCH_TO_LAST_USED_IME,
            config_->GetIdForDefaultAccelerator(removed_accelerator));
  std::vector<ui::Accelerator> default_accelerators =
      config_->GetDefaultAcceleratorsForId(SWITCH_TO_LAST_USED_IME);
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
       SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove `SWITCH_TO_LAST_USE_IME`.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      SWITCH_TO_LAST_USED_IME,
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
       SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
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
       SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // Remove `SWITCH_TO_LAST_USE_IME`.
  const AcceleratorData updated_test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };
  AcceleratorConfigResult result = config_->RemoveAccelerator(
      SWITCH_TO_LAST_USED_IME,
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN));
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Compare expected accelerators and that the observer was fired after
  // removing an accelerator.
  ExpectAllAcceleratorsEqual(updated_test_data, config_->GetAllAccelerators());
  EXPECT_EQ(2, observer_.num_times_accelerator_updated_called());

  // Reset the `SWITCH_TO_LAST_USED_IME` action..
  result = config_->RestoreDefault(SWITCH_TO_LAST_USED_IME);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // Expect accelerators to revert back to the default state and observer
  // is called.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(3, observer_.num_times_accelerator_updated_called());

  // Reset one more time, no changes should be made.
  result = config_->RestoreDefault(SWITCH_TO_LAST_USED_IME);
  EXPECT_EQ(AcceleratorConfigResult::kSuccess, result);

  // No changes to be made, but observer should be called.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(4, observer_.num_times_accelerator_updated_called());
}

TEST_F(AshAcceleratorConfigurationTest, RestoreInvalidAction) {
  EXPECT_EQ(0, observer_.num_times_accelerator_updated_called());
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/*trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/*trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  config_->Initialize(test_data);

  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());

  // `BRIGHTNESS_DOWN` is not a valid accelerator, so this should return an
  // error.
  const AcceleratorConfigResult result =
      config_->RestoreDefault(BRIGHTNESS_DOWN);
  EXPECT_EQ(AcceleratorConfigResult::kNotFound, result);

  // Expect nothing to change.
  ExpectAllAcceleratorsEqual(test_data, config_->GetAllAccelerators());
  EXPECT_EQ(1, observer_.num_times_accelerator_updated_called());
}

}  // namespace ash
