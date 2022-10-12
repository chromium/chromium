// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <memory>
#include <string>

#include "ash/accelerators/accelerator_layout_table.h"
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
                         const ash::AcceleratorInfo& actual_info) {
  ui::Accelerator expected_accel(expected_data.keycode,
                                 expected_data.modifiers);
  ash::AcceleratorInfo expected_info(
      actual_info.type, expected_accel,
      ash::KeycodeToKeyString(expected_data.keycode),
      /*locked=*/true);

  const bool type_equals = expected_info.type == actual_info.type;
  const bool accelerator_equals =
      expected_info.accelerator == actual_info.accelerator;
  const bool key_display_equals =
      expected_info.key_display == actual_info.key_display;
  const bool locked_equals = expected_info.locked == actual_info.locked;
  return type_equals && accelerator_equals && key_display_equals &&
         locked_equals;
}

void ExpectAllAcceleratorsEqual(
    const base::span<const ash::AcceleratorData> expected,
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

std::vector<ash::AcceleratorInfo> GetDeprecatedAcceleratorInfos(
    const std::vector<ash::AcceleratorInfo>& infos) {
  std::vector<ash::AcceleratorInfo> deprecated_infos;
  for (const auto& info : infos) {
    if (info.type == ash::mojom::AcceleratorType::kDeprecated) {
      deprecated_infos.push_back(info);
    }
  }
  return deprecated_infos;
}

// Validates that the passed in layouts have matching accelerator layouts in
// `kAcceleratorLayouts`. If this throws an expectation error it means that
// the there is a inconsistency between the layouts in `kAcceleratorLayouts` and
// the data provided by `AshAcceleratorConfiguration`.
void ValidateAcceleratorLayouts(
    const std::vector<ash::mojom::AcceleratorLayoutInfoPtr>& actual_layouts) {
  for (const auto& actual : actual_layouts) {
    EXPECT_TRUE(ash::kAcceleratorLayouts.contains(actual->action));

    const ash::AcceleratorLayoutDetails& expected =
        ash::kAcceleratorLayouts.at(actual->action);

    EXPECT_EQ(expected.category, actual->category);
    EXPECT_EQ(expected.sub_category, actual->sub_category);
    EXPECT_EQ(expected.layout_style, actual->style);
    EXPECT_EQ(ash::mojom::AcceleratorSource::kAsh, actual->source);
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
      {/**trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       SWITCH_TO_LAST_USED_IME},
      {/**trigger_on_press=*/true, ui::VKEY_SPACE,
       ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, SWITCH_TO_LAST_USED_IME},
      {/**trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
  };

  config_->Initialize(test_data);
  const std::vector<AcceleratorInfo> infos = config_->GetAllAcceleratorInfos();

  ExpectAllAcceleratorsEqual(test_data, infos);
}

TEST_F(AshAcceleratorConfigurationTest, DeprecatedAccelerators) {
  // Test deprecated accelerators, in this case `SHOW_TASK_MANAGER` has two
  // associated accelerators: (deprecated) ESCAPE + SHIFT and
  // (active) ESCAPE + COMMAND.
  const AcceleratorData initial_test_data[] = {
      {/**trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
  };

  const AcceleratorData expected_test_data[] = {
      {/**trigger_on_press=*/true, ui::VKEY_TAB, ui::EF_ALT_DOWN,
       CYCLE_FORWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_TAB,
       ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
      {/**trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN,
       SHOW_TASK_MANAGER},
      {/**trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       SHOW_TASK_MANAGER},
  };

  const DeprecatedAcceleratorData deprecated_data[] = {
      {SHOW_TASK_MANAGER, /**uma_histogram_name=*/"deprecated.showTaskManager",
       /**notification_message_id=*/1, /*old_shortcut_id=*/1,
       /**new_shortcut_id=*/2, /**deprecated_enabled=*/true},
  };

  const AcceleratorData test_deprecated_accelerators[] = {
      {/**trigger_on_press=*/true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN,
       SHOW_TASK_MANAGER},
  };

  config_->Initialize(initial_test_data);
  config_->InitializeDeprecatedAccelerators(deprecated_data,
                                            test_deprecated_accelerators);

  const std::vector<AcceleratorInfo> infos = config_->GetAllAcceleratorInfos();
  // When initializing deprecated accelerators, expect them to be added to the
  // overall accelerators list too.
  ExpectAllAcceleratorsEqual(expected_test_data, infos);

  // Verify that the fetch deprecated infos are correct.
  const std::vector<AcceleratorInfo>& deprecated_infos =
      GetDeprecatedAcceleratorInfos(infos);
  ExpectAllAcceleratorsEqual(test_deprecated_accelerators, deprecated_infos);

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

TEST_F(AshAcceleratorConfigurationTest, ValidateAllAcceleratorLayouts) {
  // Initialize with all default accelerators.
  config_->Initialize();
  const std::vector<mojom::AcceleratorLayoutInfoPtr>& actual_layouts =
      config_->GetAcceleratorLayoutInfos();

  // Verify that all default accelerators have the correctly mapped layout
  // details.
  ValidateAcceleratorLayouts(actual_layouts);
}

TEST_F(AshAcceleratorConfigurationTest,
       ValidatekAcceleratorActionToStringIdMap) {
  // Initialize with all default accelerators.
  config_->Initialize();
  const std::vector<mojom::AcceleratorLayoutInfoPtr>& layouts =
      config_->GetAcceleratorLayoutInfos();

  for (const auto& layout : layouts) {
    // kAcceleratorActionToStringIdMap should contain all actions in
    // AcceleratorAction. Adding a new accelerator must add a new entry to this
    // map.
    EXPECT_TRUE(ash::kAcceleratorActionToStringIdMap.contains(layout->action))
        << "Unknown accelerator action id: " << layout->action;
  }
}
}  // namespace ash
