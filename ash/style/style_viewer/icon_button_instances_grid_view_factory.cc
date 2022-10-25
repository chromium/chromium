// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"

namespace ash {

namespace {

// Conigure of grid view for `IconButton` instances. We have 4 x 3 instances
// divided into 3 column groups.
constexpr size_t kGridViewRowNum = 4;
constexpr size_t kGridViewColNum = 3;
constexpr size_t kGridViewRowGroupSize = 4;
constexpr size_t kGirdViewColGroupSize = 1;

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreateIconButtonInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  // The types of instances shown in the grid view. Each type contains the type
  // name, icon button type, and if the button is toggled.
  const std::list<std::tuple<std::u16string, IconButton::Type, bool>> types{
      {u"Default XSmall", IconButton::Type::kXSmall, false},
      {u"Floating XSmall", IconButton::Type::kXSmallFloating, false},
      {u"Toggled XSmall", IconButton::Type::kXSmall, true},
      {u"Default Small", IconButton::Type::kSmall, false},
      {u"Floating Small", IconButton::Type::kSmallFloating, false},
      {u"Toggled Small", IconButton::Type::kSmall, true},
      {u"Default Meduim", IconButton::Type::kMedium, false},
      {u"Floating Meduim", IconButton::Type::kMediumFloating, false},
      {u"Toggled Meduim", IconButton::Type::kMedium, true},
      {u"Default Large", IconButton::Type::kLarge, false},
      {u"Floating Large", IconButton::Type::kLargeFloating, false},
      {u"Toggled Large", IconButton::Type::kLarge, true}};

  for (auto type : types) {
    auto* button = grid_view->AddInstance(
        /*name=*/std::get<0>(type),
        std::make_unique<IconButton>(IconButton::PressedCallback(),
                                     /*type=*/std::get<1>(type), &kSettingsIcon,
                                     /*accessible_name=*/std::get<0>(type),
                                     /*is_togglable=*/true,
                                     /*has_border=*/true));
    button->SetToggled(std::get<2>(type));
  }
  return grid_view;
}

}  // namespace ash
