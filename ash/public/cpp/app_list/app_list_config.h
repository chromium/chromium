// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class FontList;
}

namespace ash {

// Shared layout type information for app list. Use the instance() method to
// obtain the AppListConfig.
class ASH_PUBLIC_EXPORT AppListConfig {
 public:
  // Constructor for unscaled configurations of the provided type.
  explicit AppListConfig(AppListConfigType type);

  // Constructor for scaled app list configuration.
  // Used only if kScalableAppList feature is not enabled, in which case the
  // app list configuration for small screens is created by scaling down
  // AppListConfigType::kShared configuration.
  //
  // |scale_x| - The scale at which apps grid tile should be scaled
  // horizontally.
  // |scale_y| - The scale at which apps grid tile should be scaled
  // vertically.
  // |inner_title_scale_y| - The scale to use to vertically scale dimensions
  // |min_y_scale| - Whether |scale_y| is the minimum scale allowed.
  // within the apps grid tile. Different from |scale_y| because tile title
  // height is not vertically scaled.
  AppListConfig(const AppListConfig& base_config,
                float scale_x,
                float scale_y,
                float inner_tile_scale_y,
                bool min_y_scale);
  ~AppListConfig();

  // Gets default app list configuration.
  static AppListConfig& instance();

  AppListConfigType type() const { return type_; }
  float scale_x() const { return scale_x_; }
  float scale_y() const { return scale_y_; }
  int grid_tile_width() const { return grid_tile_width_; }
  int grid_tile_height() const { return grid_tile_height_; }
  int grid_tile_spacing() const { return grid_tile_spacing_; }
  int grid_icon_dimension() const { return grid_icon_dimension_; }
  int grid_icon_bottom_padding() const { return grid_icon_bottom_padding_; }
  int grid_title_top_padding() const { return grid_title_top_padding_; }
  int grid_title_bottom_padding() const { return grid_title_bottom_padding_; }
  int grid_title_horizontal_padding() const {
    return grid_title_horizontal_padding_;
  }
  int grid_title_width() const { return grid_title_width_; }
  int grid_focus_dimension() const { return grid_focus_dimension_; }
  int grid_focus_corner_radius() const { return grid_focus_corner_radius_; }
  SkColor grid_title_color() const { return grid_title_color_; }
  int grid_fadeout_zone_height() const { return grid_fadeout_zone_height_; }
  int grid_fadeout_mask_height() const { return grid_fadeout_mask_height_; }
  int grid_to_page_switcher_margin() const {
    return grid_to_page_switcher_margin_;
  }
  int page_switcher_end_margin() const { return page_switcher_end_margin_; }
  int search_tile_icon_dimension() const { return search_tile_icon_dimension_; }
  int search_tile_badge_icon_dimension() const {
    return search_tile_badge_icon_dimension_;
  }
  int search_tile_badge_icon_offset() const {
    return search_tile_badge_icon_offset_;
  }
  int search_list_icon_dimension() const { return search_list_icon_dimension_; }
  int search_list_icon_vertical_bar_dimension() const {
    return search_list_icon_vertical_bar_dimension_;
  }
  int search_list_badge_icon_dimension() const {
    return search_list_badge_icon_dimension_;
  }
  int suggestion_chip_icon_dimension() const {
    return suggestion_chip_icon_dimension_;
  }
  int suggestion_chip_container_top_margin() const {
    return suggestion_chip_container_top_margin_;
  }
  int suggestion_chip_container_height() const {
    return suggestion_chip_container_height_;
  }
  int app_title_max_line_height() const { return app_title_max_line_height_; }
  const gfx::FontList& app_title_font() const { return app_title_font_; }
  int peeking_app_list_height() const { return peeking_app_list_height_; }
  int search_box_closed_top_padding() const {
    return search_box_closed_top_padding_;
  }
  int search_box_peeking_top_padding() const {
    return search_box_peeking_top_padding_;
  }
  int search_box_fullscreen_top_padding() const {
    return search_box_fullscreen_top_padding_;
  }
  int search_box_height() const { return search_box_height_; }
  int search_box_height_for_dense_layout() const {
    return search_box_height_for_dense_layout_;
  }
  int preferred_cols() const { return preferred_cols_; }
  int preferred_rows() const { return preferred_rows_; }
  int page_spacing() const { return page_spacing_; }
  int expand_arrow_tile_height() const { return expand_arrow_tile_height_; }
  int folder_bubble_radius() const { return folder_bubble_radius_; }
  int folder_bubble_y_offset() const { return folder_bubble_y_offset_; }
  int folder_header_height() const { return folder_header_height_; }
  int folder_header_min_width() const { return folder_header_min_width_; }
  int folder_header_max_width() const { return folder_header_max_width_; }
  int folder_header_min_tap_width() const {
    return folder_header_min_tap_width_;
  }
  int folder_name_border_radius() const { return folder_name_border_radius_; }
  int folder_name_border_thickness() const {
    return folder_name_border_thickness_;
  }
  int folder_name_padding() const { return folder_name_padding_; }
  int folder_icon_dimension() const { return folder_icon_dimension_; }
  int folder_unclipped_icon_dimension() const {
    return folder_unclipped_icon_dimension_;
  }
  int folder_icon_radius() const { return folder_icon_radius_; }
  int folder_background_radius() const { return folder_background_radius_; }
  int folder_bubble_color() const { return folder_bubble_color_; }
  int item_icon_in_folder_icon_dimension() const {
    return item_icon_in_folder_icon_dimension_;
  }
  int item_icon_in_folder_icon_margin() const {
    return item_icon_in_folder_icon_margin_;
  }
  int folder_dropping_circle_radius() const {
    return folder_dropping_circle_radius_;
  }
  int folder_dropping_delay() const { return folder_dropping_delay_; }
  SkColor folder_background_color() const { return folder_background_color_; }
  int page_flip_zone_size() const { return page_flip_zone_size_; }
  int grid_tile_spacing_in_folder() const {
    return grid_tile_spacing_in_folder_;
  }
  int blur_radius() const { return blur_radius_; }
  SkColor grid_selected_color() const { return grid_selected_color_; }
  base::TimeDelta page_transition_duration() const {
    return page_transition_duration_;
  }
  base::TimeDelta overscroll_page_transition_duration() const {
    return overscroll_page_transition_duration_;
  }
  base::TimeDelta folder_transition_in_duration() const {
    return folder_transition_in_duration_;
  }
  base::TimeDelta folder_transition_out_duration() const {
    return folder_transition_out_duration_;
  }
  size_t num_start_page_tiles() const { return num_start_page_tiles_; }
  size_t max_search_results() const { return max_search_results_; }
  size_t max_folder_pages() const { return max_folder_pages_; }
  size_t max_folder_items_per_page() const {
    return max_folder_items_per_page_;
  }
  size_t max_folder_name_chars() const { return max_folder_name_chars_; }
  float all_apps_opacity_start_px() const { return all_apps_opacity_start_px_; }
  float all_apps_opacity_end_px() const { return all_apps_opacity_end_px_; }
  ui::ResourceBundle::FontStyle search_result_title_font_style() const {
    return search_result_title_font_style_;
  }
  int search_tile_height() const { return search_tile_height_; }

