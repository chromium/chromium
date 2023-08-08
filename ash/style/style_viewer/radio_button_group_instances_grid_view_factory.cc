// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/style/radio_button.h"
#include "ash/style/radio_button_group.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"

namespace ash {

namespace {

// Conigure of grid view for `RadioButtonGroup` instances. We have 4 instances
// in one column.
constexpr size_t kGridViewRowNum = 4;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 4;
constexpr size_t kGirdViewColGroupSize = 1;

constexpr int kRadioButtonGroupWidth = 198;
constexpr gfx::Insets kInsideBorderInsets(2);
constexpr int kBetweenChildSpacing = 2;
constexpr gfx::Insets kDefaultRadioButtonPadding =
    gfx::Insets::TLBR(8, 12, 8, 12);
constexpr gfx::Insets kCustomizedRadioButtonDefaultPadding =
    gfx::Insets::TLBR(8, 24, 8, 12);
constexpr int kCustomizedImageLabelSpacing = 16;

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreateRadioButtonGroupInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  // A normal radio button group with 3 options, circle icon type and icon
  // is leading.
  std::unique_ptr<RadioButtonGroup> radio_button_group =
      std::make_unique<RadioButtonGroup>(kRadioButtonGroupWidth);
  radio_button_group->AddButton(RadioButton::PressedCallback(),
                                u"Test Button1");
  radio_button_group->AddButton(RadioButton::PressedCallback(),
                                u"Test Button2");
  radio_button_group->AddButton(RadioButton::PressedCallback(),
                                u"Test Button3");

  // A normal radio button group with 3 options, and the third option is
  // disabled.
  std::unique_ptr<RadioButtonGroup> radio_button_group_with_disabled_button =
      std::make_unique<RadioButtonGroup>(
          kRadioButtonGroupWidth, kInsideBorderInsets, kBetweenChildSpacing,
          RadioButton::IconDirection::kLeading, RadioButton::IconType::kCircle,
          kDefaultRadioButtonPadding, RadioButton::kImageLabelSpacingDP);
  radio_button_group_with_disabled_button->AddButton(
      RadioButton::PressedCallback(), u"Test Button1");
  radio_button_group_with_disabled_button->AddButton(
      RadioButton::PressedCallback(), u"Test Button2");
  auto* disabel_buttton = radio_button_group_with_disabled_button->AddButton(
      RadioButton::PressedCallback(), u"Test Button3");
  disabel_buttton->SetEnabled(false);

  // A radio button group with 3 options, check icon type and icon
  // is following.
  std::unique_ptr<RadioButtonGroup> customized_button_group =
      std::make_unique<RadioButtonGroup>(
          kRadioButtonGroupWidth, kInsideBorderInsets, kBetweenChildSpacing,
          RadioButton::IconDirection::kFollowing, RadioButton::IconType::kCheck,
          kCustomizedRadioButtonDefaultPadding, kCustomizedImageLabelSpacing);
  customized_button_group->AddButton(RadioButton::PressedCallback(),
                                     u"Test Button1");
  customized_button_group->AddButton(RadioButton::PressedCallback(),
                                     u"Test Button2");
  customized_button_group->AddButton(RadioButton::PressedCallback(),
                                     u"Test Button3");

  // A radio button group with 3 options, all options are disabled.
  std::unique_ptr<RadioButtonGroup> disabled_radio_button_group =
      std::make_unique<RadioButtonGroup>(kRadioButtonGroupWidth);
  disabled_radio_button_group->AddButton(RadioButton::PressedCallback(),
                                         u"Test Button1");
  auto* button = disabled_radio_button_group->AddButton(
      RadioButton::PressedCallback(), u"Test Button2");
  button->SetSelected(true);
  disabled_radio_button_group->AddButton(RadioButton::PressedCallback(),
                                         u"Test Button3");
  disabled_radio_button_group->SetEnabled(false);

  grid_view->AddInstance(u"RadioButtonGroup with default layout and type",
                         std::move(radio_button_group));
  grid_view->AddInstance(
      u"RadioButtonGroup with default layout and a disabled button",
      std::move(radio_button_group_with_disabled_button));

  grid_view->AddInstance(u"RadioButtonGroup with customeize layout and type",
                         std::move(customized_button_group));
  grid_view->AddInstance(
      u"Disabled RadioButtonGroup with default layout and type",
      std::move(disabled_radio_button_group));

  return grid_view;
}

}  // namespace ash
