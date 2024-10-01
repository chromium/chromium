// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"

#include <cstddef>

#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "base/containers/contains.h"
#include "base/hash/md5.h"
#include "base/hash/md5_boringssl.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace ash {

namespace {

// The total number of Ash accelerators.
constexpr int kAshAcceleratorsTotalNum = 157;
// The hash of Ash accelerators.
constexpr char kAshAcceleratorsHash[] = "00d64e050abba1b0354ad946f608d420";

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

std::string HashAshAcceleratorData(
    const std::vector<ash::AcceleratorData>& accelerators) {
  base::MD5Context context;
  base::MD5Init(&context);
  for (const auto& accelerator : accelerators) {
    base::MD5Update(&context, AshAcceleratorDataToString(accelerator));
  }

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

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
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& accel_data = ash::kAcceleratorData[i];
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
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    ash_accelerators.emplace_back(ash::kAcceleratorData[i]);
  }
  for (size_t i = 0; i < ash::kDisableWithNewMappingAcceleratorDataLength;
       ++i) {
    ash_accelerators.emplace_back(
        ash::kDisableWithNewMappingAcceleratorData[i]);
  }

  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    for (size_t i = 0;
         i <
         ash::kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength;
         ++i) {
      ash_accelerators.emplace_back(
          ash::kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData[i]);
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
      HashAshAcceleratorData(ash_accelerators);
  EXPECT_EQ(ash_accelerators_hash, kAshAcceleratorsHash)
      << kCommonMessage << "kAshAcceleratorsHash=\"" << ash_accelerators_hash
      << "\"\n";
}

}  // namespace ash