  size_t max_search_result_tiles() const { return max_search_result_tiles_; }

  size_t max_search_result_list_items() const {
    return max_search_result_list_items_;
  }

  size_t max_assistant_search_result_list_items() const {
    return max_assistant_search_result_list_items_;
  }

  double privacy_container_score() const { return privacy_container_score_; }
  double app_tiles_container_score() const {
    return app_tiles_container_score_;
  }
  double results_list_container_score() const {
    return results_list_container_score_;
  }
  double answer_card_container_score() const {
    return answer_card_container_score_;
  }

  gfx::Size grid_icon_size() const {
    return gfx::Size(grid_icon_dimension_, grid_icon_dimension_);
  }

  gfx::Size grid_focus_size() const {
    return gfx::Size(grid_focus_dimension_, grid_focus_dimension_);
  }

  gfx::Size search_tile_icon_size() const {
    return gfx::Size(search_tile_icon_dimension_, search_tile_icon_dimension_);
  }

  gfx::Size search_tile_badge_icon_size() const {
    return gfx::Size(search_tile_badge_icon_dimension_,
                     search_tile_badge_icon_dimension_);
  }

  gfx::Size search_list_icon_size() const {
    return gfx::Size(search_list_icon_dimension_, search_list_icon_dimension_);
  }

  gfx::Size search_list_badge_icon_size() const {
    return gfx::Size(search_list_badge_icon_dimension_,
                     search_list_badge_icon_dimension_);
  }

  gfx::Size folder_icon_size() const {
    return gfx::Size(folder_icon_dimension_, folder_icon_dimension_);
  }

  gfx::Size folder_unclipped_icon_size() const {
    return gfx::Size(folder_unclipped_icon_dimension_,
                     folder_unclipped_icon_dimension_);
  }

