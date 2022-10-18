// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_config.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"

namespace ash {

namespace {

// Used in as a value in histogram to record the reason shelf navigation buttons
// are shown in tablet mode.
// The values assigned to enum items should not be changed/reassigned.
constexpr int kControlButtonsShownForShelfNavigationButtonsSetting = 1;
constexpr int kControlButtonsShownForSpokenFeedback = 1 << 1;
constexpr int kControlButtonsShownForSwitchAccess = 1 << 2;
constexpr int kControlButtonsShownForAutoclick = 1 << 3;
constexpr int kControlButtonsShownReasonCount = 1 << 4;

// When any edge of the primary display is less than or equal to this threshold,
// dense shelf will be active.
constexpr int kDenseShelfScreenSizeThreshold = 600;

// Drags on the shelf that are greater than this number times the shelf size
// will trigger shelf visibility changes.
constexpr float kDragHideRatioThreshold = 0.4f;

constexpr int kSystemShelfSizeTabletModeDense = 48;
constexpr int kSystemShelfSizeTabletModeNormal = 56;

// Records the histogram value tracking the reason shelf control buttons are
// shown in tablet mode.
void RecordReasonForShowingShelfControls() {
  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();
  int buttons_shown_reason_mask = 0;

  if (accessibility_controller
          ->tablet_mode_shelf_navigation_buttons_enabled()) {
    buttons_shown_reason_mask |=
        kControlButtonsShownForShelfNavigationButtonsSetting;
  }

  if (accessibility_controller->spoken_feedback().enabled())
    buttons_shown_reason_mask |= kControlButtonsShownForSpokenFeedback;

  if (accessibility_controller->switch_access().enabled())
    buttons_shown_reason_mask |= kControlButtonsShownForSwitchAccess;

  if (accessibility_controller->autoclick().enabled())
    buttons_shown_reason_mask |= kControlButtonsShownForAutoclick;

  base::UmaHistogramExactLinear(
      "Ash.Shelf.NavigationButtonsInTabletMode.ReasonShown",
      buttons_shown_reason_mask, kControlButtonsShownReasonCount);
}

int IsDenseForCurrentScreen() {
  const gfx::Rect screen_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  return screen_size.width() <= kDenseShelfScreenSizeThreshold ||
         screen_size.height() <= kDenseShelfScreenSizeThreshold;
}

}  // namespace

class ShelfConfig::ShelfAccessibilityObserver : public AccessibilityObserver {
 public:
  explicit ShelfAccessibilityObserver(
      const base::RepeatingClosure& accessibility_state_changed_callback)
      : accessibility_state_changed_callback_(
            accessibility_state_changed_callback) {
    observation_.Observe(Shell::Get()->accessibility_controller());
  }

  ShelfAccessibilityObserver(const ShelfAccessibilityObserver& other) = delete;
  ShelfAccessibilityObserver& operator=(
      const ShelfAccessibilityObserver& other) = delete;

  ~ShelfAccessibilityObserver() override = default;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override {
    accessibility_state_changed_callback_.Run();
  }
  void OnAccessibilityControllerShutdown() override { observation_.Reset(); }

 private:
  base::RepeatingClosure accessibility_state_changed_callback_;

  base::ScopedObservation<AccessibilityControllerImpl, AccessibilityObserver>
      observation_{this};
};

class ShelfConfig::ShelfSplitViewObserver : public SplitViewObserver {
 public:
  explicit ShelfSplitViewObserver(
      SplitViewController* controller,
      const base::RepeatingCallback<void(SplitViewController::State,
                                         SplitViewController::State)>&
          split_view_state_changed_callback)
      : split_view_state_changed_callback_(split_view_state_changed_callback) {
    observation_.Observe(controller);
  }

  ShelfSplitViewObserver(const ShelfSplitViewObserver& other) = delete;
  ShelfSplitViewObserver& operator=(const ShelfSplitViewObserver& other) =
      delete;

