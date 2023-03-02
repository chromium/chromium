// Copyright 2018 The Chromium Authors
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

// Scales `value` by `scale`
int Scale(int value, float scale) {
  return std::round(value * scale);
}

int GridTileWidthForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 96;
    case ash::AppListConfigType::kDense:
      return 80;
  }
}

int GridTileHeightForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 120;
    case ash::AppListConfigType::kDense:
      return 88;
  }
}

int GridIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 64;
    case ash::AppListConfigType::kDense:
      return 48;
  }
}

int GridTitleTopPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 88;
    case ash::AppListConfigType::kDense:
      return 60;
  }
}

int GridTitleBottomPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 12;
    case ash::AppListConfigType::kDense:
      return 8;
  }
}

int GridTitleHorizontalPaddingForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 8;
    case ash::AppListConfigType::kDense:
      return 4;
  }
}

int AppTitleMaxLineHeightForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 20;
    case ash::AppListConfigType::kDense:
      return 18;
  }
}

gfx::FontList AppTitleFontForType(ash::AppListConfigType type) {
  ui::ResourceBundle::FontDetails details;
  // TODO(https://crbug.com/1197600): Use Google Sans Text (medium weight) when
  // the font is available.
  switch (type) {
    case ash::AppListConfigType::kRegular:
      details.size_delta = 1;
      break;
    case ash::AppListConfigType::kDense:
      details.size_delta = 0;
      break;
  }
  return ui::ResourceBundle::GetSharedInstance().GetFontListForDetails(details);
}

// See "App drag over folder" in go/cros-launcher-spec.
int UnclippedIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 76;
    case ash::AppListConfigType::kDense:
      return 56;
  }
}

int IconVisibleDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 60;
    case ash::AppListConfigType::kDense:
      return 44;
  }
}

int IconExtendedBackgroundRadius(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 24;
    case ash::AppListConfigType::kDense:
      return 16;
  }
}

int ItemIconInFolderIconDimensionForType(ash::AppListConfigType type) {
  switch (type) {
    case ash::AppListConfigType::kRegular:
      return 32;
    case ash::AppListConfigType::kDense:
      return 24;
  }
}

int ItemIconInFolderIconMargin() {
  return features::IsAppCollectionFolderRefreshEnabled() ? 2 : 4;
}

}  // namespace

SharedAppListConfig& SharedAppListConfig::instance() {
  static SharedAppListConfig shared_config;
  return shared_config;
}

SharedAppListConfig::SharedAppListConfig() = default;

int SharedAppListConfig::GetMaxNumOfItemsPerPage() const {
  return 20;
}

int SharedAppListConfig::GetPreferredIconDimension(
    SearchResultDisplayType display_type) const {
  switch (display_type) {
    case SearchResultDisplayType::kList:
      return search_list_icon_dimension_;
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
      grid_tile_width_(GridTileWidthForType(type)),
      grid_tile_height_(GridTileHeightForType(type)),
      grid_icon_dimension_(GridIconDimensionForType(type)),
      grid_icon_bottom_padding_(24),
      grid_title_top_padding_(GridTitleTopPaddingForType(type)),
      grid_title_bottom_padding_(GridTitleBottomPaddingForType(type)),
      grid_title_horizontal_padding_(GridTitleHorizontalPaddingForType(type)),
      grid_title_width_(grid_tile_width_),
      grid_focus_corner_radius_(8),
      app_title_max_line_height_(AppTitleMaxLineHeightForType(type)),
      app_title_font_(AppTitleFontForType(type)),
      folder_bubble_radius_(UnclippedIconDimensionForType(type) / 2),
      icon_visible_dimension_(IconVisibleDimensionForType(type)),
      unclipped_icon_dimension_(UnclippedIconDimensionForType(type)),
      folder_icon_radius_(IconVisibleDimensionForType(type) / 2),
      icon_extended_background_radius_(IconExtendedBackgroundRadius(type)),
      item_icon_in_folder_icon_dimension_(
          ItemIconInFolderIconDimensionForType(type)),
      item_icon_in_folder_icon_margin_(ItemIconInFolderIconMargin()),
      folder_dropping_circle_radius_(folder_bubble_radius_) {}

AppListConfig::AppListConfig(const AppListConfig& base_config, float scale_x)
    : type_(base_config.type_),
      scale_x_(scale_x),
      grid_tile_width_(Scale(base_config.grid_tile_width_, scale_x)),
      grid_tile_height_(base_config.grid_tile_height_),
      grid_icon_dimension_(Scale(base_config.grid_icon_dimension_, scale_x)),
      grid_icon_bottom_padding_(base_config.grid_icon_bottom_padding_),
      grid_title_top_padding_(base_config.grid_title_top_padding_),
      grid_title_bottom_padding_(base_config.grid_title_bottom_padding_),
      grid_title_horizontal_padding_(
          Scale(base_config.grid_title_horizontal_padding_, scale_x)),
      grid_title_width_(base_config.grid_tile_width_),
      grid_focus_corner_radius_(
          Scale(base_config.grid_focus_corner_radius_, scale_x)),
      app_title_max_line_height_(base_config.app_title_max_line_height_),
      app_title_font_(base_config.app_title_font_),
      folder_bubble_radius_(Scale(base_config.folder_bubble_radius_, scale_x)),
      icon_visible_dimension_(
          Scale(base_config.icon_visible_dimension_, scale_x)),
      unclipped_icon_dimension_(
          Scale(base_config.unclipped_icon_dimension_, scale_x)),
      folder_icon_radius_(Scale(base_config.folder_icon_radius_, scale_x)),
      icon_extended_background_radius_(
          Scale(base_config.icon_extended_background_radius_, scale_x)),

      item_icon_in_folder_icon_dimension_(
          Scale(base_config.item_icon_in_folder_icon_dimension_, scale_x)),
      item_icon_in_folder_icon_margin_(
          Scale(base_config.item_icon_in_folder_icon_margin_, scale_x)),
      folder_dropping_circle_radius_(
          Scale(base_config.folder_dropping_circle_radius_, scale_x)) {}

AppListConfig::~AppListConfig() = default;

}  // namespace ash
