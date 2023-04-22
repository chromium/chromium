// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"
#include "base/memory/raw_ptr.h"

#include <memory>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

namespace {

// Conigure of grid view for `IconButton` instances. We have 4 x 3 instances
// divided into 3 column groups.
constexpr size_t kGridViewRowNum = 8;
constexpr size_t kGridViewColNum = 4;
constexpr size_t kGridViewRowGroupSize = 4;
constexpr size_t kGirdViewColGroupSize = 1;

struct IconButtonInfo {
  std::u16string name;
  IconButton::Type type;
  bool is_toggled;
  bool is_enabled;
  raw_ptr<gfx::ImageSkia, ExperimentalAsh> bg_img;
};

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreateIconButtonInstancesGridView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  auto* image =
      rb.GetImageSkiaNamed(IDR_SETTINGS_RGB_KEYBOARD_RAINBOW_COLOR_48_PNG);
  // The types of instances shown in the grid view. Each type contains the type
  // name, icon button type, if the button is toggled, if the button is enabled,
  // the background image pointer.
  const std::vector<std::vector<IconButtonInfo>> type_groups{
      {{u"Default XSmall", IconButton::Type::kXSmall, false, true, nullptr},
       {u"Default Small", IconButton::Type::kSmall, false, true, nullptr},
       {u"Default Meduim", IconButton::Type::kMedium, false, true, nullptr},
       {u"Default Large", IconButton::Type::kLarge, false, true, nullptr},

       {u"Floating XSmall", IconButton::Type::kXSmallFloating, false, true,
        nullptr},
       {u"Floating Small", IconButton::Type::kSmallFloating, false, true,
        nullptr},
       {u"Floating Meduim", IconButton::Type::kMediumFloating, false, true,
        nullptr},
       {u"Floating Large", IconButton::Type::kLargeFloating, false, true,
        nullptr},

       {u"Toggled XSmall", IconButton::Type::kXSmall, true, true, nullptr},
       {u"Toggled Small", IconButton::Type::kSmall, true, true, nullptr},
       {u"Toggled Meduim", IconButton::Type::kMedium, true, true, nullptr},
       {u"Toggled Large", IconButton::Type::kLarge, true, true, nullptr},

       {u"Prominent Floating XSmall",
        IconButton::Type::kXSmallProminentFloating, false, true, nullptr},
       {u"Prominent Floating Small", IconButton::Type::kSmallProminentFloating,
        false, true, nullptr},
       {u"Prominent Floating Medium",
        IconButton::Type::kMediumProminentFloating, false, true, nullptr},
       {u"Prominent Floating Large", IconButton::Type::kLargeProminentFloating,
        false, true, nullptr}},

      {{u"Default XSmall With Background Image", IconButton::Type::kXSmall,
        false, true, image},
       {u"Default Small With Background Image", IconButton::Type::kSmall, false,
        true, image},
       {u"Default Medium With Background Image", IconButton::Type::kMedium,
        false, true, image},
       {u"Default Large With Background Image", IconButton::Type::kLarge, false,
        true, image},

       {u"Disabled XSmall", IconButton::Type::kXSmall, false, false, nullptr},
       {u"Disabled Small", IconButton::Type::kSmall, false, false, nullptr},
       {u"Disabled Meduim", IconButton::Type::kMedium, false, false, nullptr},
       {u"Disabled Large", IconButton::Type::kLarge, false, false, nullptr}}};

  // Insert the instance in grid view with column-primary order.
  for (auto types : type_groups) {
    const size_t group_size = types.size();
    for (size_t i = 0; i < kGridViewColNum * kGridViewRowGroupSize; i++) {
      // Transfer index from row-primary order to column-primary order
      const size_t row_id = i / kGridViewColNum;
      const size_t col_id = i % kGridViewColNum;
      const size_t idx = col_id * kGridViewRowGroupSize + row_id;
      if (idx >= group_size) {
        grid_view->AddInstance(u"", std::unique_ptr<IconButton>(nullptr));
      } else {
        IconButtonInfo type_info = types[idx];
        auto* button = grid_view->AddInstance(
            /*name=*/type_info.name,
            std::make_unique<IconButton>(IconButton::PressedCallback(),
                                         /*type=*/type_info.type,
                                         &kSettingsIcon,
                                         /*accessible_name=*/type_info.name,
                                         /*is_togglable=*/true,
                                         /*has_border=*/true));
        button->SetToggled(type_info.is_toggled);
        button->SetEnabled(type_info.is_enabled);
        if (type_info.bg_img) {
          button->SetBackgroundImage(*type_info.bg_img);
        }
      }
    }
  }

  return grid_view;
}

}  // namespace ash
