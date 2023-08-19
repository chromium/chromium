// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/mojom/accelerator_actions_mojom_traits.h"
#include "ash/public/mojom/accelerator_actions.mojom.h"

#include <algorithm>

#include "ash/public/cpp/accelerator_actions.h"
#include "base/containers/fixed_flat_map.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using mojom_accelerator_action = ash::mojom::AcceleratorAction;

namespace {

template <typename MojoEnum, typename AcceleratorActionEnum, size_t N>
void TestAcceleratorActionToMojo(
    const base::fixed_flat_map<MojoEnum, AcceleratorActionEnum, N>& enums) {
  for (auto enum_pair : enums) {
    EXPECT_EQ(enum_pair.first,
              (mojo::EnumTraits<MojoEnum, AcceleratorActionEnum>::ToMojom(
                  enum_pair.second)))
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}

template <typename MojoEnum, typename AcceleratorActionEnum, size_t N>
void TestMojoToAcceleratorAction(
    const base::fixed_flat_map<MojoEnum, AcceleratorActionEnum, N>& enums) {
  for (auto enum_pair : enums) {
    AcceleratorActionEnum mojo_to_accelerator_action_enum;
    EXPECT_TRUE((mojo::EnumTraits<MojoEnum, AcceleratorActionEnum>::FromMojom(
        enum_pair.first, &mojo_to_accelerator_action_enum)));
    EXPECT_EQ(mojo_to_accelerator_action_enum, enum_pair.second)
        << "enum " << enum_pair.first << " != " << enum_pair.second;
  }
}

}  // namespace

TEST(AcceleratorActionsTraitsTest, SerializeAndDeserialize) {
  // Spot check random accelerator actions and confirm that they were able to
  // serialize to mojom and then deserialize back to accelerator actions.
  const auto enums =
      base::MakeFixedFlatMap<mojom_accelerator_action, ash::AcceleratorAction>(
          {{mojom_accelerator_action::kBrightnessDown,
            ash::AcceleratorAction::kBrightnessDown},
           {mojom_accelerator_action::kCycleBackwardMru,
            ash::AcceleratorAction::kCycleBackwardMru},
           {mojom_accelerator_action::kDesksActivateDeskLeft,
            ash::AcceleratorAction::kDesksActivateDeskLeft},
           {mojom_accelerator_action::kDesksToggleAssignToAllDesks,
            ash::AcceleratorAction::kDesksToggleAssignToAllDesks},
           {mojom_accelerator_action::kExit, ash::AcceleratorAction::kExit},
           {mojom_accelerator_action::kFocusCameraPreview,
            ash::AcceleratorAction::kFocusCameraPreview},
           {mojom_accelerator_action::kFocusNextPane,
            ash::AcceleratorAction::kFocusNextPane},
           {mojom_accelerator_action::kKeyboardBrightnessUp,
            ash::AcceleratorAction::kKeyboardBrightnessUp},
           {mojom_accelerator_action::kLaunchApp0,
            ash::AcceleratorAction::kLaunchApp0},
           {mojom_accelerator_action::kPrivacyScreenToggle,
            ash::AcceleratorAction::kPrivacyScreenToggle}});

  TestAcceleratorActionToMojo(enums);
  TestMojoToAcceleratorAction(enums);
}

}  // namespace ash
