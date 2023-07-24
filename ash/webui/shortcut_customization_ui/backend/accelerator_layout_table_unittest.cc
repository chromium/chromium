// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"

#include <cstddef>

#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

std::string ToActionName(ash::AcceleratorAction action) {
  return base::StrCat(
      {"AcceleratorAction::k", GetAcceleratorActionName(action)});
}

class AcceleratorLayoutMetadataTest : public testing::Test {
 public:
  AcceleratorLayoutMetadataTest() = default;

  AcceleratorLayoutMetadataTest(const AcceleratorLayoutMetadataTest&) = delete;
  AcceleratorLayoutMetadataTest& operator=(
      const AcceleratorLayoutMetadataTest&) = delete;

  ~AcceleratorLayoutMetadataTest() override = default;

  void SetUp() override {
    for (const auto& layout : ash::kAcceleratorLayouts) {
      if (layout.source == mojom::AcceleratorSource::kAsh) {
        ash_accelerator_with_layouts_.insert(
            static_cast<ash::AcceleratorAction>(layout.action_id));
      }
    }

    testing::Test::SetUp();
  }

 protected:
  bool ShouldNotHaveLayouts(ash::AcceleratorAction action) {
    return kAshAcceleratorsWithoutLayout.find(action) !=
           kAshAcceleratorsWithoutLayout.end();
  }

  bool HasLayouts(ash::AcceleratorAction action) {
    return ash_accelerator_with_layouts_.find(action) !=
           ash_accelerator_with_layouts_.end();
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

}  // namespace ash