  gfx::Insets folder_icon_insets() const {
    int folder_icon_dimension_diff =
        folder_unclipped_icon_dimension_ - folder_icon_dimension_;
    return gfx::Insets(folder_icon_dimension_diff / 2,
                       folder_icon_dimension_diff / 2,
                       (folder_icon_dimension_diff + 1) / 2,
                       (folder_icon_dimension_diff + 1) / 2);
  }

  gfx::Size item_icon_in_folder_icon_size() const {
    return gfx::Size(item_icon_in_folder_icon_dimension_,
                     item_icon_in_folder_icon_dimension_);
  }

  // Returns the dimension at which a result's icon should be displayed.
  int GetPreferredIconDimension(SearchResultDisplayType display_type) const;

  // Returns the maximum number of items allowed in specified page in apps grid.
  int GetMaxNumOfItemsPerPage(int page) const;

  // The minimal horizontal padding for the apps grid.
  int GetMinGridHorizontalPadding() const;

  // Returns the ideal apps container margins for the bounds available for app
  // list content.
  int GetIdealHorizontalMargin(const gfx::Rect& abailable_bounds) const;
  int GetIdealVerticalMargin(const gfx::Rect& abailable_bounds) const;

  // Returns the color and opacity for the page background.
  SkColor GetCardifiedBackgroundColor(bool is_active) const;

 private:
  const AppListConfigType type_;

  // Current config scale values - should be different from 1 for
  // AppListConfigType::kShared only.
  const float scale_x_;
  const float scale_y_;

  // The tile view's width and height of the item in apps grid view.
  const int grid_tile_width_;
  const int grid_tile_height_;

  // The spacing between tile views in apps grid view.
  const int grid_tile_spacing_;

  // The icon dimension of tile views in apps grid view.
  const int grid_icon_dimension_;

  // The icon bottom padding in tile views in apps grid view.
  const int grid_icon_bottom_padding_;

  // The title top, bottom and horizontal padding in tile views in apps grid
  // view.
  const int grid_title_top_padding_;
  const int grid_title_bottom_padding_;
  const int grid_title_horizontal_padding_;

  // The title width and color of tile views in apps grid view.
  const int grid_title_width_;
  const SkColor grid_title_color_;

  // The focus dimension and corner radius of tile views in apps grid view.
  const int grid_focus_dimension_;
  const int grid_focus_corner_radius_;

  // The vertical insets in the apps grid reserved for the grid fade out area.
  const int grid_fadeout_zone_height_;

  // The height of the masked area in the grid fade out zone.
  // This is different from |grid_fadeout_zone_height_|, which may include
  // additional margin outside the fadeout mask.
  const int grid_fadeout_mask_height_;

  // Horizontal margin between the apps grid and the page switcher UI.
  const int grid_to_page_switcher_margin_;

  // Minimal horizontal page switcher distance from the app list UI edge.
  const int page_switcher_end_margin_;

  // The icon dimension of tile views in search result page view.
  const int search_tile_icon_dimension_;

  // The badge icon dimension of tile views in search result page view.
  const int search_tile_badge_icon_dimension_;

  // The badge icon offset of tile views in search result page view.
  const int search_tile_badge_icon_offset_;

  // The icon dimension of list views in search result page view.
  const int search_list_icon_dimension_;

  // The vertical bar icon dimension of list views in search result page view.
  const int search_list_icon_vertical_bar_dimension_;

  // The badge background corner radius of list views in search result page
  // view.
  const int search_list_badge_icon_dimension_;

  // The suggestion chip icon dimension.
  const int suggestion_chip_icon_dimension_;

  // The suggestion chip container top margin.
  const int suggestion_chip_container_top_margin_;

  // The suggestion chip container height.
  const int suggestion_chip_container_height_;

  // The maximum line height for app title in app list.
  const int app_title_max_line_height_;

  // The font for app title in app list.
  const gfx::FontList app_title_font_;

  // The height of app list in peeking mode.
  const int peeking_app_list_height_;

  // The top padding of search box in closed state.
  const int search_box_closed_top_padding_;

  // The top padding of search box in peeking state.
  const int search_box_peeking_top_padding_;

  // The top padding of search box in fullscreen state.
  const int search_box_fullscreen_top_padding_;

  // The preferred search box height.
  const int search_box_height_;

  // The preferred search box height when the vertical app list contents space
  // is condensed - normally |search_box_height_| would be used.
  const int search_box_height_for_dense_layout_;

