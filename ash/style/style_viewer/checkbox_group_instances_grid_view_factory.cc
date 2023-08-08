// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/style/checkbox.h"
#include "ash/style/checkbox_group.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"

namespace ash {

namespace {

// Conigure of grid view for `CheckboxGroup` instances. We have 3 instances in
// one column.
constexpr size_t kGridViewRowNum = 3;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 3;
constexpr size_t kGirdViewColGroupSize = 1;

constexpr int kCheckboxGroupWidth = 198;

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreateCheckboxGroupInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  // A normal checkbox group with 4 options.
  std::unique_ptr<CheckboxGroup> checkbox_group =
      std::make_unique<CheckboxGroup>(kCheckboxGroupWidth);
  checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button1");
  checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button2");
  checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button3");
  checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button4");

  // A checkbox group with 4 options and the third one is disabled.
  std::unique_ptr<CheckboxGroup> checkbox_group_with_disabled_button =
      std::make_unique<CheckboxGroup>(
          kCheckboxGroupWidth,
          /*inside_border_insets=*/gfx::Insets(2),
          /*between_child_spacing=*/2,
          /*checkbox_padding*/ gfx::Insets::TLBR(8, 24, 8, 12),
          /*image_label_spacing=*/16);
  checkbox_group_with_disabled_button->AddButton(Checkbox::PressedCallback(),
                                                 u"Test Button1");
  checkbox_group_with_disabled_button->AddButton(Checkbox::PressedCallback(),
                                                 u"Test Button2");
  auto* disabel_buttton = checkbox_group_with_disabled_button->AddButton(
      Checkbox::PressedCallback(), u"Test Button3");
  disabel_buttton->SetEnabled(false);
  checkbox_group_with_disabled_button->AddButton(Checkbox::PressedCallback(),
                                                 u"Test Button4");

  // A checkbox group with 4 options and all options are disabled.
  std::unique_ptr<CheckboxGroup> disabled_checkbox_group =
      std::make_unique<CheckboxGroup>(kCheckboxGroupWidth);
  disabled_checkbox_group->AddButton(Checkbox::PressedCallback(),
                                     u"Test Button1");
  auto* button = disabled_checkbox_group->AddButton(Checkbox::PressedCallback(),
                                                    u"Test Button2");
  button->SetSelected(true);
  disabled_checkbox_group->AddButton(Checkbox::PressedCallback(),
                                     u"Test Button3");
  disabled_checkbox_group->AddButton(Checkbox::PressedCallback(),
                                     u"Test Button4");
  disabled_checkbox_group->SetEnabled(false);

  grid_view->AddInstance(u"CheckboxGroup with default layout",
                         std::move(checkbox_group));
  grid_view->AddInstance(
      u"CheckboxGroup with customized layout and a disabled button",
      std::move(checkbox_group_with_disabled_button));

  grid_view->AddInstance(u"Disabled CheckboxGroup with default layout",
                         std::move(disabled_checkbox_group));

  return grid_view;
}

}  // namespace ash
