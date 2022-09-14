// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_accelerator_configuration.h"

#include <memory>
#include <string>

#include "ash/public/cpp/accelerators.h"
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
  ash::AcceleratorInfo expected_info(ash::mojom::AcceleratorType::kDefault,
                                     expected_accel,
                                     /**locked=*/true);

  const bool type_equals = expected_info.type == actual_info.type;
  const bool accelerator_equals =
      expected_info.accelerator == actual_info.accelerator;
  const bool locked_equals = expected_info.locked == actual_info.locked;
  return type_equals && accelerator_equals && locked_equals;
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
      {true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN, SWITCH_TO_LAST_USED_IME},
      {true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
       SWITCH_TO_LAST_USED_IME},
      {true, ui::VKEY_TAB, ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU},
      {true, ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
       CYCLE_BACKWARD_MRU},
  };

  config_->InitializeAcceleratorMapping(test_data);
  const std::vector<AcceleratorInfo> infos = config_->GetAllAcceleratorInfos();
  EXPECT_EQ(std::size(test_data), infos.size());

  for (const auto& expected : test_data) {
    const std::vector<AcceleratorInfo> actual_configs =
        config_->GetConfigForAction(expected.action);
    bool found_match = false;
    for (const auto& actual : actual_configs) {
      found_match = CompareAccelerators(expected, actual);
      if (found_match) {
        break;
      }
    }
    EXPECT_TRUE(found_match);
  }
}

}  // namespace ash
