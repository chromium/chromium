// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/no_destructor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class FontList;
}

namespace ash {

// App list layout related constants that are used outside the core app list UI
// code - for example in chrome, and app list search UI.
// Unlike values is `AppListConfig`, the values in `SharedAppListConfig` do not
// depend on the current app list view state nor dimensions.
// An instance can be retrieved using `SharedAppListConfig::instance()`.
class ASH_PUBLIC_EXPORT SharedAppListConfig {
 public:
  static SharedAppListConfig& instance();

  int default_grid_icon_dimension() const {
    return default_grid_icon_dimension_;
  }

  int shortcut_badge_icon_dimension() const {
    return shortcut_badge_icon_dimension_;
  }

  size_t max_search_results() const { return max_search_results_; }

  size_t max_search_result_list_items() const {
    return max_search_result_list_items_;
  }

  size_t max_assistant_search_result_list_items() const {
    return max_assistant_search_result_list_items_;
  }

  int search_tile_icon_dimension() const { return search_tile_icon_dimension_; }

  gfx::Size search_tile_icon_size() const {
    return gfx::Size(search_tile_icon_dimension_, search_tile_icon_dimension_);
  }

  int search_tile_badge_icon_dimension() const {
    return search_tile_badge_icon_dimension_;
  }

  gfx::Size search_tile_badge_icon_size() const {
    return gfx::Size(search_tile_badge_icon_dimension_,
                     search_tile_badge_icon_dimension_);
  }

  int search_tile_badge_icon_offset() const {
    return search_tile_badge_icon_offset_;
  }

  int search_list_icon_dimension() const { return search_list_icon_dimension_; }

  gfx::Size search_list_icon_size() const {
    return gfx::Size(search_list_icon_dimension_, search_list_icon_dimension_);
  }

  int search_list_icon_vertical_bar_dimension() const {
    return search_list_icon_vertical_bar_dimension_;
  }

  int search_list_badge_icon_dimension() const {
    return search_list_badge_icon_dimension_;
  }

  gfx::Size search_list_badge_icon_size() const {
    return gfx::Size(search_list_badge_icon_dimension_,
                     search_list_badge_icon_dimension_);
  }

  int suggestion_chip_icon_dimension() const {
    return suggestion_chip_icon_dimension_;
  }

  int search_tile_height() const { return search_tile_height_; }

  size_t max_results_with_categorical_search() const {
    return max_results_with_categorical_search_;
  }

  int answer_card_max_results() const { return answer_card_max_results_; }

  size_t image_search_max_results() const { return image_search_max_results_; }

  // Returns the maximum number of items allowed in a page in the apps grid.
  int GetMaxNumOfItemsPerPage() const;

  // Returns the dimension at which a result's icon should be displayed.
  int GetPreferredIconDimension(SearchResultDisplayType display_type) const;

 private:
  friend class SharedAppListConfig;
  SharedAppListConfig();

  // The icon dimension of tile views in apps grid view.
  const int default_grid_icon_dimension_ = 64;

  // The badge icon dimension of a shortcut in apps grid view.
  // TODO(crbug.com/40281395): Update the size after the effects visual done.
  const int shortcut_badge_icon_dimension_ = 42;

  // Maximum number of results to show in the launcher Search UI.
  const size_t max_search_results_ = 6;

  // Max number of search result list items in the launcher suggestion window.
  const size_t max_search_result_list_items_ = 5;

  // Max number of Assistant search result list items in the launcher suggestion
  // window. Appears in the list after normal search results.
  const size_t max_assistant_search_result_list_items_ = 1;

  // The icon dimension of tile views in search result page view.
  const int search_tile_icon_dimension_ = 48;

  // The badge icon dimension of tile views in search result page view.
  const int search_tile_badge_icon_dimension_ = 20;

  // The badge icon offset of tile views in search result page view.
  const int search_tile_badge_icon_offset_ = 5;

  // The icon dimension of list views in search result page view.
  const int search_list_icon_dimension_ = 20;

  // The vertical bar icon dimension of list views in search result page view.
  const int search_list_icon_vertical_bar_dimension_ = 48;

  // The badge background corner radius of list views in search result page
  // view.
  const int search_list_badge_icon_dimension_ = 14;

  // The suggestion chip icon dimension.
  const int suggestion_chip_icon_dimension_ = 20;

  // The height of tiles in search result.
  const int search_tile_height_ = 92;

  // The maximum number of filtered results within categorical search
  const size_t max_results_with_categorical_search_ = 3;

  // The maximum number of filtered results of type answer card within
  // categorical search
  const int answer_card_max_results_ = 1;

  // The maximum number of results shown in the launcher image search category.
  const size_t image_search_max_results_ = 3;
};

// Contains app list layout information for an app list view. `AppListConfig`
// values depend on the context in which the app list is shown (e.g. the size of
// the display on which the app list is shown). `AppListConfig` instances are
// generally owned by the app list view, which creates them using
// `AppListConfigProvider` (defined in
// ash/public/cpp/app_list/app_list_config_provider.h).
class ASH_PUBLIC_EXPORT AppListConfig {
 public:
  // Constructor for unscaled configurations of the provided type.
  explicit AppListConfig(AppListConfigType type);

  // Constructor for scaled app list configuration.
  // `scale_x` - The scale at which apps grid tile should be scaled
  // horizontally.
  AppListConfig(const AppListConfig& base_config, float scale_x);

  AppListConfig(const AppListConfig&) = delete;
  AppListConfig& operator=(const AppListConfig&) = delete;

  ~AppListConfig();