  ~ShelfSplitViewObserver() override = default;
  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override {
    split_view_state_changed_callback_.Run(previous_state, state);
  }

 private:
  base::RepeatingCallback<void(SplitViewController::State,
                               SplitViewController::State)>
      split_view_state_changed_callback_;

  base::ScopedObservation<SplitViewController, SplitViewObserver> observation_{
      this};
};

ShelfConfig::ShelfConfig()
    : use_in_app_shelf_in_overview_(false),
      overview_mode_(false),
      in_tablet_mode_(false),
      is_dense_(false),
      is_in_app_(true),
      in_split_view_with_overview_(false),
      shelf_controls_shown_(true),
      is_virtual_keyboard_shown_(false),
      is_app_list_visible_(false),
      shelf_button_icon_size_(44),
      shelf_button_icon_size_median_(40),
      shelf_button_icon_size_dense_(36),
      shelf_button_size_(56),
      shelf_button_size_median_(52),
      shelf_button_size_dense_(48),
      shelf_button_spacing_(8),
      shelf_status_area_hit_region_padding_(4),
      shelf_status_area_hit_region_padding_dense_(2),
      app_icon_group_margin_tablet_(16),
      app_icon_group_margin_clamshell_(12),
      workspace_area_visible_inset_(2),
      workspace_area_auto_hide_inset_(5),
      hidden_shelf_in_screen_portion_(3),
      status_indicator_offset_from_shelf_edge_(1),
      scrollable_shelf_ripple_padding_(2),
      shelf_tooltip_preview_height_(128),
      shelf_tooltip_preview_max_width_(192),
      shelf_tooltip_preview_max_ratio_(1.5),    // = 3/2
      shelf_tooltip_preview_min_ratio_(0.666),  // = 2/3
      shelf_blur_radius_(30),
      mousewheel_scroll_offset_threshold_(20),
      in_app_control_button_height_inset_(4),
      app_icon_end_padding_(4) {
  accessibility_observer_ = std::make_unique<ShelfAccessibilityObserver>(
      base::BindRepeating(&ShelfConfig::UpdateConfigForAccessibilityState,
                          base::Unretained(this)));
}

ShelfConfig::~ShelfConfig() = default;

// static
ShelfConfig* ShelfConfig::Get() {
  return Shell::HasInstance() ? Shell::Get()->shelf_config() : nullptr;
}

void ShelfConfig::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShelfConfig::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ShelfConfig::Init() {
  Shell* const shell = Shell::Get();

  shell->app_list_controller()->AddObserver(this);
  display_observer_.emplace(this);
  shell->system_tray_model()->virtual_keyboard()->AddObserver(this);
  shell->overview_controller()->AddObserver(this);
  shell->session_controller()->AddObserver(this);

  shell->tablet_mode_controller()->AddObserver(this);
  in_tablet_mode_ = shell->IsInTabletMode();
  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/false);
}

void ShelfConfig::Shutdown() {
  Shell* const shell = Shell::Get();
  shell->tablet_mode_controller()->RemoveObserver(this);

  shell->session_controller()->RemoveObserver(this);
  shell->overview_controller()->RemoveObserver(this);
  shell->system_tray_model()->virtual_keyboard()->RemoveObserver(this);
  display_observer_.reset();
  shell->app_list_controller()->RemoveObserver(this);
}

void ShelfConfig::OnOverviewModeWillStart() {
  DCHECK(!overview_mode_);
  use_in_app_shelf_in_overview_ = is_in_app_;
  overview_mode_ = true;
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  in_split_view_with_overview_ = split_view_controller->InSplitViewMode();

  split_view_observer_ = std::make_unique<ShelfSplitViewObserver>(
      split_view_controller,
      base::BindRepeating(&ShelfConfig::OnSplitViewStateChanged,
                          base::Unretained(this)));
}

