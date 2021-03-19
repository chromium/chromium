// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_config_provider.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/no_destructor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

namespace {

// The minimum allowed app list grid item heightafter scaling a config down.
constexpr float kMinimumTileHeightAfterConfigScale = 48.;

// Determines the app list config that should be used for a display work area
// size.
ash::AppListConfigType GetConfigTypeForDisplaySize(
    const gfx::Size& display_size) {
  // Landscape:
  if (display_size.width() > display_size.height()) {
    if (display_size.width() >= 1200)
      return ash::AppListConfigType::kLarge;
    if (display_size.width() >= 960)
      return ash::AppListConfigType::kMedium;
    return ash::AppListConfigType::kSmall;
  }

  // Portrait:
  if (display_size.width() >= 768)
    return ash::AppListConfigType::kLarge;
  if (display_size.width() >= 600)
    return ash::AppListConfigType::kMedium;
  return ash::AppListConfigType::kSmall;
}

}  // namespace

// static
AppListConfigProvider& AppListConfigProvider::Get() {
  static base::NoDestructor<AppListConfigProvider> instance;
  return *instance;
}

AppListConfigProvider::AppListConfigProvider() = default;

AppListConfigProvider::~AppListConfigProvider() = default;

void AppListConfigProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppListConfigProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

AppListConfig* AppListConfigProvider::GetConfigForType(AppListConfigType type,
                                                       bool can_create) {
  const auto config_it = configs_.find(type);
  if (config_it != configs_.end())
    return config_it->second.get();

  if (!can_create)
    return nullptr;

  auto config = std::make_unique<AppListConfig>(type);
  auto* result = config.get();
  configs_.emplace(type, std::move(config));

  for (auto& observer : observers_)
    observer.OnAppListConfigCreated(type);

  return result;
}

std::unique_ptr<AppListConfig> AppListConfigProvider::CreateForAppListWidget(
    const gfx::Size& display_work_area_size,
    const gfx::Insets& shelf_insets,
    const AppListConfig* current_config) {
  const AppListConfig& base_config =
      GetBaseConfigForDisplaySize(display_work_area_size);

  float scale_x = 1;
  float scale_y = 1;
  float inner_tile_scale_y = 1;

  const float min_config_scale =
      kMinimumTileHeightAfterConfigScale / base_config.grid_tile_height();

  const int min_grid_height =
      (display_work_area_size.width() < display_work_area_size.height()
           ? base_config.preferred_cols()
           : base_config.preferred_rows()) *
      base_config.grid_tile_height();
  const int min_grid_width =
      (display_work_area_size.width() < display_work_area_size.height()
           ? base_config.preferred_rows()
           : base_config.preferred_cols()) *
      base_config.grid_tile_width();

  // Note that minimum top margin matches the grid fadeout zone height.
  int non_grid_height = base_config.grid_fadeout_zone_height() +
                        base_config.suggestion_chip_container_top_margin() +
                        base_config.suggestion_chip_container_height();
  // Add search box height.
  non_grid_height +=
      display_work_area_size.height() - shelf_insets.height() >= 600
          ? base_config.search_box_height()
          : base_config.search_box_height_for_dense_layout();

  const int available_grid_height =
      display_work_area_size.height() - shelf_insets.height() - non_grid_height;

  if (available_grid_height < min_grid_height) {
    scale_y = std::max(
        min_config_scale,
        static_cast<float>(available_grid_height -
                           2 * base_config.grid_fadeout_zone_height()) /
            min_grid_height);
    // Adjust scale to reflect the fact the app list item title height does not
    // get scaled. The adjustment is derived from:
    // s * x + c = S * (x + c) and t = x + c
    // With: S - the target grid scale,
    //       x - scalable part of the tile (total title padding),
    //       c - constant part of the tile,
    //       t - tile height, and
    //       s - the adjusted scale.
    const int total_title_padding = base_config.grid_title_bottom_padding() +
                                    base_config.grid_title_top_padding();
    inner_tile_scale_y =
        (base_config.grid_tile_height() * (scale_y - 1) + total_title_padding) /
        total_title_padding;
  }

  const int available_grid_width =
      display_work_area_size.width() - shelf_insets.width() -
      2 * base_config.GetMinGridHorizontalPadding();
  if (available_grid_width < min_grid_width) {
    scale_x =
        std::max(min_config_scale,
                 static_cast<float>(available_grid_width) / min_grid_width);
  }

  if (current_config && current_config->type() == base_config.type() &&
      current_config->scale_x() == scale_x &&
      current_config->scale_y() == scale_y) {
    return nullptr;
  }

  return std::make_unique<AppListConfig>(base_config, scale_x, scale_y,
                                         inner_tile_scale_y,
                                         scale_y == min_config_scale);
}

std::set<AppListConfigType> AppListConfigProvider::GetAvailableConfigTypes() {
  std::set<AppListConfigType> types;
  for (auto& config : configs_)
    types.insert(config.first);
  return types;
}

void AppListConfigProvider::ResetForTesting() {
  configs_.clear();
}

const AppListConfig& AppListConfigProvider::GetBaseConfigForDisplaySize(
    const gfx::Size& display_work_area_size) {
  AppListConfigType type = GetConfigTypeForDisplaySize(display_work_area_size);
  // Ensures that the app list config provider has a config with the same
  // type as the created config - the app list model will use the config owned
  // by the AppListConfigProvider instance to generate folder icons needed by
  // app list UI.
  return *GetConfigForType(type, true /*can_create*/);
}

}  // namespace ash