  // Preferred number of columns and rows in apps grid.
  const int preferred_cols_;
  const int preferred_rows_;

  // The spacing between each page.
  const int page_spacing_;

  // The tile height of expand arrow.
  const int expand_arrow_tile_height_;

  // The folder image bubble radius.
  const int folder_bubble_radius_;

  // The y offset of folder image bubble center.
  const int folder_bubble_y_offset_;

  // The height of the in folder name and pagination buttons.
  const int folder_header_height_;

  // The min and max widths of the folder name.
  const int folder_header_min_width_;
  const int folder_header_max_width_;

  // The min width of folder name for tap events.
  const int folder_header_min_tap_width_;

  // The border radius for folder name.
  const int folder_name_border_radius_;

  // The border thickness for folder name.
  const int folder_name_border_thickness_;

  // The inner padding for folder name.
  const int folder_name_padding_;

  // The icon dimension of folder.
  const int folder_icon_dimension_;

  // The unclipped icon dimension of folder.
  const int folder_unclipped_icon_dimension_;

  // The corner radius of folder icon.
  const int folder_icon_radius_;

  // The corner radius of folder background.
  const int folder_background_radius_;

  // The color of folder bubble.
  const int folder_bubble_color_;

  // The dimension of the item icon in folder icon.
  const int item_icon_in_folder_icon_dimension_;

  // The margin between item icons inside a folder icon.
  const int item_icon_in_folder_icon_margin_;

  // Radius of the circle, in which if entered, show folder dropping preview
  // UI.
  const int folder_dropping_circle_radius_;

  // Delays in milliseconds to show folder dropping preview circle.
  const int folder_dropping_delay_;

  // The background color of folder.
  const SkColor folder_background_color_;

  // Width in pixels of the area on the sides that triggers a page flip.
  const int page_flip_zone_size_;

  // The spacing between tile views in folder.
  const int grid_tile_spacing_in_folder_;

  // The blur radius used in the app list.
  const int blur_radius_;

  // The keyboard select color for grid views, which are on top of a black
  // shield view for new design (12% white).
  const SkColor grid_selected_color_;

  // Duration for page transition.
  const base::TimeDelta page_transition_duration_;

  // Duration for over scroll page transition.
  const base::TimeDelta overscroll_page_transition_duration_;

  // Duration for fading in the target page when opening
  // or closing a folder, and the duration for the top folder icon animation
  // for flying in or out the folder.
  const base::TimeDelta folder_transition_in_duration_;

  // Duration for fading out the old page when opening or
  // closing a folder.
  const base::TimeDelta folder_transition_out_duration_;

  // The number of apps shown in the start page app grid.
  const size_t num_start_page_tiles_;

  // Maximum number of results to show in the launcher Search UI.
  const size_t max_search_results_;

  // Max pages allowed in a folder.
  const size_t max_folder_pages_;

  // Max items per page allowed in a folder.
  const size_t max_folder_items_per_page_;

  // Maximum length of the folder name in chars.
  const size_t max_folder_name_chars_;

  // Range of the height of centerline above screen bottom that all apps should
  // change opacity. NOTE: this is used to change page switcher's opacity as
  // well.
  const float all_apps_opacity_start_px_ = 8.0f;
  const float all_apps_opacity_end_px_ = 144.0f;

  // Font style for AppListSearchResultTileItemViews that are not suggested
  // apps.
  const ui::ResourceBundle::FontStyle search_result_title_font_style_;

  // The height of tiles in search result.
  const int search_tile_height_ = 90;

  // Max number of search result tiles in the launcher suggestion window.
  const size_t max_search_result_tiles_ = 6;

  // Max number of search result list items in the launcher suggestion window.
  const size_t max_search_result_list_items_ = 5;

  // Max number of Assistant search result list items in the launcher suggestion
  // window. Appears in the list after normal search results.
  const size_t max_assistant_search_result_list_items_ = 1;

  // Scores for the containers within the search box view. These are displayed
  // in high-to-low order.
  // The privacy container is not always visible, but when available it should
  // always be the first item underneath the search box.
  const double privacy_container_score_ = 4.0;
  const double app_tiles_container_score_ = 3.0;
  const double answer_card_container_score_ = 2.0;
  const double results_list_container_score_ = 1.0;

  // Cardified app list background properties
  const SkColor cardified_background_color_;
  const SkColor cardified_background_color_active_;

  DISALLOW_COPY_AND_ASSIGN(AppListConfig);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_