void ShelfConfig::OnOverviewModeEnding(OverviewSession* overview_session) {
  split_view_observer_.reset();
  overview_mode_ = false;
  in_split_view_with_overview_ = false;
  use_in_app_shelf_in_overview_ = false;
  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/false);
}

void ShelfConfig::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  in_split_view_with_overview_ = (state != SplitViewController::State::kNoSnap);
  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/false);
}

void ShelfConfig::OnTabletModeStarting() {
  // Update the shelf config at the "starting" stage of the tablet mode
  // transition, so that the shelf bounds are set and remains stable during the
  // transition animation. Otherwise, updating the shelf bounds during the
  // animation will lead to work-area bounds changes which lead to many
  // re-layouts, hurting the animation's smoothness. https://crbug.com/1044316.
  DCHECK(!in_tablet_mode_);
  in_tablet_mode_ = true;

  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/true);
}

void ShelfConfig::OnSessionStateChanged(session_manager::SessionState state) {
  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/false);
}

void ShelfConfig::OnTabletModeEnding() {
  // Many events can lead to UpdateConfig being called as a result of
  // OnTabletModeEnded(), therefore we need to listen to the "ending" stage
  // rather than the "ended", so |in_tablet_mode_| gets updated correctly, and
  // the shelf bounds are stabilized early so as not to have multiple
  // unnecessary work-area bounds changes.
  in_tablet_mode_ = false;

  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/true);
}

void ShelfConfig::OnDisplayMetricsChanged(const display::Display& display,
                                          uint32_t changed_metrics) {
  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/false);
}

void ShelfConfig::OnVirtualKeyboardVisibilityChanged() {
  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/false);
}

void ShelfConfig::OnAppListVisibilityWillChange(bool shown,
                                                int64_t display_id) {
  // Let's check that the app visibility mechanism isn't mis-firing, which
  // would lead to a lot of extraneous relayout work.
  DCHECK_NE(is_app_list_visible_, shown);

  UpdateConfig(/*new_is_app_list_visible=*/shown,
               /*tablet_mode_changed=*/false);
}

bool ShelfConfig::ShelfControlsForcedShownForAccessibility() const {
  auto* accessibility_controller = Shell::Get()->accessibility_controller();
  return accessibility_controller->spoken_feedback().enabled() ||
         accessibility_controller->autoclick().enabled() ||
         accessibility_controller->switch_access().enabled() ||
         accessibility_controller
             ->tablet_mode_shelf_navigation_buttons_enabled();
}

int ShelfConfig::GetShelfButtonSize(HotseatDensity density) const {
  if (is_dense_)
    return shelf_button_size_dense_;

  switch (density) {
    case HotseatDensity::kNormal:
      return shelf_button_size_;
    case HotseatDensity::kSemiDense:
      return shelf_button_size_median_;
    case HotseatDensity::kDense:
      return shelf_button_size_dense_;
  }
}

int ShelfConfig::GetShelfButtonIconSize(HotseatDensity density) const {
  if (is_dense_)
    return shelf_button_icon_size_dense_;

  switch (density) {
    case HotseatDensity::kNormal:
      return shelf_button_icon_size_;
    case HotseatDensity::kSemiDense:
      return shelf_button_icon_size_median_;
    case HotseatDensity::kDense:
      return shelf_button_icon_size_dense_;
  }
}

int ShelfConfig::GetHotseatSize(HotseatDensity density) const {
  if (!in_tablet_mode_)
    return shelf_size();

  return GetShelfButtonSize(density);
}

int ShelfConfig::shelf_size() const {
  return GetShelfSize(false /*ignore_in_app_state*/);
}

int ShelfConfig::in_app_shelf_size() const {
  return is_dense_ ? 36 : 40;
}

int ShelfConfig::system_shelf_size() const {
  return GetShelfSize(true /*ignore_in_app_state*/);
}

int ShelfConfig::shelf_drag_handle_centering_size() const {
  const session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  return session_state == session_manager::SessionState::ACTIVE
             ? in_app_shelf_size()
             : 28;
}

