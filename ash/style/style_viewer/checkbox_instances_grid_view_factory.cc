// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/style/checkbox.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"

namespace ash {

namespace {

// Conigure of grid view for `Checkbox` instances. We have 4 instances in
// one column.
constexpr size_t kGridViewRowNum = 4;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 4;
constexpr size_t kGirdViewColGroupSize = 1;

constexpr auto kCheckboxWidth = 238;
constexpr auto kCheckboxPadding = gfx::Insets::TLBR(8, 12, 8, 12);

}  // namespace

std::unique_ptr<SystemUIComponentsGridView> CreateCheckboxInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);
  grid_view->AddInstance(
      u"", std::make_unique<Checkbox>(
               kCheckboxWidth, Checkbox::PressedCallback(),
               /*label=*/u"Unselected Enabled Checkbox", kCheckboxPadding));
  auto selected_enabled_checkbox = std::make_unique<Checkbox>(
      kCheckboxWidth, Checkbox::PressedCallback(),
      /*label=*/u"Selected Enabled Checkbox", kCheckboxPadding);

  selected_enabled_checkbox->SetSelected(true);
  grid_view->AddInstance(u"", std::move(selected_enabled_checkbox));

  auto unselected_disabled_checkbox = std::make_unique<Checkbox>(
      kCheckboxWidth, Checkbox::PressedCallback(),
      /*label=*/u"Unselected Disabled Checkbox", kCheckboxPadding);
  unselected_disabled_checkbox->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(unselected_disabled_checkbox));

  auto selected_disabled_checkbox = std::make_unique<Checkbox>(
      kCheckboxWidth, Checkbox::PressedCallback(),
      /*label=*/u"Selected Disabled Checkbox", kCheckboxPadding);
  selected_disabled_checkbox->SetSelected(true);
  selected_disabled_checkbox->SetEnabled(false);
  grid_view->AddInstance(u"", std::move(selected_disabled_checkbox));

  return grid_view;
}

}  // namespace ash
