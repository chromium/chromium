// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <memory>
#include <string>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accelerators_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
    config_ = std::make_unique<AshAcceleratorConfiguration>();
  }

  ~AshAcceleratorConfigurationTest() override = default;

 protected:
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

  // Verify that the fetch deprecated accelerators are correct.
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

}  // namespace ash