int ShelfConfig::hotseat_bottom_padding() const {
  return 8;
}

int ShelfConfig::button_spacing() const {
  return shelf_button_spacing_;
}

int ShelfConfig::control_size() const {
  if (!in_tablet_mode_)
    return 36;

  return is_dense_ ? 36 : 40;
}

int ShelfConfig::control_border_radius() const {
  return (is_in_app_ && in_tablet_mode_)
             ? control_size() / 2 - in_app_control_button_height_inset_
             : control_size() / 2;
}

int ShelfConfig::control_button_edge_spacing(bool is_primary_axis_edge) const {
  if (is_primary_axis_edge)
    return in_tablet_mode_ ? 8 : 6;

  return (shelf_size() - control_size()) / 2;
}

base::TimeDelta ShelfConfig::hotseat_background_animation_duration() const {
  // This matches the duration of the maximize/minimize animation.
  return base::Milliseconds(300);
}

base::TimeDelta ShelfConfig::shelf_animation_duration() const {
  return hotseat_background_animation_duration();
}

int ShelfConfig::status_area_hit_region_padding() const {
  return is_dense_ ? shelf_status_area_hit_region_padding_dense_
                   : shelf_status_area_hit_region_padding_;
}

float ShelfConfig::drag_hide_ratio_threshold() const {
  return kDragHideRatioThreshold;
}

void ShelfConfig::UpdateConfig(bool new_is_app_list_visible,
                               bool tablet_mode_changed) {
  const bool new_is_dense = !in_tablet_mode_ || IsDenseForCurrentScreen();

  const bool can_hide_shelf_controls =
      in_tablet_mode_ && features::IsHideShelfControlsInTabletModeEnabled();
  const bool new_shelf_controls_shown =
      !can_hide_shelf_controls || ShelfControlsForcedShownForAccessibility();
  // Record reason to show shelf control buttons only if tablet mode changes, or
  // if the buttons visibility state changes
  if (can_hide_shelf_controls && new_shelf_controls_shown &&
      (tablet_mode_changed || !shelf_controls_shown_)) {
    RecordReasonForShowingShelfControls();
  }

  // TODO(https://crbug.com/1058205): Test this behavior.
  // If the virtual keyboard is shown, the back button and in-app shelf should
  // be shown so users can exit the keyboard. SystemTrayModel may be null in
  // tests.
  const bool new_is_virtual_keyboard_shown = Shell::Get()->system_tray_model()
                                                 ? Shell::Get()
                                                       ->system_tray_model()
                                                       ->virtual_keyboard()
                                                       ->arc_keyboard_visible()
                                                 : false;

  const bool new_is_in_app =
      CalculateIsInApp(new_is_app_list_visible, new_is_virtual_keyboard_shown);

  const bool changed =
      tablet_mode_changed || is_dense_ != new_is_dense ||
      is_in_app_ != new_is_in_app ||
      shelf_controls_shown_ != new_shelf_controls_shown ||
      is_virtual_keyboard_shown_ != new_is_virtual_keyboard_shown ||
      is_app_list_visible_ != new_is_app_list_visible;

  if (!changed)
    return;

  is_dense_ = new_is_dense;
  shelf_controls_shown_ = new_shelf_controls_shown;
  is_virtual_keyboard_shown_ = new_is_virtual_keyboard_shown;
  is_app_list_visible_ = new_is_app_list_visible;
  is_in_app_ = new_is_in_app;

  OnShelfConfigUpdated();
}

int ShelfConfig::GetShelfSize(bool ignore_in_app_state) const {
  // In clamshell mode, the shelf always has the same size.
  if (!in_tablet_mode_)
    return 48;

  // Use in app shelf when split view is enabled.
  if (!ignore_in_app_state && (is_in_app_ || in_split_view_with_overview_))
    return in_app_shelf_size();

  return is_dense_ ? kSystemShelfSizeTabletModeDense
                   : kSystemShelfSizeTabletModeNormal;
}

