// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"

#include <cstddef>

#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/test/ash_test_util.h"
#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace ash {

namespace {

// The total number of Ash accelerators.
constexpr int kAshAcceleratorsTotalNum = 160;
// The hash of Ash accelerators.
constexpr char kAshAcceleratorsHash[] =
    "7c9f5d090e6be1c01bcfca53b67a79956737dbba149b4e683b44c6ace07e509d";

std::string ToActionName(ash::AcceleratorAction action) {
  return base::StrCat(
      {"AcceleratorAction::k", GetAcceleratorActionName(action)});
}

const char* BooleanToString(bool value) {
  return value ? "true" : "false";
}

std::string ModifiersToString(int modifiers) {
  return base::StringPrintf("shift=%s control=%s alt=%s search=%s",
                            BooleanToString(modifiers & ui::EF_SHIFT_DOWN),
                            BooleanToString(modifiers & ui::EF_CONTROL_DOWN),
                            BooleanToString(modifiers & ui::EF_ALT_DOWN),
                            BooleanToString(modifiers & ui::EF_COMMAND_DOWN));
}

std::string AshAcceleratorDataToString(
    const ash::AcceleratorData& accelerator) {
  return base::StringPrintf("trigger_on_press=%s keycode=%d action=%d ",
                            BooleanToString(accelerator.trigger_on_press),
                            accelerator.keycode, accelerator.action) +
         ModifiersToString(accelerator.modifiers);
}

struct AshAcceleratorDataCmp {
  bool operator()(const ash::AcceleratorData& lhs,
                  const ash::AcceleratorData& rhs) {
    return std::tie(lhs.trigger_on_press, lhs.keycode, lhs.modifiers) <
           std::tie(rhs.trigger_on_press, rhs.keycode, rhs.modifiers);
  }
};

class AcceleratorLayoutMetadataTest : public testing::Test {
 public:
  AcceleratorLayoutMetadataTest() = default;

  AcceleratorLayoutMetadataTest(const AcceleratorLayoutMetadataTest&) = delete;
  AcceleratorLayoutMetadataTest& operator=(
      const AcceleratorLayoutMetadataTest&) = delete;

  ~AcceleratorLayoutMetadataTest() override = default;

  void SetUp() override {
    for (const auto& layout_id : ash::kAcceleratorLayouts) {
      const std::optional<AcceleratorLayoutDetails> layout =
          GetAcceleratorLayout(layout_id);
      ASSERT_TRUE(layout.has_value());
      if (layout->source == mojom::AcceleratorSource::kAsh) {
        ash_accelerator_with_layouts_.insert(
            static_cast<ash::AcceleratorAction>(layout->action_id));
      }
    }

    testing::Test::SetUp();
  }

 protected:
  bool ShouldNotHaveLayouts(ash::AcceleratorAction action) {
    return base::Contains(kAshAcceleratorsWithoutLayout, action);
  }

  bool HasLayouts(ash::AcceleratorAction action) {
    return base::Contains(ash_accelerator_with_layouts_, action);
  }

  // Ash accelerator with layouts.
  std::set<ash::AcceleratorAction> ash_accelerator_with_layouts_;
};

}  // namespace

// Test that all ash accelerators should have a layout or should be added to the
// exception list kAshAcceleratorsWithoutLayout.
TEST_F(AcceleratorLayoutMetadataTest,
       AshAcceleratorsNotInAllowedListShouldHaveLayouts) {
  for (const ash::AcceleratorData& accel_data : ash::kAcceleratorData) {
    if (ShouldNotHaveLayouts(accel_data.action)) {
      EXPECT_FALSE(HasLayouts(accel_data.action))
          << ToActionName(accel_data.action)
          << " has layouts. Please remove it from "
             "kAshAcceleratorsWithoutLayout in "
             "ash/webui/shortcut_customization_ui/backend/"
             "accelerator_layout_table.h.";
    } else {
      EXPECT_TRUE(HasLayouts(accel_data.action))
          << ToActionName(accel_data.action)
          << " does not has layouts. Please add a layout to "
             "kAcceleratorLayouts and following the instruction in [1] or if "
             "it should not have layouts, state so "
             "explicitly by adding it to kAshAcceleratorsWithoutLayout in "
             "[1].\n"
             "[1] ash/webui/shortcut_customization_ui/backend/"
             "accelerator_layout_table.h.";
    }
  }
}

// Test that modifying Ash accelerator should update kAcceleratorLayouts.
// 1. If you are adding/deleting/modifying shortcuts, please also
//    add/delete/modify the corresponding item in kAcceleratorLayouts.
// 2. Please update the number and hash value of Ash accelerators on the top of
//    this file. The new number and hash value will be provided in the test
//    output.
TEST_F(AcceleratorLayoutMetadataTest, ModifyAcceleratorShouldUpdateLayout) {
  std::vector<ash::AcceleratorData> ash_accelerators;
  for (const ash::AcceleratorData& data : ash::kAcceleratorData) {
    ash_accelerators.emplace_back(data);
  }
  for (const ash::AcceleratorData& data :
       ash::kDisableWithNewMappingAcceleratorData) {
    ash_accelerators.emplace_back(data);
  }

  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    for (const AcceleratorData& data :
         ash::kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData) {
      ash_accelerators.emplace_back(data);
    }
  }

  if (!ash::assistant::features::IsNewEntryPointEnabled()) {
    for (const AcceleratorData& data :
         ash::kAssistantSearchPlusAAcceleratorData) {
      ash_accelerators.emplace_back(data);
    }
  }

  const char kCommonMessage[] =
      "If you are modifying Chrome OS available shortcuts, please update "
      "kAcceleratorLayouts & following the instruction in "
      "ash/webui/shortcut_customization_ui/backend/"
      "accelerator_layout_table.h and the following value(s) on the "
      "top of this file:\n";
  const int ash_accelerators_number = ash_accelerators.size();
  EXPECT_EQ(ash_accelerators_number, kAshAcceleratorsTotalNum)
      << kCommonMessage
      << "kAshAcceleratorsTotalNum=" << ash_accelerators_number << "\n";

  std::stable_sort(ash_accelerators.begin(), ash_accelerators.end(),
                   AshAcceleratorDataCmp());
  const std::string ash_accelerators_hash =
      ash::StableHashOfCollection(ash_accelerators, AshAcceleratorDataToString);
  EXPECT_EQ(ash_accelerators_hash, kAshAcceleratorsHash)
      << kCommonMessage << "kAshAcceleratorsHash=\"" << ash_accelerators_hash
      << "\"\n";
}

}  // namespace ash
