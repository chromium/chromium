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

// 3 columns with 15 rows divided into groups of 5.
//
//+-----------------+-----------------+-----------------+
//|Default XSmall   | Floating XSmall | Toggled XSmall  |
//|      ...        |       ...       |       ...       |
//|Default XLarge   | Floating XLarge | Toggled XLarge  |
//+-----------------+-----------------+-----------------+
//|Prominent XSmall | Image XSmall    | Disabled XSmall |
//|      ...        |       ...       |       ...       |
//|Prominent XLarge | Image XLarge    | Disabled XLarge |
//+-----------------+-----------------+-----------------+
//|Symbol XSmall    |                 |                 |
//|      ...        |                 |                 |
//|Symbol XLarge    |                 |                 |
//+-----------------+-----------------+-----------------+
constexpr size_t kGridViewRowNum = 15;
constexpr size_t kGridViewRowGroupSize = 5;

constexpr size_t kGridViewColNum = 3;
constexpr size_t kGirdViewColGroupSize = 1;

struct IconButtonInfo {
  std::u16string name;
  IconButton::Type type;
  bool is_toggled;
  bool is_enabled;
  raw_ptr<gfx::ImageSkia> bg_img;
  std::optional<base_icu::UChar32> symbol;
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
       {u"Default XLarge", IconButton::Type::kXLarge, false, true, nullptr},

       {u"Floating XSmall", IconButton::Type::kXSmallFloating, false, true,
        nullptr},
       {u"Floating Small", IconButton::Type::kSmallFloating, false, true,
        nullptr},
       {u"Floating Meduim", IconButton::Type::kMediumFloating, false, true,
        nullptr},
       {u"Floating Large", IconButton::Type::kLargeFloating, false, true,
        nullptr},
       {u"Floating XLarge", IconButton::Type::kXLargeFloating, false, true,
        nullptr},

       {u"Toggled XSmall", IconButton::Type::kXSmall, true, true, nullptr},
       {u"Toggled Small", IconButton::Type::kSmall, true, true, nullptr},
       {u"Toggled Meduim", IconButton::Type::kMedium, true, true, nullptr},
       {u"Toggled Large", IconButton::Type::kLarge, true, true, nullptr},
       {u"Toggled XLarge", IconButton::Type::kLarge, true, true, nullptr}},

      {{u"Prominent Floating XSmall",
        IconButton::Type::kXSmallProminentFloating, false, true, nullptr},
       {u"Prominent Floating Small", IconButton::Type::kSmallProminentFloating,
        false, true, nullptr},
       {u"Prominent Floating Medium",
        IconButton::Type::kMediumProminentFloating, false, true, nullptr},
       {u"Prominent Floating Large", IconButton::Type::kLargeProminentFloating,
        false, true, nullptr},
       {u"Prominent Floating XLarge",
        IconButton::Type::kXLargeProminentFloating, false, true, nullptr},

       {u"Default XSmall With Background Image", IconButton::Type::kXSmall,
        false, true, image},
       {u"Default Small With Background Image", IconButton::Type::kSmall, false,
        true, image},
       {u"Default Medium With Background Image", IconButton::Type::kMedium,
        false, true, image},
       {u"Default Large With Background Image", IconButton::Type::kLarge, false,
        true, image},
       {u"Default XLarge With Background Image", IconButton::Type::kXLarge,
        false, true, image},

       {u"Disabled XSmall", IconButton::Type::kXSmall, false, false, nullptr},
       {u"Disabled Small", IconButton::Type::kSmall, false, false, nullptr},
       {u"Disabled Meduim", IconButton::Type::kMedium, false, false, nullptr},
       {u"Disabled Large", IconButton::Type::kLarge, false, false, nullptr},
       {u"Disabled XLarge", IconButton::Type::kXLarge, false, false, nullptr}},

      {{u"Symbol XSmall", IconButton::Type::kXSmall, false, true, nullptr,
        0x0032 /*2 numeral*/},
       {u"Symbol Small", IconButton::Type::kSmall, false, true, nullptr,
        0x003E /*> symbol*/},
       {u"Symbol Meduim", IconButton::Type::kMedium, false, true, nullptr,
        0x03A9 /*Upper Case Omega*/},
       {u"Symbol Large", IconButton::Type::kLarge, false, true, nullptr,
        0x2615 /*Hot Beverage*/},
       {u"Symbol XLarge", IconButton::Type::kXLarge, false, true, nullptr,
        0x1F6EBU /*Airplane Departing*/}},
  };

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
        IconButton::Builder builder;
        builder.SetType(type_info.type)
            .SetAccessibleName(type_info.name)
            .SetTogglable(type_info.is_toggled)
            .SetEnabled(type_info.is_enabled)
            .SetBorder(/*has_border*/ true);
        if (type_info.symbol.has_value()) {
          builder.SetSymbol(*type_info.symbol);
        } else {
          builder.SetVectorIcon(&kSettingsIcon);
        }
        if (type_info.bg_img) {
          builder.SetBackgroundImage(*type_info.bg_img);
        }
        grid_view->AddInstance(/*name=*/type_info.name, builder.Build());
      }
    }
  }

  return grid_view;
}

}  // namespace ash
