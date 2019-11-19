// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_CONFIG_H_
#define ASH_PUBLIC_CPP_SHELF_CONFIG_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display_observer.h"

namespace ash {

// Provides layout and drawing config for the Shelf. Note That some of these
// values could change at runtime.
class ASH_EXPORT ShelfConfig : public TabletModeObserver,
                               public AppListControllerObserver,
                               public display::DisplayObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when shelf config values are changed.
    virtual void OnShelfConfigUpdated() {}
  };

  ShelfConfig();
  ~ShelfConfig() override;

  static ShelfConfig* Get();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Add observers to this objects's dependencies.
  void Init();

  // Remove observers from this object's dependencies.
  void Shutdown();

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // AppListControllerObserver:
  void OnAppListVisibilityWillChange(bool shown, int64_t display_id) override;

  // Size of the shelf when visible (height when the shelf is horizontal and
  // width when the shelf is vertical).
  int shelf_size() const;

  // Size of the shelf when an app is visible in tablet mode.
  int in_app_shelf_size() const;

  // Size of the shelf when not in tablet mode, or when no apps are visible.
  int system_shelf_size() const;

  // Size of the hotseat, which contains the scrollable shelf in tablet mode.
  int hotseat_size() const;

  // The size of the gap between the hotseat and shelf when the hotseat is
  // extended.
  int hotseat_bottom_padding() const;

  // Size allocated for each app button on the shelf.
  int button_size() const;

  // Size of the space between buttons on the shelf.
  int button_spacing() const;

  // Size of the icons within shelf buttons.
  int button_icon_size() const;

  // Size for controls like the home button, back button, etc.
  int control_size() const;

  // The radius of shelf control buttons.
  int control_border_radius() const;

  // The margin around the overflow button on the shelf.
  int overflow_button_margin() const;

  // The distance between the edge of the shelf and the home and back button.
  int home_button_edge_spacing() const;

  // The duration of the hotseat background animations in ms.
  base::TimeDelta hotseat_background_animation_duration() const;

  // The extra padding added to status area tray buttons on the shelf.
  int status_area_hit_region_padding() const;

  // Returns whether we are within an app.
  bool is_in_app() const;

  int app_icon_group_margin() const { return app_icon_group_margin_; }
  SkColor shelf_control_permanent_highlight_background() const {
    return shelf_control_permanent_highlight_background_;
  }
  SkColor shelf_focus_border_color() const { return shelf_focus_border_color_; }
  int workspace_area_visible_inset() const {
    return workspace_area_visible_inset_;
  }
  int workspace_area_auto_hide_inset() const {
    return workspace_area_auto_hide_inset_;
  }
  int hidden_shelf_in_screen_portion() const {
    return hidden_shelf_in_screen_portion_;
  }
  SkColor shelf_ink_drop_base_color() const {
    return shelf_ink_drop_base_color_;
  }
  float shelf_ink_drop_visible_opacity() const {
    return shelf_ink_drop_visible_opacity_;
  }
  SkColor shelf_icon_color() const { return shelf_icon_color_; }
  int status_indicator_offset_from_shelf_edge() const {
    return status_indicator_offset_from_shelf_edge_;
  }
  int scrollable_shelf_ripple_padding() const {
    return scrollable_shelf_ripple_padding_;
  }
  int shelf_tooltip_preview_height() const {
    return shelf_tooltip_preview_height_;
  }
  int shelf_tooltip_preview_max_width() const {
    return shelf_tooltip_preview_max_width_;
  }
  float shelf_tooltip_preview_max_ratio() const {
    return shelf_tooltip_preview_max_ratio_;
  }
  float shelf_tooltip_preview_min_ratio() const {
    return shelf_tooltip_preview_min_ratio_;
  }
  int shelf_blur_radius() const { return shelf_blur_radius_; }
  int mousewheel_scroll_offset_threshold() const {
    return mousewheel_scroll_offset_threshold_;
  }

  bool is_dense() const { return is_dense_; }

  // Gets the current color for the shelf control buttons.
  SkColor GetShelfControlButtonColor() const;

  // Gets the shelf color when the app list is open, used in clamshell mode.
  SkColor GetShelfWithAppListColor() const;

  // Gets the shelf color when a window is maximized.
  SkColor GetMaximizedShelfColor() const;

  // Gets the default shelf color, calculated using the wallpaper color if
  // available.
  SkColor GetDefaultShelfColor() const;

  // Returns the current blur radius to use for the control buttons.
  int GetShelfControlButtonBlurRadius() const;

 private:
  friend class ShelfConfigTest;

  // Called whenever something has changed in the shelf configuration. Notifies
  // all observers.
  void OnShelfConfigUpdated();

  // Updates |is_dense_| and notifies all observers of the update.
  void UpdateIsDense();

  // Gets the current shelf size.
  // |ignore_in_app_state| - Whether the returned shelf size should be
  //                         calculated as if is_in_app() returns false.
  int GetShelfSize(bool ignore_in_app_state) const;

  // Whether shelf is currently standard or dense.
  bool is_dense_;

  // Whether the app list (or home launcher in tablet mode) is visible.
  bool is_app_list_visible_;

  // Size of the icons within shelf buttons.
  const int shelf_button_icon_size_;
  const int shelf_button_icon_size_dense_;

  // Size allocated for each app button on the shelf.
  const int shelf_button_size_;
  const int shelf_button_size_dense_;

  // Size of the space between buttons on the shelf.
  const int shelf_button_spacing_;

  // The extra padding added to status area tray buttons on the shelf.
  const int shelf_status_area_hit_region_padding_;
  const int shelf_status_area_hit_region_padding_dense_;

  // The margin on either side of the group of app icons (including the overflow
  // button).
  const int app_icon_group_margin_;

  const SkColor shelf_control_permanent_highlight_background_;

  const SkColor shelf_focus_border_color_;

  // We reserve a small area on the edge of the workspace area to ensure that
  // the resize handle at the edge of the window can be hit.
  const int workspace_area_visible_inset_;

  // When autohidden we extend the touch hit target onto the screen so that the
  // user can drag the shelf out.
  const int workspace_area_auto_hide_inset_;

  // Portion of the shelf that's within the screen bounds when auto-hidden.
  const int hidden_shelf_in_screen_portion_;

  // Ink drop color for shelf items.
  const SkColor shelf_ink_drop_base_color_;

  // Opacity of the ink drop ripple for shelf items when the ripple is visible.
  const float shelf_ink_drop_visible_opacity_;

  // The foreground color of the icons used in the shelf (launcher,
  // notifications, etc).
  const SkColor shelf_icon_color_;

  // The distance between the edge of the shelf and the status indicators.
  const int status_indicator_offset_from_shelf_edge_;

  // Padding between the shelf container view and the edging app icon in order
  // to show the app icon's ripple correctly.
  const int scrollable_shelf_ripple_padding_;

  // Dimensions for hover previews.
  const int shelf_tooltip_preview_height_;
  const int shelf_tooltip_preview_max_width_;
  const float shelf_tooltip_preview_max_ratio_;
  const float shelf_tooltip_preview_min_ratio_;

  // The blur radius used for the shelf.
  const int shelf_blur_radius_;

  // The threshold at which mousewheel and touchpad scrolls are either ignored
  // or acted upon.
  const int mousewheel_scroll_offset_threshold_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ShelfConfig);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_CONFIG_H_