SkColor ShelfConfig::GetShelfControlButtonColor(
    const views::Widget* widget) const {
  DCHECK(widget);

  const session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();

  if (in_tablet_mode_ &&
      session_state == session_manager::SessionState::ACTIVE) {
    return is_in_app_ ? SK_ColorTRANSPARENT : GetDefaultShelfColor(widget);
  }
  if (!features::IsDarkLightModeEnabled() &&
      session_state == session_manager::SessionState::OOBE) {
    return SkColorSetA(SK_ColorBLACK, 16);  // 6% opacity
  }
  return widget->GetColorProvider()->GetColor(
      kColorAshControlBackgroundColorInactive);
}

SkColor ShelfConfig::GetMaximizedShelfColor(const views::Widget* widget) const {
  return SkColorSetA(GetDefaultShelfColor(widget), 0xFF);  // 100% opacity
}

ui::ColorId ShelfConfig::GetShelfBaseLayerColorId() const {
  if (!in_tablet_mode_)
    return kColorAshShieldAndBase80;

  if (!is_in_app_)
    return kColorAshShieldAndBase60;

  return DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
             ? kColorAshShieldAndBase90
             : kColorAshShieldAndBaseOpaque;
}

SkColor ShelfConfig::GetDefaultShelfColor(const views::Widget* widget) const {
  DCHECK(widget);

  const auto* color_provider = widget->GetColorProvider();
  if (!features::IsBackgroundBlurEnabled())
    return color_provider->GetColor(kColorAshShieldAndBase90);

  return color_provider->GetColor(GetShelfBaseLayerColorId());
}

int ShelfConfig::GetShelfControlButtonBlurRadius() const {
  if (features::IsBackgroundBlurEnabled() && in_tablet_mode_ && !is_in_app_)
    return shelf_blur_radius_;
  return 0;
}

int ShelfConfig::GetAppIconEndPadding() const {
  return app_icon_end_padding_;
}

int ShelfConfig::GetAppIconGroupMargin() const {
  return in_tablet_mode_ ? app_icon_group_margin_tablet_
                         : app_icon_group_margin_clamshell_;
}

base::TimeDelta ShelfConfig::DimAnimationDuration() const {
  return base::Milliseconds(1000);
}

gfx::Tween::Type ShelfConfig::DimAnimationTween() const {
  return gfx::Tween::LINEAR;
}

gfx::Size ShelfConfig::DragHandleSize() const {
  const session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  return session_state == session_manager::SessionState::ACTIVE
             ? gfx::Size(80, 4)
             : gfx::Size(120, 4);
}

int ShelfConfig::GetSystemShelfSizeInTabletMode() const {
  // Note that existing `is_dense_` takes in account current tablet/clamshell
  // mode, but sometimes there is a need to get shelf size in tablet mode
  // staying in clamshell mode.
  return IsDenseForCurrentScreen() ? kSystemShelfSizeTabletModeDense
                                   : kSystemShelfSizeTabletModeNormal;
}

void ShelfConfig::UpdateConfigForAccessibilityState() {
  UpdateConfig(is_app_list_visible_, /*tablet_mode_changed=*/false);
}

bool ShelfConfig::CalculateIsInApp(bool app_list_visible,
                                   bool virtual_keyboard_shown) const {
  Shell* shell = Shell::Get();
  const auto* session = shell->session_controller();
  if (!session ||
      session->GetSessionState() != session_manager::SessionState::ACTIVE) {
    return false;
  }
  if (virtual_keyboard_shown)
    return true;
  if (app_list_visible)
    return false;
  if (in_split_view_with_overview_)
    return true;
  if (overview_mode_)
    return use_in_app_shelf_in_overview_;
  return true;
}

void ShelfConfig::OnShelfConfigUpdated() {
  for (auto& observer : observers_)
    observer.OnShelfConfigUpdated();
}

}  // namespace ash
