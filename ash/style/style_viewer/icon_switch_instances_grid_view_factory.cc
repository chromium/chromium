// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_switch.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"

namespace ash {

namespace {

// Conigure of grid view for `IconSwitch` instances. We have 3 instances in one
// column.
constexpr size_t kGridViewRowNum = 3;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 3;
constexpr size_t kGirdViewColGroupSize = 1;

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreateIconSwitchInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  // Icon switch with two toggle buttons.
  std::unique_ptr<IconSwitch> icon_switch_two = std::make_unique<IconSwitch>();
  icon_switch_two->AddButton(IconButton::PressedCallback(),
                             &kCaptureModeImageIcon,
                             /*tooltip_text=*/u"Button");
  icon_switch_two->AddButton(IconButton::PressedCallback(),
                             &kCaptureModeCameraIcon, u"Button");

  // Disabled icon switch with two toggle buttons.
  std::unique_ptr<IconSwitch> icon_switch_two_disabled =
      std::make_unique<IconSwitch>();
  icon_switch_two_disabled->AddButton(IconButton::PressedCallback(),
                                      &kCaptureModeImageIcon,
                                      /*tooltip_text=*/u"Button");
  icon_switch_two_disabled->AddButton(IconButton::PressedCallback(),
                                      &kCaptureModeCameraIcon,
                                      /*tooltip_text=*/u"Button");
  icon_switch_two_disabled->SetEnabled(false);

  // Icon switch with three toggle buttons, customized layout, and no
  // background.
  std::unique_ptr<IconSwitch> icon_switch_three = std::make_unique<IconSwitch>(
      /*has_background*/ false, /*inside_border_insets=*/gfx::Insets(),
      /*between_child_spacing=*/16);
  icon_switch_three->AddButton(IconButton::PressedCallback(),
                               &kCaptureModeFullscreenIcon,
                               /*tooltip_text=*/u"Button");
  icon_switch_three->AddButton(IconButton::PressedCallback(),
                               &kCaptureModeRegionIcon,
                               /*tooltip_text=*/u"Button");
  icon_switch_three->AddButton(IconButton::PressedCallback(),
                               &kCaptureModeWindowIcon,
                               /*tooltip_text=*/u"Button");

  grid_view->AddInstance(
      u"Icon Switch with 2 buttons, default layout and background",
      std::move(icon_switch_two));
  grid_view->AddInstance(
      u"Icon Switch with 3 buttons, customized layout and no background",
      std::move(icon_switch_three));
  grid_view->AddInstance(u"Disabled Icon Switch with 2 buttons",
                         std::move(icon_switch_two_disabled));
  return grid_view;
}

}  // namespace ash
