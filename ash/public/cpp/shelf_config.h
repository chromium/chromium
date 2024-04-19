// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_CONFIG_H_
#define ASH_PUBLIC_CPP_SHELF_CONFIG_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/animation/tween.h"

namespace base {
class TimeDelta;
}

namespace session_manager {
enum class SessionState;
}  // namespace session_manager

namespace ash {

// Provides layout and drawing config for the Shelf. Note That some of these
// values could change at runtime.
class ASH_EXPORT ShelfConfig : public SessionObserver,
                               public AppListControllerObserver,
                               public display::DisplayObserver,
                               public VirtualKeyboardModel::Observer,
                               public OverviewObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when shelf config values are changed.
    virtual void OnShelfConfigUpdated() {}
  };

  ShelfConfig();

  ShelfConfig(const ShelfConfig&) = delete;
  ShelfConfig& operator=(const ShelfConfig&) = delete;

  ~ShelfConfig() override;

  static ShelfConfig* Get();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Add observers to this objects's dependencies.
  void Init();

  // Remove observers from this object's dependencies.
  void Shutdown();

  // OverviewObserver:
  void OnOverviewModeWillStart() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;

  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // VirtualKeyboardModel::Observer:
  void OnVirtualKeyboardVisibilityChanged() override;

  // AppListControllerObserver:
  void OnAppListVisibilityWillChange(bool shown, int64_t display_id) override;

  // Updates the shelf configuration to match the provided tablet mode state.
  // Called during transitions to enter or exit tablet mode.
  void UpdateForTabletMode(bool in_tablet_mode);

  // Whether the shelf control buttons must be shown for accessibility
  // reasons.
  bool ShelfControlsForcedShownForAccessibility() const;

  // Returns the optimal shelf button size for the given hotseat density.
  int GetShelfButtonSize(HotseatDensity density) const;

  // Returns the optimal shelf icon size for the given hotseat density.
  int GetShelfButtonIconSize(HotseatDensity density) const;

  // Returns the shelf shortuct icon size.
  int GetShelfShortcutIconSize() const;

  // Returns the shelf shortcut icon border size.
  int GetShelfShortcutIconBorderSize() const;

  // Returns the shelf shortcut host badge icon size.
  int GetShelfShortcutHostBadgeIconSize() const;

  // Returns the shelf shortcut host badge icon border size.
  int GetShelfShortcutHostBadgeBorderSize() const;

  // Returns the shelf shortcut bottom right corner radius.
  int GetShelfShortcutTeardropCornerRadiusSize() const;

  // Returns the hotseat height for the given hotseat density.
  // NOTE: This may not match the actual hotseat size, as hotseat may get scaled
  // down if it does not fit in available bounds within the shelf. Use
  // HotseatWidget::GetHotseatSize() to get the actual widget size.
  int GetHotseatSize(HotseatDensity density) const;

  // Returns the padding between the shelf and elevated homecher.
  int GetHomecherElevatedAppBarOffset() const;

  // Size of the shelf when visible (height when the shelf is horizontal and
  // width when the shelf is vertical).
  int shelf_size() const;

  // Size of the shelf when an app is visible in tablet mode.
  int in_app_shelf_size() const;

  // Size of the shelf when not in tablet mode, or when no apps are visible.
  int system_shelf_size() const;

  // The shelf size within which the drag handle should be centered.
  int shelf_drag_handle_centering_size() const;

  // The size of the gap between the hotseat and shelf when the hotseat is
  // extended.
  int hotseat_bottom_padding() const;

  // Size of the space between buttons on the shelf.
  int button_spacing() const;

  // Size for controls like the home button, back button, etc.
  int control_size() const;

  // The radius of shelf control buttons.
  int control_border_radius() const;

  // The spacing between the edge of the shelf and the control buttons. When
  // shelf is horizontal, the left/right edges of the shelf are considered a
  // primary axis edge. When shelf is vertical, the top/bottom edges are
  // considered the primary axis edge.
  int control_button_edge_spacing(bool is_primary_axis_edge) const;

  // The duration of the hotseat background animations in ms.
  base::TimeDelta hotseat_background_animation_duration() const;

  // The duration of the shelf show/hide animation in ms.
  base::TimeDelta shelf_animation_duration() const;

  // The extra padding added to status area tray buttons on the shelf.
  int status_area_hit_region_padding() const;

  // The threshold relative to the size of the shelf that is used to determine
  // if the shelf visibility should change during a drag.
  float drag_hide_ratio_threshold() const;

  int workspace_area_visible_inset() const {
    return workspace_area_visible_inset_;
  }
  int workspace_area_auto_hide_inset() const {
    return workspace_area_auto_hide_inset_;
  }
  int hidden_shelf_in_screen_portion() const {
    return hidden_shelf_in_screen_portion_;
  }
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
  int in_app_control_button_height_inset() const {
    return in_app_control_button_height_inset_;
  }

  bool is_dense() const { return is_dense_; }

  bool is_in_app() const { return is_in_app_; }

  bool in_split_view_with_overview() const {
    return in_split_view_with_overview_;
  }

  bool shelf_controls_shown() const { return shelf_controls_shown_; }

  bool is_virtual_keyboard_shown() const { return is_virtual_keyboard_shown_; }

  bool in_tablet_mode() const { return in_tablet_mode_; }

  bool in_overview_mode() const { return overview_mode_; }

  bool elevate_tablet_mode_app_bar() const {
    return elevate_tablet_mode_app_bar_;
  }

  // Gets the current color for the shelf control buttons.
  SkColor GetShelfControlButtonColor(const views::Widget* widget) const;

  // Gets the shelf color when a window is maximized.
  SkColor GetMaximizedShelfColor(const views::Widget* widget) const;

  // Gets the ColorId for shelf color.
  ui::ColorId GetShelfBaseLayerColorId() const;

  // Gets the default shelf color, calculated using the wallpaper color if
  // available.
  SkColor GetDefaultShelfColor(const views::Widget* widget) const;

  // Returns the current blur radius to use for the control buttons.
  int GetShelfControlButtonBlurRadius() const;

  // The padding between the app icon and the end of the scrollable shelf.
  int GetAppIconEndPadding() const;

  // Returns the margin on either side of the group of app icons.
  int GetAppIconGroupMargin() const;

  // The animation time for dimming shelf icons, widgets, and buttons.
  base::TimeDelta DimAnimationDuration() const;

  // The tween type for dimming shelf icons, widgets, and buttons.
  gfx::Tween::Type DimAnimationTween() const;

  // The size of the shelf drag handle.
  gfx::Size DragHandleSize() const;

  // Size of the shelf in tablet mode.
  int GetSystemShelfSizeInTabletMode() const;

  // Records the UMA of showing the stacked hotseat app bar and returns the size
  // of the insets used in tablet mode to allocate space to the shelf.
  int GetTabletModeShelfInsetsAndRecordUMA();

  // Minimum size for the inline app bar.
  int GetMinimumInlineAppBarSize() const;

  // Updates 'elevate_tablet_mode_app_bar_' for `inline_app_bar_size`.
  void UpdateShowElevatedAppBar(const gfx::Size& inline_app_bar_size);

 private:
  friend class ShelfConfigTest;

  class ShelfAccessibilityObserver;
  class ShelfSplitViewObserver;

  // Called whenever something has changed in the shelf configuration. Notifies
  // all observers.
  void OnShelfConfigUpdated();

  // Updates |is_dense_|, |is_app_list_visible_|, and |shelf_controls_shown_|
  // and notifies all observers of the update if the state changes.
  // |new_is_app_list_visible| - The new app list visibility state.
  // |tablet_mode_changed| should be set to true if this config is being updated
  // as a result of a change in tablet mode state.
  void UpdateConfig(bool new_is_app_list_visible, bool tablet_mode_changed);

  // Gets the current shelf size.
  // |ignore_in_app_state| - Whether the returned shelf size should be
  //                         calculated as if is_in_app() returns false.
  int GetShelfSize(bool ignore_in_app_state) const;

  // Updates shelf config - called when the accessibility state changes.
  void UpdateConfigForAccessibilityState();

  // Calculates the intended in-app state with the provided app list and virtual
  // keyboard visibility.
  bool CalculateIsInApp(bool app_list_visible,
                        bool virtual_keyboard_shown) const;

  // Whether an elevated app bar has been rendered (stacked hotseat). This
  // boolean is used for logging UMA metrics.
  std::optional<bool> has_shown_elevated_app_bar_;

  // Whether tablet mode homecher should use elevated app bar.
  bool elevate_tablet_mode_app_bar_ = false;

  // Whether the in app shelf should be shown in overview mode.
  bool use_in_app_shelf_in_overview_ = false;

  // True if device is currently in overview mode.
  bool overview_mode_ = false;

  // True if device is currently in tablet mode.
  bool in_tablet_mode_ = false;

  // Whether shelf is currently standard or dense.
  bool is_dense_ = false;

  // Whether the shelf is currently in in-app state.
  bool is_in_app_ = false;

  // Whether the device is in split view. Only tracked in overview mode.
  bool in_split_view_with_overview_ = false;

  // Whether the shelf buttons (navigation controls, and overview tray button)
  // should be shown.
  bool shelf_controls_shown_ = false;

  // Whether virtual IME keyboard is shown.
  bool is_virtual_keyboard_shown_ = false;

  // Whether the app list (or home launcher in tablet mode) is visible.
  bool is_app_list_visible_ = false;

  // Size of the icons within shelf buttons.
  const int shelf_button_icon_size_;
  const int shelf_button_icon_size_median_;
  const int shelf_button_icon_size_dense_;

  // Size of the shortcut icon.
  const int shelf_shortcut_icon_size_;

  // Size of the shortcut icon border.
  const int shelf_shortcut_icon_border_size_;

  // Size of the shortcut host badge icon.
  const int shelf_shortcut_host_badge_icon_size_;

  // Size of the shortcut host badge border.
  const int shelf_shortcut_host_badge_border_size_;

  // Size of the bottom right corner radius of a shortcut.
  const int shelf_shortcut_teardrop_corner_radius_;

  // Size allocated for each app button on the shelf.
  const int shelf_button_size_;
  const int shelf_button_size_median_;
  const int shelf_button_size_dense_;

  // Size of the space between buttons on the shelf.
  const int shelf_button_spacing_;

  // The extra padding added to status area tray buttons on the shelf.
  const int shelf_status_area_hit_region_padding_;
  const int shelf_status_area_hit_region_padding_dense_;

  // The margin on either side of the group of app icons in tablet/clamshell.
  const int app_icon_group_margin_tablet_;
  const int app_icon_group_margin_clamshell_;

  // We reserve a small area on the edge of the workspace area to ensure that
  // the resize handle at the edge of the window can be hit.
  const int workspace_area_visible_inset_;

  // When autohidden we extend the touch hit target onto the screen so that the
  // user can drag the shelf out.
  const int workspace_area_auto_hide_inset_;

  // Portion of the shelf that's within the screen bounds when auto-hidden.
  const int hidden_shelf_in_screen_portion_;

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

  // The height inset on the control buttons when in-app shelf is shown.
  const int in_app_control_button_height_inset_;

  // The padding between the app icon and the end of the scrollable shelf in
  // tablet mode.
  const int app_icon_end_padding_;

  // Object responsible for observing accessibility settings relevant to shelf
  // config.
  std::unique_ptr<ShelfAccessibilityObserver> accessibility_observer_;

  // Object responsible for observing overview mode split state changes relevant
  // to shelf config.
  std::unique_ptr<ShelfSplitViewObserver> split_view_observer_;

  // Receive callbacks from DisplayObserver.
  display::ScopedDisplayObserver display_observer_{this};

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_CONFIG_H_