  AppListConfigType type() const { return type_; }
  float scale_x() const { return scale_x_; }
  int grid_tile_width() const { return grid_tile_width_; }
  int grid_tile_height() const { return grid_tile_height_; }
  int grid_icon_dimension() const { return grid_icon_dimension_; }
  int grid_icon_bottom_padding() const { return grid_icon_bottom_padding_; }
  int grid_title_top_padding() const { return grid_title_top_padding_; }
  int grid_title_bottom_padding() const { return grid_title_bottom_padding_; }
  int grid_title_horizontal_padding() const {
    return grid_title_horizontal_padding_;
  }
  int grid_title_width() const { return grid_title_width_; }
  int grid_focus_corner_radius() const { return grid_focus_corner_radius_; }
  int app_title_max_line_height() const { return app_title_max_line_height_; }
  const gfx::FontList& app_title_font() const { return app_title_font_; }
  const gfx::FontList& item_counter_in_folder_icon_font() const {
    return item_counter_in_folder_icon_font_;
  }
  int folder_bubble_radius() const { return folder_bubble_radius_; }
  int icon_visible_dimension() const { return icon_visible_dimension_; }
  int folder_icon_dimension() const { return folder_icon_dimension_; }
  int folder_icon_radius() const { return folder_icon_radius_; }
  int icon_extended_background_dimension() const {
    return icon_extended_background_dimension_;
  }
  int icon_extended_background_radius() const {
    return icon_extended_background_radius_;
  }
  int item_icon_in_folder_icon_dimension() const {
    return item_icon_in_folder_icon_dimension_;
  }
  int item_icon_in_folder_icon_margin() const {
    return item_icon_in_folder_icon_margin_;
  }
  int shortcut_host_badge_icon_dimension() const {
    return shortcut_host_badge_icon_dimension_;
  }
  int shortcut_host_badge_icon_border_margin() const {
    return shortcut_host_badge_icon_border_margin_;
  }
  int shortcut_background_border_margin() const {
    return shortcut_background_border_margin_;
  }
  int promise_icon_dimension_installing() const {
    return promise_icon_dimension_installing_;
  }
  int promise_icon_dimension_pending() const {
    return promise_icon_dimension_pending_;
  }
  int GetShortcutHostBadgeIconContainerDimension() const;
  int GetShortcutBackgroundContainerDimension() const;
  int GetShortcutTeardropCornerRadius() const;

  gfx::Size GetShortcutIconSize() const;

  gfx::Size grid_icon_size() const {
    return gfx::Size(grid_icon_dimension_, grid_icon_dimension_);
  }

  gfx::Size icon_visible_size() const {
    return gfx::Size(icon_visible_dimension_, icon_visible_dimension_);
  }

  gfx::Size folder_icon_size() const {
    return gfx::Size(folder_icon_dimension_, folder_icon_dimension_);
  }

  gfx::Size item_icon_in_folder_icon_size() const {
    return gfx::Size(item_icon_in_folder_icon_dimension_,
                     item_icon_in_folder_icon_dimension_);
  }

 private:
  const AppListConfigType type_;

  // Current config scale values - should be different from 1 for
  // AppListConfigType::kShared only. Note that `scale_x_` should be always less
  // or equal to 1.
  const float scale_x_;

  // The tile view's width and height of the item in apps grid view.
  const int grid_tile_width_;
  const int grid_tile_height_;

  // The icon dimension of tile views in apps grid view.
  const int grid_icon_dimension_;

  // The icon bottom padding in tile views in apps grid view.
  const int grid_icon_bottom_padding_;

  // The title top, bottom and horizontal padding in tile views in apps grid
  // view.
  const int grid_title_top_padding_;
  const int grid_title_bottom_padding_;
  const int grid_title_horizontal_padding_;

  // The title width of tile views in apps grid view.
  const int grid_title_width_;

  // Corner radius of the focus ring for tile views in apps grid view.
  const int grid_focus_corner_radius_;

  // The maximum line height for app title in app list.
  const int app_title_max_line_height_;

  // The font for app title in app list.
  const gfx::FontList app_title_font_;

  // The font used to the number of apps in a folder on the fonder icon. Only
  // used if app collection folder icon refresh is enabled.
  const gfx::FontList item_counter_in_folder_icon_font_;

  // The radius of the circle in a folder icon (i.e. the gray circle underneath
  // the mini app icons).
  const int folder_bubble_radius_;

  // Because the nature of how an app is drawn, the visual size is slightly
  // smaller than its actual icon size. `icon_visible_dimension_` is used to
  // cache the visible size of an app. This is also the size of the folder icon
  // in its usual state (e.g. in the apps grid, not when the user is dragging an
  // item over it).
  const int icon_visible_dimension_;

  // The folder icon size.
  const int folder_icon_dimension_;

  // The corner radius of folder icon.
  const int folder_icon_radius_;

  // The width and height of background for item icons in extended state.
  const int icon_extended_background_dimension_;

  // The background corner radius of an item icon in extended state.
  const int icon_extended_background_radius_;

  // The dimension of the item icon in folder icon.
  const int item_icon_in_folder_icon_dimension_;

  // The margin between item icons inside a folder icon.
  const int item_icon_in_folder_icon_margin_;

  // The dimension of a host badge icon of a shortcut.
  const int shortcut_host_badge_icon_dimension_;

  // The dimension of a host badge icon container border of a shortcut.
  const int shortcut_host_badge_icon_border_margin_;

  // The bottom right corner radius of a shortcut.
  const int shortcut_teardrop_corner_radius_;

  // The dimension of a background container border of a shortcut.
  const int shortcut_background_border_margin_;

  // The preferred icon size for the promise apps on installing state.
  const int promise_icon_dimension_installing_;

  // The preferred icon size for the promise apps on pending state.
  const int promise_icon_dimension_pending_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_
