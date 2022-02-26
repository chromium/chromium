// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_config.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

namespace {

// Scales |value| using the smaller one of |scale_1| and |scale_2|.
int MinScale(int value, float scale_1, float scale_2) {
  return std::round(value * std::min(scale_1, scale_2));
}

// The height reduced from the tile when min scale is not sufficient to make the
// apps grid fit the available size - This would essentially remove the vertical
// padding for the unclipped folder icon.
int MinYScaleHeightAdjustmentForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kRegular:
      return 16;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kDense:
      return 8;
    case ash::AppListConfigType::kSmall:
      return 4;
  }
}

int GridTileWidthForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
      return 120;
    case ash::AppListConfigType::kMedium:
      return 88;
    case ash::AppListConfigType::kSmall:
      return 80;
    case ash::AppListConfigType::kRegular:
      return 96;
    case ash::AppListConfigType::kDense:
      return 80;
  }
}

int GridTileHeightForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kRegular:
      return 120;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kDense:
      return 88;
    case ash::AppListConfigType::kSmall:
      return 80;
  }
}

int GridIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kRegular:
      return 64;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kDense:
      return 48;
    case ash::AppListConfigType::kSmall:
      return 40;
  }
}

int GridTitleTopPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
      return 92;
    case ash::AppListConfigType::kMedium:
      return 64;
    case ash::AppListConfigType::kSmall:
      return 56;
    case ash::AppListConfigType::kRegular:
      return 88;
    case ash::AppListConfigType::kDense:
      return 60;
  }
}

int GridTitleBottomPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
      return 8;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kSmall:
      return 6;
    case ash::AppListConfigType::kRegular:
      return 12;
    case ash::AppListConfigType::kDense:
      return 8;
  }
}

int GridTitleHorizontalPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kRegular:
      return 8;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kDense:
      return 4;
    case ash::AppListConfigType::kSmall:
      return 0;
  }
}

int GridFocusDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
      return 80;
    case ash::AppListConfigType::kMedium:
      return 64;
    case ash::AppListConfigType::kSmall:
      return 56;
    // Unused for ProductivityLauncher.
    case ash::AppListConfigType::kRegular:
    case ash::AppListConfigType::kDense:
      return -1;
  }
}

int GridFocusCornerRadiusForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
      return 12;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kSmall:
    case ash::AppListConfigType::kRegular:
    case ash::AppListConfigType::kDense:
      return 8;
  }
}

int AppTitleMaxLineHeightForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kRegular:
      return 20;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kSmall:
    case ash::AppListConfigType::kDense:
      return 18;
  }
}

gfx::FontList AppTitleFontForType(ash::AppListConfigType type) {
  ui::ResourceBundle::FontDetails details;
  // TODO(https://crbug.com/1197600): Use Google Sans Text (medium weight) for
  // ProductivityLauncher (kRegular, kDense) when that font is available.
  switch (type) {
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kRegular:
      details.size_delta = 1;
      break;
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kSmall:
    case ash::AppListConfigType::kDense:
      details.size_delta = 0;
      break;
  }
  return ui::ResourceBundle::GetSharedInstance().GetFontListForDetails(details);
}

// See "App drag over folder" in go/cros-launcher-spec.
int FolderUnclippedIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
      return 88;
    case ash::AppListConfigType::kMedium:
      return 64;
    case ash::AppListConfigType::kSmall:
      return 56;
    case ash::AppListConfigType::kRegular:
      return 76;
    case ash::AppListConfigType::kDense:
      return 56;
  }
}

int FolderClippedIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
      return 72;
    case ash::AppListConfigType::kMedium:
      return 56;
    case ash::AppListConfigType::kSmall:
      return 48;
    case ash::AppListConfigType::kRegular:
      return 60;
    case ash::AppListConfigType::kDense:
      return 44;
  }
}

int ItemIconInFolderIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kRegular:
      return 32;
    case ash::AppListConfigType::kMedium:
      return 28;
    case ash::AppListConfigType::kSmall:
    case ash::AppListConfigType::kDense:
      return 24;
  }
}

int ItemIconInFolderIconMarginForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kLarge:
    case ash::AppListConfigType::kRegular:
    case ash::AppListConfigType::kMedium:
    case ash::AppListConfigType::kDense:
      return 4;
    case ash::AppListConfigType::kSmall:
      return 2;
  }
}

}  // namespace

SharedAppListConfig& SharedAppListConfig::instance() {
  static base::NoDestructor<SharedAppListConfig> shared_config;
  return *shared_config;
}

SharedAppListConfig::SharedAppListConfig()
    : search_result_title_font_style_(ui::ResourceBundle::BaseFont),
      search_result_recommendation_title_font_(
          ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(1)) {}

int SharedAppListConfig::GetMaxNumOfItemsPerPage() const {
  return 20;
}

int SharedAppListConfig::GetPreferredIconDimension(
    SearchResultDisplayType display_type) const {
  switch (display_type) {
    case SearchResultDisplayType::kList:
      return search_list_icon_dimension_;
    case SearchResultDisplayType::kTile:
      return search_tile_icon_dimension_;
    case SearchResultDisplayType::kChip:
      return suggestion_chip_icon_dimension_;
    case SearchResultDisplayType::kContinue:
      return suggestion_chip_icon_dimension_;
    case SearchResultDisplayType::kNone:
    case SearchResultDisplayType::kAnswerCard:
    case SearchResultDisplayType::kRecentApps:
    case SearchResultDisplayType::kLast:
      return 0;
  }
}

