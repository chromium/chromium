// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/style/radio_button.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"

namespace ash {

namespace {

// Conigure of grid view for `RadioButton` instances. We have 4* 4 instances
// divided into 4 column groups.
constexpr size_t kGridViewRowNum = 4;
constexpr size_t kGridViewColNum = 4;
constexpr size_t kGridViewRowGroupSize = 4;
constexpr size_t kGirdViewColGroupSize = 1;

constexpr int kRadioButtonWidth = 218;
constexpr auto kRadioButtonPadding = gfx::Insets::TLBR(8, 12, 8, 12);

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreateRadioButtonInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);
  grid_view->AddInstance(
      u"", std::make_unique<RadioButton>(
               kRadioButtonWidth, RadioButton::PressedCallback(),
               /*label=*/u"Default CircleRight",
               RadioButton::IconDirection::kLeading,
               RadioButton::IconType::kCircle, kRadioButtonPadding));
  grid_view->AddInstance(
      u"", std::make_unique<RadioButton>(
               kRadioButtonWidth, RadioButton::PressedCallback(),
               /*label=*/u"Default CircleRight",
               RadioButton::IconDirection::kFollowing,
               RadioButton::IconType::kCircle, kRadioButtonPadding));
  grid_view->AddInstance(
      u"",
      std::make_unique<RadioButton>(
          kRadioButtonWidth, RadioButton::PressedCallback(),
          /*label=*/u"Default CheckRight", RadioButton::IconDirection::kLeading,
          RadioButton::IconType::kCheck, kRadioButtonPadding));
  grid_view->AddInstance(
      u"", std::make_unique<RadioButton>(
               kRadioButtonWidth, RadioButton::PressedCallback(),
               /*label=*/u"Default CheckRight",
               RadioButton::IconDirection::kFollowing,
               RadioButton::IconType::kCheck, kRadioButtonPadding));

  auto selected_circle_left = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Selected CircleRight", RadioButton::IconDirection::kLeading,
      RadioButton::IconType::kCircle, kRadioButtonPadding);
  selected_circle_left->SetSelected(true);
  grid_view->AddInstance(u"", std::move(selected_circle_left));

  auto selected_circle_right = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Selected CircleRight", RadioButton::IconDirection::kFollowing,
      RadioButton::IconType::kCircle, kRadioButtonPadding);
  selected_circle_right->SetSelected(true);
  grid_view->AddInstance(u"", std::move(selected_circle_right));

  auto selected_check_left = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Selected CheckRight", RadioButton::IconDirection::kLeading,
      RadioButton::IconType::kCheck, kRadioButtonPadding);
  selected_check_left->SetSelected(true);
  grid_view->AddInstance(u"", std::move(selected_check_left));

  auto selected_check_right = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Selected CheckRight", RadioButton::IconDirection::kFollowing,
      RadioButton::IconType::kCheck, kRadioButtonPadding);
  selected_check_right->SetSelected(true);
  grid_view->AddInstance(u"", std::move(selected_check_right));

  auto disable_circle_left = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Disabled CircleRight", RadioButton::IconDirection::kLeading,
      RadioButton::IconType::kCircle, kRadioButtonPadding);
  disable_circle_left->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(disable_circle_left));

  auto disable_circle_right = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Disabled CircleRight", RadioButton::IconDirection::kFollowing,
      RadioButton::IconType::kCircle, kRadioButtonPadding);
  disable_circle_right->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(disable_circle_right));

  auto disable_check_left = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Disabled CheckRight", RadioButton::IconDirection::kLeading,
      RadioButton::IconType::kCheck, kRadioButtonPadding);
  disable_check_left->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(disable_check_left));

  auto disable_check_right = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Disabled CheckRight", RadioButton::IconDirection::kFollowing,
      RadioButton::IconType::kCheck, kRadioButtonPadding);
  disable_check_right->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(disable_check_right));

  auto disable_selected_circle_left = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Disabled Selected CircleRight",
      RadioButton::IconDirection::kLeading, RadioButton::IconType::kCircle,
      kRadioButtonPadding);
  disable_selected_circle_left->SetSelected(true);
  disable_selected_circle_left->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(disable_selected_circle_left));

  auto disable_selected_circle_right = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Disabled SelectedCircleRight",
      RadioButton::IconDirection::kFollowing, RadioButton::IconType::kCircle,
      kRadioButtonPadding);
  disable_selected_circle_right->SetSelected(true);
  disable_selected_circle_right->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(disable_selected_circle_right));

  auto disable_selected_check_left = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Disabled Selected CheckRight",
      RadioButton::IconDirection::kLeading, RadioButton::IconType::kCheck,
      kRadioButtonPadding);
  disable_selected_check_left->SetSelected(true);
  disable_selected_check_left->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(disable_selected_check_left));

  auto disable_selected_check_right = std::make_unique<RadioButton>(
      kRadioButtonWidth, RadioButton::PressedCallback(),
      /*label=*/u"Disabled Selected CheckRight",
      RadioButton::IconDirection::kFollowing, RadioButton::IconType::kCheck,
      kRadioButtonPadding);
  disable_selected_check_right->SetSelected(true);
  disable_selected_check_right->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(disable_selected_check_right));

  return grid_view;
}

}  // namespace ash