AppListConfig::AppListConfig(AppListConfigType type)
    : type_(type),
      scale_x_(1),
      scale_y_(1),
      grid_tile_width_(GridTileWidthForType(type)),
      grid_tile_height_(GridTileHeightForType(type)),
      grid_icon_dimension_(GridIconDimensionForType(type)),
      grid_icon_bottom_padding_(24),
      grid_title_top_padding_(GridTitleTopPaddingForType(type)),
      grid_title_bottom_padding_(GridTitleBottomPaddingForType(type)),
      grid_title_horizontal_padding_(GridTitleHorizontalPaddingForType(type)),
      grid_title_width_(grid_tile_width_),
      grid_focus_dimension_(GridFocusDimensionForType(type)),
      grid_focus_corner_radius_(GridFocusCornerRadiusForType(type)),
      app_title_max_line_height_(AppTitleMaxLineHeightForType(type)),
      app_title_font_(AppTitleFontForType(type)),
      folder_bubble_radius_(FolderUnclippedIconDimensionForType(type) / 2),
      folder_icon_dimension_(FolderClippedIconDimensionForType(type)),
      folder_unclipped_icon_dimension_(
          FolderUnclippedIconDimensionForType(type)),
      folder_icon_radius_(FolderClippedIconDimensionForType(type) / 2),
      folder_background_radius_(12),
      item_icon_in_folder_icon_dimension_(
          ItemIconInFolderIconDimensionForType(type)),
      item_icon_in_folder_icon_margin_(ItemIconInFolderIconMarginForType(type)),
      folder_dropping_circle_radius_(folder_bubble_radius_) {}

AppListConfig::AppListConfig(const AppListConfig& base_config,
                             float scale_x,
                             float scale_y,
                             float inner_tile_scale_y,
                             bool min_y_scale)
    : type_(base_config.type_),
      scale_x_(scale_x),
      scale_y_(scale_y),
      grid_tile_width_(MinScale(base_config.grid_tile_width_, scale_x, 1)),
      grid_tile_height_(MinScale(
          base_config.grid_tile_height_ -
              (min_y_scale ? MinYScaleHeightAdjustmentForType(type_) : 0),
          scale_y,
          1)),
      grid_icon_dimension_(MinScale(base_config.grid_icon_dimension_,
                                    scale_x,
                                    inner_tile_scale_y)),
      grid_icon_bottom_padding_(MinScale(
          base_config.grid_icon_bottom_padding_ +
              (min_y_scale ? MinYScaleHeightAdjustmentForType(type_) : 0),
          inner_tile_scale_y,
          1)),
      grid_title_top_padding_(MinScale(
          base_config.grid_title_top_padding_ -
              (min_y_scale ? MinYScaleHeightAdjustmentForType(type_) : 0),
          inner_tile_scale_y,
          1)),
      grid_title_bottom_padding_(
          MinScale(base_config.grid_title_bottom_padding_,
                   inner_tile_scale_y,
                   1)),
      grid_title_horizontal_padding_(
          MinScale(base_config.grid_title_horizontal_padding_, scale_x, 1)),
      grid_title_width_(base_config.grid_tile_width_),
      grid_focus_dimension_(MinScale(base_config.grid_focus_dimension_,
                                     scale_x,
                                     inner_tile_scale_y)),
      grid_focus_corner_radius_(MinScale(base_config.grid_focus_corner_radius_,
                                         scale_x,
                                         inner_tile_scale_y)),
      app_title_max_line_height_(base_config.app_title_max_line_height_),
      app_title_font_(base_config.app_title_font_.DeriveWithSizeDelta(
          min_y_scale ? -2 : (scale_y < 0.66 ? -1 : 0))),
      folder_bubble_radius_(MinScale(base_config.folder_bubble_radius_,
                                     scale_x,
                                     inner_tile_scale_y)),
      folder_icon_dimension_(MinScale(base_config.folder_icon_dimension_,
                                      scale_x,
                                      inner_tile_scale_y)),
      folder_unclipped_icon_dimension_(
          MinScale(base_config.folder_unclipped_icon_dimension_,
                   scale_x,
                   inner_tile_scale_y)),
      folder_icon_radius_(MinScale(base_config.folder_icon_radius_,
                                   scale_x,
                                   inner_tile_scale_y)),
      folder_background_radius_(
          MinScale(base_config.folder_background_radius_, scale_x, scale_y)),
      item_icon_in_folder_icon_dimension_(
          MinScale(base_config.item_icon_in_folder_icon_dimension_,
                   scale_x,
                   inner_tile_scale_y)),
      item_icon_in_folder_icon_margin_(
          MinScale(base_config.item_icon_in_folder_icon_margin_,
                   scale_x,
                   inner_tile_scale_y)),
      folder_dropping_circle_radius_(
          MinScale(base_config.folder_dropping_circle_radius_,
                   scale_x,
                   inner_tile_scale_y)) {}

AppListConfig::~AppListConfig() = default;

}  // namespace ash
