// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_view.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_event_targeter.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/ime_util_chromeos.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// Default color for classic, unthemed app list.
constexpr SkColor kAppListBackgroundColor = gfx::kGoogleGrey900;

// The height of the peeking app list, measured from the bottom of the screen.
constexpr int kPeekingHeight = 284;

// The height of the half app list, measured from the bottom of the screen.
constexpr int kHalfHeight = 545;

// The DIP distance from the bezel in which a gesture drag end results in a
// closed app list.
constexpr int kAppListBezelMargin = 50;

// The size of app info dialog in fullscreen app list.
constexpr int kAppInfoDialogWidth = 512;
constexpr int kAppInfoDialogHeight = 384;

// The duration of app list animations when they should run immediately.
constexpr int kAppListAnimationDurationImmediateMs = 0;

// Histogram for the app list dragging in clamshell mode.
constexpr char kAppListDragInClamshellHistogram[] =
    "Apps.StateTransition.Drag.PresentationTime.ClamshellMode";
constexpr char kAppListDragInClamshellMaxLatencyHistogram[] =
    "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode";

// The number of minutes that must pass for the current app list page to reset
// to the first page.
constexpr int kAppListPageResetTimeLimitMinutes = 20;

// When true, immdeidately fires the page reset timer upon starting.
bool skip_page_reset_timer_for_testing = false;

// Returns whether AppList's rounded corners should be hidden based on
// the app list view state and app list view bounds.
bool ShouldHideRoundedCorners(ash::AppListViewState app_list_state,
                              const gfx::Rect& bounds) {
  switch (app_list_state) {
    case ash::AppListViewState::kClosed:
      return false;
    case ash::AppListViewState::kFullscreenAllApps:
    case ash::AppListViewState::kFullscreenSearch:
      // Hide rounded corners in fullscreen state.
      return true;

    case ash::AppListViewState::kPeeking:
    case ash::AppListViewState::kHalf:
      // When the virtual keyboard shows, the AppListView is moved upward to
      // avoid the overlapping area with the virtual keyboard. As a result, its
      // bottom side may be on the display edge. Stop showing the rounded
      // corners under this circumstance.
      return bounds.y() == 0;
  }
  NOTREACHED();
  return false;
}

// This view forwards the focus to the search box widget by providing it as a
// FocusTraversable when a focus search is provided.
class SearchBoxFocusHost : public views::View {
 public:
  explicit SearchBoxFocusHost(views::Widget* search_box_widget)
      : search_box_widget_(search_box_widget) {}

  SearchBoxFocusHost(const SearchBoxFocusHost&) = delete;
  SearchBoxFocusHost& operator=(const SearchBoxFocusHost&) = delete;

  ~SearchBoxFocusHost() override = default;

  views::FocusTraversable* GetFocusTraversable() override {
    if (search_box_widget_->IsVisible())
      return search_box_widget_;
    return nullptr;
  }

  // views::View:
  const char* GetClassName() const override { return "SearchBoxFocusHost"; }

 private:
  views::Widget* search_box_widget_;
};

SkColor GetBackgroundShieldColor(const std::vector<SkColor>& colors,
                                 float color_opacity,
                                 bool is_tablet_mode) {
  const U8CPU sk_opacity_value = static_cast<U8CPU>(255 * color_opacity);
  SkColor default_color =
      SkColorSetA(kAppListBackgroundColor, sk_opacity_value);

  if (!colors.empty()) {
    DCHECK_EQ(static_cast<size_t>(ColorProfileType::NUM_OF_COLOR_PROFILES),
              colors.size());
    const SkColor dark_muted =
        colors[static_cast<int>(ColorProfileType::DARK_MUTED)];
    if (SK_ColorTRANSPARENT != dark_muted) {
      default_color = SkColorSetA(
          color_utils::GetResultingPaintColor(
              SkColorSetA(SK_ColorBLACK, AppListView::kAppListColorDarkenAlpha),
              dark_muted),
          sk_opacity_value);
    }
  }

  return SkColorSetA(AppListColorProvider::Get()->GetAppListBackgroundColor(
                         is_tablet_mode, default_color),
                     sk_opacity_value);
}

// Gets radius for app list background corners when the app list has the
// provided height. The rounded corner should match the current app list height
// (so the rounded corners bottom edge matches the shelf top), until it reaches
// the app list background radius (i.e. background radius in peeking app list
// state).
// |height|: App list view height, relative to the shelf top (i.e. distance
//           between app list top and shelf top edge).
double GetBackgroundRadiusForAppListHeight(double height,
                                           int shelf_background_corner_radius) {
  return std::min(static_cast<double>(shelf_background_corner_radius),
                  std::max(height, 0.));
}

float ComputeSubpixelOffset(const display::Display& display, float value) {
  float pixel_position = std::round(display.device_scale_factor() * value);
  float dp_position = pixel_position / display.device_scale_factor();
  return dp_position - std::floor(value);
}

}  // namespace

// AppListView::ScopedContentsResetDisabler ------------------------------------

AppListView::ScopedContentsResetDisabler::ScopedContentsResetDisabler(
    AppListView* view)
    : view_(view) {
  DCHECK(!view_->disable_contents_reset_when_showing_);
  view_->disable_contents_reset_when_showing_ = true;
}

AppListView::ScopedContentsResetDisabler::~ScopedContentsResetDisabler() {
  DCHECK(view_->disable_contents_reset_when_showing_);
  view_->disable_contents_reset_when_showing_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// AppListView::StateAnimationMetricsReporter

class AppListView::StateAnimationMetricsReporter {
 public:
  StateAnimationMetricsReporter() = default;
  StateAnimationMetricsReporter(const StateAnimationMetricsReporter&) = delete;
  StateAnimationMetricsReporter& operator=(
      const StateAnimationMetricsReporter&) = delete;
  ~StateAnimationMetricsReporter() = default;

  // Sets target state of the transition for metrics.
  void SetTargetState(AppListViewState target_state) {
    target_state_ = target_state;
  }

  // Sets tablet animation transition type for metrics.
  void SetTabletModeAnimationTransition(
      TabletModeAnimationTransition transition) {
    tablet_transition_ = transition;
  }

  // Resets the target state and animation type for metrics.
  void Reset();

  // Gets a callback to report smoothness.
  metrics_util::SmoothnessCallback GetReportCallback(bool tablet_mode) {
    if (tablet_mode) {
      return base::BindRepeating(
          &StateAnimationMetricsReporter::RecordMetricsInTablet,
          std::move(tablet_transition_));
    }
    return base::BindRepeating(
        &StateAnimationMetricsReporter::RecordMetricsInClamshell,
        std::move(target_state_));
  }

 private:
  static void RecordMetricsInTablet(
      absl::optional<TabletModeAnimationTransition> transition,
      int value);
  static void RecordMetricsInClamshell(
      absl::optional<AppListViewState> target_state,
      int value);

  absl::optional<AppListViewState> target_state_;
  absl::optional<TabletModeAnimationTransition> tablet_transition_;
};

void AppListView::StateAnimationMetricsReporter::Reset() {
  tablet_transition_.reset();
  target_state_.reset();
}

// static
void AppListView::StateAnimationMetricsReporter::RecordMetricsInTablet(
    absl::optional<TabletModeAnimationTransition> tablet_transition,
    int value) {
  UMA_HISTOGRAM_PERCENTAGE("Apps.StateTransition.AnimationSmoothness", value);

  // It can't ensure the target transition is properly set. Simply give up
  // reporting per-state metrics in that case. See https://crbug.com/954907.
  if (!tablet_transition)
    return;
  switch (*tablet_transition) {
    case TabletModeAnimationTransition::kDragReleaseShow:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness.DragReleaseShow",
          value);
      break;
    case TabletModeAnimationTransition::kDragReleaseHide:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "DragReleaseHide",
          value);
      break;
    case TabletModeAnimationTransition::kHomeButtonShow:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "PressAppListButtonShow",
          value);
      break;
    case TabletModeAnimationTransition::kHideHomeLauncherForWindow:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "HideLauncherForWindow",
          value);
      break;
    case TabletModeAnimationTransition::kEnterFullscreenAllApps:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "EnterFullscreenAllApps",
          value);
      break;
    case TabletModeAnimationTransition::kEnterFullscreenSearch:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "EnterFullscreenSearch",
          value);
      break;
    case TabletModeAnimationTransition::kFadeInOverview:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness.FadeInOverview",
          value);
      break;
    case TabletModeAnimationTransition::kFadeOutOverview:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness.FadeOutOverview",
          value);
      break;
  }
}

// static
void AppListView::StateAnimationMetricsReporter::RecordMetricsInClamshell(
    absl::optional<AppListViewState> target_state,
    int value) {
  UMA_HISTOGRAM_PERCENTAGE("Apps.StateTransition.AnimationSmoothness", value);

  // It can't ensure the target transition is properly set. Simply give up
  // reporting per-state metrics in that case. See https://crbug.com/954907.
  if (!target_state)
    return;

  switch (*target_state) {
    case AppListViewState::kClosed:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.Close.ClamshellMode",
          value);
      break;
    case AppListViewState::kPeeking:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.Peeking.ClamshellMode",
          value);
      break;
    case AppListViewState::kHalf:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.Half.ClamshellMode", value);
      break;
    case AppListViewState::kFullscreenAllApps:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.FullscreenAllApps."
          "ClamshellMode",
          value);
      break;
    case AppListViewState::kFullscreenSearch:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.FullscreenSearch."
          "ClamshellMode",
          value);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// An animation observer to notify AppListView when animations for an app list
// view state transition complete. The observer goes through the following
// states:
// 1. kIdle
// 2. kReady, once `Reset()` has been called, and target app list state has been
//    set.
// 3. kActive, once `Activate()` has been called.
// 4. kTransitionDone, once `SetTransitionDone()` has been called.
//    *   `SetTransitionDone()` gets called when observed implicit animation
//        complete, but can be called directly if the app list view state is
//        updated without animation.
// 5. kIdle, once the app list view has been notified that the transition has
//    complete.
//
// Note that 3. and 4. may happen out of order - app list view will only be
// notified of transition completion when both steps are complete. The goal is
// to ensure that state transition notification is not sent out prematurely,
// before the internal app list view state is updated.
class StateTransitionNotifier : public ui::ImplicitAnimationObserver {
 public:
  explicit StateTransitionNotifier(AppListView* view) : view_(view) {}

  StateTransitionNotifier(const StateTransitionNotifier&) = delete;
  StateTransitionNotifier& operator=(const StateTransitionNotifier&) = delete;

  ~StateTransitionNotifier() override = default;

  // Resets the notifier, and set a new target app list state.
  void Reset(AppListViewState target_app_list_state) {
    StopObservingImplicitAnimations();

    state_ = State::kReady;
    target_app_list_view_state_ = target_app_list_state;
  }

  // Activates the notifier - moves the notifier in the state where it can
  // notify the app list view of state transition completion.
  // NOTE: If the app list state transition has already completed, the app list
  // view will get notified immediately.
  void Activate() {
    DCHECK(target_app_list_view_state_.has_value());

    if (state_ == State::kTransitionDone) {
      NotifyTransitionCompleted();
      return;
    }

    DCHECK_EQ(state_, State::kReady);
    state_ = State::kActive;
  }

  // Marks the app list view state transition as completed. If the notifier is
  // active, it will notify the app list view of the transition completion.
  // NOTE: This should be called directly only if the notifier is not added as a
  // transition animation observer. If the notifier is observing the animation,
  // this method gets called on the animation completion.
  void SetTransitionDone() {
    DCHECK_NE(state_, State::kTransitionDone);
    DCHECK_NE(state_, State::kIdle);

    const bool can_notify = state_ == State::kActive;
    state_ = State::kTransitionDone;

    if (can_notify)
      NotifyTransitionCompleted();
  }

 private:
  enum class State { kIdle, kReady, kActive, kTransitionDone };

  // Overridden from ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    StopObservingImplicitAnimations();

    TRACE_EVENT_NESTABLE_ASYNC_END1("ui", "AppList::StateTransitionAnimations",
                                    this, "state",
                                    target_app_list_view_state_.value());
    SetTransitionDone();
  }

  void NotifyTransitionCompleted() {
    DCHECK_EQ(state_, State::kTransitionDone);

    state_ = State::kIdle;

    AppListViewState app_list_state = *target_app_list_view_state_;
    target_app_list_view_state_ = absl::nullopt;
    view_->OnBoundsAnimationCompleted(app_list_state);
  }

  State state_ = State::kIdle;
  AppListView* const view_;
  absl::optional<AppListViewState> target_app_list_view_state_;
};

// The view for the app list background shield which changes color and radius.
class AppListBackgroundShieldView : public views::View {
 public:
  explicit AppListBackgroundShieldView(int shelf_background_corner_radius,
                                       bool is_tablet_mode)
      : color_(AppListColorProvider::Get()->GetAppListBackgroundColor(
            is_tablet_mode,
            /*default_color*/ kAppListBackgroundColor)),
        shelf_background_corner_radius_(shelf_background_corner_radius) {
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetFillsBoundsOpaquely(false);
    SetBackgroundRadius(shelf_background_corner_radius_);
    layer()->SetColor(color_);
    layer()->SetName("launcher/BackgroundShield");
  }

  AppListBackgroundShieldView(const AppListBackgroundShieldView&) = delete;
  AppListBackgroundShieldView& operator=(const AppListBackgroundShieldView&) =
      delete;

  ~AppListBackgroundShieldView() override = default;

  void UpdateBackground(bool use_blur) {
    if (use_blur_ == use_blur)
      return;
    use_blur_ = use_blur;

    if (use_blur) {
      layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
      layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    } else {
      layer()->SetBackgroundBlur(0);
    }
  }

  void UpdateBackgroundRadius(
      AppListViewState state,
      bool shelf_has_rounded_corners,
      absl::optional<base::TimeTicks> animation_end_timestamp) {
    const float target_corner_radius =
        (state == AppListViewState::kClosed && !shelf_has_rounded_corners)
            ? 0
            : shelf_background_corner_radius_;
    if (corner_radius_ == target_corner_radius)
      return;

    layer()->GetAnimator()->StopAnimatingProperty(
        ui::LayerAnimationElement::ROUNDED_CORNERS);

    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (animation_end_timestamp.has_value()) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          layer()->GetAnimator());
      settings->SetTransitionDuration((*animation_end_timestamp) -
                                      base::TimeTicks::Now());
      settings->SetTweenType(gfx::Tween::EASE_OUT);
      settings->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    }
    SetBackgroundRadius(target_corner_radius);
  }

  void SetBackgroundRadius(float corner_radius) {
    corner_radius_ = corner_radius;
    layer()->SetRoundedCornerRadius({corner_radius, corner_radius, 0, 0});
  }

  void UpdateColor(SkColor color) {
    if (color_ == color)
      return;

    color_ = color;
    layer()->SetColor(color);
  }

  void UpdateBounds(const gfx::Rect& bounds) {
    // Inset bottom by 2 * the background radius to account for the rounded
    // corners on the top and bottom of the |app_list_background_shield_|. Only
    // add the inset to the bottom to keep padding at the top of the AppList the
    // same.
    gfx::Rect new_bounds = bounds;
    new_bounds.Inset(
        gfx::Insets::TLBR(0, 0, -shelf_background_corner_radius_ * 2, 0));
    SetBoundsRect(new_bounds);
  }

  SkColor GetColorForTest() const { return color_; }

  const char* GetClassName() const override {
    return "AppListBackgroundShieldView";
  }

 private:
  // Whether the background blur has been set on the background shield.
  bool use_blur_ = false;

  float corner_radius_ = 0.0f;

  SkColor color_;

  int shelf_background_corner_radius_ = 0;
};

////////////////////////////////////////////////////////////////////////////////
// AppListView::TestApi

AppListView::TestApi::TestApi(AppListView* view) : view_(view) {
  DCHECK(view_);
}

AppListView::TestApi::~TestApi() = default;

PagedAppsGridView* AppListView::TestApi::GetRootAppsGridView() {
  return view_->GetRootAppsGridView();
}

////////////////////////////////////////////////////////////////////////////////
// AppListView:

AppListView::AppListView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      is_background_blur_enabled_(features::IsBackgroundBlurEnabled()),
      state_transition_notifier_(
          std::make_unique<StateTransitionNotifier>(this)),
      state_animation_metrics_reporter_(
          std::make_unique<StateAnimationMetricsReporter>()) {
  CHECK(delegate);
  // Default role of WidgetDelegate is ax::mojom::Role::kWindow which traps
  // ChromeVox focus within the root view. Assign ax::mojom::Role::kGroup here
  // to allow the focus to move from elements in app list view to search box.
  // TODO(pbos): Should this be necessary with the OverrideNextFocus() used
  // below?
  SetAccessibleRole(ax::mojom::Role::kGroup);
}

AppListView::~AppListView() {
  // Shutdown a11y announcer before the announcement view gets removed.
  a11y_announcer_->Shutdown();

  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews();
}

// static
float AppListView::GetTransitionProgressForState(AppListViewState state) {
  switch (state) {
    case AppListViewState::kClosed:
      return 0.0f;
    case AppListViewState::kPeeking:
    case AppListViewState::kHalf:
      return 1.0f;
    case AppListViewState::kFullscreenAllApps:
    case AppListViewState::kFullscreenSearch:
      return 2.0f;
  }
  NOTREACHED();
  return 0.0f;
}

// static
void AppListView::SetSkipPageResetTimerForTesting(bool enabled) {
  skip_page_reset_timer_for_testing = enabled;
}

void AppListView::InitView(gfx::NativeView parent) {
  base::AutoReset<bool> auto_reset(&is_building_, true);
  time_shown_ = base::Time::Now();
  InitContents();
  InitWidget(parent);
  InitChildWidget();
}

void AppListView::InitContents() {
  DCHECK(!app_list_background_shield_);
  DCHECK(!app_list_main_view_);
  DCHECK(!search_box_view_);

  auto app_list_background_shield =
      std::make_unique<AppListBackgroundShieldView>(
          delegate_->GetShelfSize() / 2, delegate_->IsInTabletMode());
  app_list_background_shield->UpdateBackground(
      /*use_blur*/ !delegate_->IsInTabletMode() && is_background_blur_enabled_);
  app_list_background_shield_ =
      AddChildView(std::move(app_list_background_shield));

  a11y_announcer_ = std::make_unique<AppListA11yAnnouncer>(
      AddChildView(std::make_unique<views::View>()));

  auto app_list_main_view = std::make_unique<AppListMainView>(delegate_, this);
  search_box_view_ =
      new SearchBoxView(app_list_main_view.get(), delegate_, this);
  SearchBoxViewBase::InitParams params;
  params.show_close_button_when_active = true;
  params.create_background = true;
  params.animate_changing_search_icon = true;
  search_box_view_->Init(params);

  // Assign |app_list_main_view_| here since it is accessed during Init().
  app_list_main_view_ = app_list_main_view.get();
  app_list_main_view->Init(0, search_box_view_);
  AddChildView(std::move(app_list_main_view));
}

void AppListView::InitWidget(gfx::NativeView parent) {
  DCHECK(!GetWidget());
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "AppList";
  params.parent = parent;
  params.delegate = this;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  DCHECK_EQ(widget, GetWidget());
  widget->GetNativeWindow()->SetEventTargeter(
      std::make_unique<AppListEventTargeter>(delegate_));

  // Enable arrow key. Arrow left/right and up/down triggers the same focus
  // movement as tab/shift+tab.
  SetEnableArrowKeyTraversal(true);

  widget->GetNativeView()->AddObserver(this);

  // Directs A11y focus ring from search box view to AppListView's descendants
  // (like ExpandArrowView) without focusing on the whole app list window when
  // using search + arrow button.
  search_box_view_->GetViewAccessibility().OverrideNextFocus(GetWidget());
  search_box_view_->GetViewAccessibility().OverridePreviousFocus(GetWidget());
}

void AppListView::InitChildWidget() {
  // Create a widget for the SearchBoxView to live in. This allows the
  // SearchBoxView to be on top of the custom launcher page's WebContents
  // (otherwise the search box events will be captured by the WebContents).
  views::Widget::InitParams search_box_widget_params(
      views::Widget::InitParams::TYPE_CONTROL);
  search_box_widget_params.parent = GetWidget()->GetNativeView();
  search_box_widget_params.opacity =
      views::Widget::InitParams::WindowOpacity::kTranslucent;
  search_box_widget_params.name = "SearchBoxView";

  // Focus should be able to move from search box to items in app list view.
  auto widget_delegate = std::make_unique<views::WidgetDelegate>();
  widget_delegate->SetFocusTraversesOut(true);

  // Default role of root view is ax::mojom::Role::kWindow which traps
  // ChromeVox focus within the root view. Assign ax::mojom::Role::kGroup here
  // to allow the focus to move from elements in search box to app list view.
  widget_delegate->SetAccessibleRole(ax::mojom::Role::kGroup);

  // SearchBoxView used to be a WidgetDelegateView, so we follow the legacy
  // behavior and have the Widget delete the delegate.
  widget_delegate->SetOwnedByWidget(true);
  search_box_widget_params.delegate = widget_delegate.release();

  views::Widget* search_box_widget = new views::Widget;
  search_box_widget->Init(std::move(search_box_widget_params));
  search_box_widget->SetContentsView(search_box_view_);
  search_box_view_->MaybeCreateFocusRing();
  DCHECK_EQ(search_box_widget, search_box_view_->GetWidget());

  // Assign an accessibility role to the native window of |search_box_widget|,
  // so that hitting search+right could move ChromeVox focus across search box
  // to other elements in app list view.
  search_box_widget->GetNativeWindow()->SetProperty(
      ui::kAXRoleOverride,
      static_cast<ax::mojom::Role>(ax::mojom::Role::kGroup));

  // The search box will not naturally receive focus by itself (because it is in
  // a separate widget). Create this SearchBoxFocusHost in the main widget to
  // forward the focus search into to the search box.
  SearchBoxFocusHost* search_box_focus_host =
      new SearchBoxFocusHost(search_box_widget);
  AddChildView(search_box_focus_host);
  search_box_widget->SetFocusTraversableParentView(search_box_focus_host);
  search_box_widget->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());

  // Directs A11y focus ring from AppListView's descendants (like
  // ExpandArrowView) to search box view without focusing on the whole app list
  // window when using search + arrow button.
  GetViewAccessibility().OverrideNextFocus(search_box_widget);
  GetViewAccessibility().OverridePreviousFocus(search_box_widget);
}

void AppListView::Show(AppListViewState preferred_state, bool is_side_shelf) {
  if (!time_shown_.has_value())
    time_shown_ = base::Time::Now();
  // The opacity of the AppListView may have been manipulated by overview mode,
  // so reset it before it is shown.
  GetWidget()->GetLayer()->SetOpacity(1.0f);
  is_side_shelf_ = is_side_shelf;

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));

  UpdateWidget();

  if (!disable_contents_reset_when_showing_) {
    app_list_main_view_->contents_view()->ResetForShow();
    if (!delegate_->IsInTabletMode())
      SelectInitialAppsPage();
  }

  SetState(preferred_state);

  // Ensures that the launcher won't open underneath the a11y keyboard.
  CloseKeyboardIfVisible();

  OnTabletModeChanged(delegate_->IsInTabletMode());
  app_list_main_view_->ShowAppListWhenReady();

  UMA_HISTOGRAM_TIMES("Apps.AppListCreationTime",
                      base::Time::Now() - time_shown_.value());
  time_shown_ = absl::nullopt;
}

void AppListView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  app_list_main_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListView::Dismiss() {
  CloseKeyboardIfVisible();
  delegate_->DismissAppList();
}

void AppListView::CloseOpenedPage() {
  if (HandleCloseOpenFolder())
    return;

  HandleCloseOpenSearchBox();
}

bool AppListView::HandleCloseOpenFolder() {
  if (GetAppsContainerView()->IsInFolderView()) {
    GetAppsContainerView()->app_list_folder_view()->CloseFolderPage();
    return true;
  }
  return false;
}

bool AppListView::HandleCloseOpenSearchBox() {
  if (app_list_main_view_ &&
      app_list_main_view_->contents_view()->IsShowingSearchResults()) {
    return Back();
  }
  return false;
}

bool AppListView::Back() {
  if (app_list_main_view_)
    return app_list_main_view_->contents_view()->Back();

  return false;
}

void AppListView::OnPaint(gfx::Canvas* canvas) {
  views::WidgetDelegateView::OnPaint(canvas);
}

const char* AppListView::GetClassName() const {
  return "AppListView";
}

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
    case ui::VKEY_BROWSER_BACK:
      // If the ContentsView does not handle the back action, then this is the
      // top level, so we close the app list.
      if (!Back() && !delegate_->IsInTabletMode())
        Dismiss();
      break;
    default:
      NOTREACHED();
      return false;
  }

  // Don't let DialogClientView handle the accelerator.
  return true;
}

void AppListView::Layout() {
  // Avoid layout while building the view.
  if (is_building_)
    return;

  // Avoid layout during animations.
  if (GetWidget()->GetLayer()->GetAnimator() &&
      GetWidget()->GetLayer()->GetAnimator()->is_animating()) {
    return;
  }

  const gfx::Rect contents_bounds = GetContentsBounds();

  // Exclude the shelf size from the contents bounds to avoid apps grid from
  // overlapping with shelf.
  gfx::Rect main_bounds = contents_bounds;
  main_bounds.Inset(GetMainViewInsetsForShelf());

  app_list_main_view_->SetBoundsRect(main_bounds);

  app_list_background_shield_->UpdateBounds(contents_bounds);

  UpdateAppListBackgroundYPosition(target_app_list_state_);
}

void AppListView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackgroundShieldColor();
}

views::View* AppListView::GetAppListBackgroundShieldForTest() {
  return app_list_background_shield_;
}

SkColor AppListView::GetAppListBackgroundShieldColorForTest() {
  return app_list_background_shield_->GetColorForTest();
}

bool AppListView::IsShowingEmbeddedAssistantUI() const {
  return app_list_main_view()->contents_view()->IsShowingEmbeddedAssistantUI();
}

bool AppListView::IsFolderBeingRenamed() {
  return GetAppsContainerView()
      ->app_list_folder_view()
      ->folder_header_view()
      ->HasTextFocus();
}

void AppListView::UpdatePageResetTimer(bool app_list_visibility) {
  if (app_list_visibility || !delegate_->IsInTabletMode()) {
    page_reset_timer_.Stop();
    return;
  }
  page_reset_timer_.Start(FROM_HERE,
                          base::Minutes(kAppListPageResetTimeLimitMinutes),
                          this, &AppListView::SelectInitialAppsPage);

  if (skip_page_reset_timer_for_testing)
    page_reset_timer_.FireNow();
}

gfx::Insets AppListView::GetMainViewInsetsForShelf() const {
  if (is_side_shelf()) {
    // Set both horizontal insets so the app list remains centered on the
    // screen.
    return gfx::Insets::VH(0, delegate_->GetShelfSize());
  }
  return gfx::Insets::TLBR(0, 0, delegate_->GetShelfSize(), 0);
}

void AppListView::UpdateWidget() {
  // The widget's initial position will be off the bottom of the display.
  // Set native view's bounds directly to avoid screen position controller
  // setting bounds in the display where the widget has the largest
  // intersection.
  GetWidget()->GetNativeView()->SetBounds(
      GetPreferredWidgetBoundsForState(AppListViewState::kClosed));
  ResetSubpixelPositionOffset(GetWidget()->GetNativeView()->layer());
}

void AppListView::HandleClickOrTap(ui::LocatedEvent* event) {
  // If the virtual keyboard is visible, dismiss the keyboard. If there is some
  // text in the search box or the embedded assistant UI is shown, return early
  // so they don't get closed.
  if (CloseKeyboardIfVisible()) {
    search_box_view_->NotifyGestureEvent();
    if (search_box_view_->HasSearch() || IsShowingEmbeddedAssistantUI())
      return;
  }

  // Close embedded Assistant UI if it is shown.
  if (IsShowingEmbeddedAssistantUI()) {
    Back();
    search_box_view_->ClearSearchAndDeactivateSearchBox();
    return;
  }

  // Clear focus if the located event is not handled by any child view.
  GetFocusManager()->ClearFocus();

  if (GetAppsContainerView()->IsInFolderView()) {
    // Close the folder if it is opened.
    GetAppsContainerView()->app_list_folder_view()->CloseFolderPage();
    return;
  }

  if ((event->IsGestureEvent() &&
       (event->AsGestureEvent()->type() == ui::ET_GESTURE_LONG_PRESS ||
        event->AsGestureEvent()->type() == ui::ET_GESTURE_LONG_TAP ||
        event->AsGestureEvent()->type() == ui::ET_GESTURE_TWO_FINGER_TAP)) ||
      (event->IsMouseEvent() &&
       event->AsMouseEvent()->IsOnlyRightMouseButton())) {
    // Don't show menus on empty areas of the AppListView in clamshell mode.
    if (!delegate_->IsInTabletMode())
      return;

    // Home launcher is shown on top of wallpaper with transparent background.
    // So trigger the wallpaper context menu for the same events.
    gfx::Point onscreen_location(event->location());
    ConvertPointToScreen(this, &onscreen_location);
    delegate_->ShowWallpaperContextMenu(
        onscreen_location, event->IsGestureEvent() ? ui::MENU_SOURCE_TOUCH
                                                   : ui::MENU_SOURCE_MOUSE);
    return;
  }

  if (!search_box_view_->is_search_box_active() &&
      delegate_->GetCurrentAppListPage() !=
          AppListState::kStateEmbeddedAssistant) {
    if (!delegate_->IsInTabletMode())
      Dismiss();
    return;
  }

  search_box_view_->ClearSearchAndDeactivateSearchBox();
}

void AppListView::StartDrag(const gfx::PointF& location_in_root) {
  initial_drag_point_ = location_in_root;

  drag_offset_ =
      initial_drag_point_.y() - GetWidget()->GetNativeWindow()->bounds().y();
}

void AppListView::UpdateDrag(const gfx::PointF& location_in_root) {
  float new_y_position_in_root = location_in_root.y() - drag_offset_;

  UpdateYPositionAndOpacity(new_y_position_in_root,
                            GetAppListBackgroundOpacityDuringDragging());
}

void AppListView::EndDrag(const gfx::PointF& location_in_root) {
  // |is_in_drag_| might have been cleared if the app list was dismissed while
  // drag was still in progress. Nothing to do here in that case.
  if (!is_in_drag_) {
    DCHECK_EQ(AppListViewState::kClosed, app_list_state_);
    return;
  }

  // Remember the last fling velocity, as the value gets reset in SetIsInDrag.
  const int last_fling_velocity = last_fling_velocity_;
  SetIsInDrag(false);

  // Change the app list state based on where the drag ended. If fling velocity
  // was over the threshold, snap to the next state in the direction of the
  // fling.
  if (std::abs(last_fling_velocity) >= kDragVelocityThreshold) {
    // If the user releases drag with velocity over the threshold, snap to
    // the next state, ignoring the drag release position.

    if (last_fling_velocity > 0) {
      switch (app_list_state_) {
        case AppListViewState::kPeeking:
        case AppListViewState::kHalf:
        case AppListViewState::kFullscreenSearch:
        case AppListViewState::kFullscreenAllApps:
          Dismiss();
          break;
        case AppListViewState::kClosed:
          NOTREACHED();
          break;
      }
    } else {
      switch (app_list_state_) {
        case AppListViewState::kFullscreenAllApps:
        case AppListViewState::kFullscreenSearch:
          SetState(app_list_state_);
          break;
        case AppListViewState::kHalf:
          SetState(AppListViewState::kFullscreenSearch);
          break;
        case AppListViewState::kPeeking:
          UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                    kSwipe, kMaxPeekingToFullscreen);
          SetState(AppListViewState::kFullscreenAllApps);
          break;
        case AppListViewState::kClosed:
          NOTREACHED();
          break;
      }
    }
  } else {
    int app_list_height = GetHeightForState(app_list_state_);

    const int app_list_threshold =
        app_list_height / kAppListThresholdDenominator;
    const int drag_delta = initial_drag_point_.y() - location_in_root.y();
    int display_bottom_in_root = GetDisplayNearestView().bounds().height();
    // If the drag ended near the bezel, close the app list.
    if (location_in_root.y() >=
        (display_bottom_in_root - kAppListBezelMargin)) {
      Dismiss();
    } else {
      switch (app_list_state_) {
        case AppListViewState::kFullscreenAllApps:
          if (drag_delta < -app_list_threshold) {
            if (delegate_->IsInTabletMode() || is_side_shelf_)
              Dismiss();
            else
              SetState(AppListViewState::kPeeking);
          } else {
            SetState(app_list_state_);
          }
          break;
        case AppListViewState::kFullscreenSearch:
          if (drag_delta < -app_list_threshold)
            Dismiss();
          else
            SetState(app_list_state_);
          break;
        case AppListViewState::kHalf:
          if (drag_delta > app_list_threshold)
            SetState(AppListViewState::kFullscreenSearch);
          else if (drag_delta < -app_list_threshold)
            Dismiss();
          else
            SetState(app_list_state_);
          break;
        case AppListViewState::kPeeking:
          if (drag_delta > app_list_threshold) {
            SetState(AppListViewState::kFullscreenAllApps);
            UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                      kSwipe, kMaxPeekingToFullscreen);
          } else if (drag_delta < -app_list_threshold) {
            Dismiss();
          } else {
            SetState(app_list_state_);
          }
          break;
        case AppListViewState::kClosed:
          NOTREACHED();
          break;
      }
    }
  }
  initial_drag_point_ = gfx::PointF();
}

void AppListView::SetChildViewsForStateTransition(
    AppListViewState target_state) {
  if (target_state == AppListViewState::kHalf ||
      target_state == AppListViewState::kFullscreenSearch) {
    return;
  }

  if (GetAppsContainerView()->IsInFolderView())
    GetAppsContainerView()->ResetForShowApps();

  // Do not update the contents view state on closing.
  if (target_state != AppListViewState::kClosed) {
    app_list_main_view_->contents_view()->SetActiveState(
        AppListState::kStateApps, !is_side_shelf_);
  }

  // Set the apps to the initial page when PEEKING.
  if (target_state == AppListViewState::kPeeking)
    SelectInitialAppsPage();

  if (target_state == AppListViewState::kClosed && is_side_shelf_) {
    // Reset the search box to be shown again. This is done after the animation
    // is complete normally, but there is no animation when |is_side_shelf_|.
    search_box_view_->ClearSearchAndDeactivateSearchBox();
  }
}

void AppListView::ConvertAppListStateToFullscreenEquivalent(
    AppListViewState* state) {
  if (!(is_side_shelf_ || delegate_->IsInTabletMode()))
    return;

  // If side shelf or tablet mode are active, all transitions should be
  // made to the tablet mode/side shelf friendly versions.
  if (*state == AppListViewState::kHalf) {
    *state = AppListViewState::kFullscreenSearch;
  } else if (*state == AppListViewState::kPeeking) {
    // FULLSCREEN_ALL_APPS->PEEKING in tablet/side shelf mode should close
    // instead of going to PEEKING.
    *state = app_list_state_ == AppListViewState::kFullscreenAllApps
                 ? AppListViewState::kClosed
                 : AppListViewState::kFullscreenAllApps;
  }
}

void AppListView::MaybeIncreasePrivacyInfoRowShownCounts(
    AppListViewState new_state) {
  AppListStateTransitionSource transition =
      GetAppListStateTransitionSource(new_state);
  switch (transition) {
    case kPeekingToHalf:
    case kFullscreenAllAppsToFullscreenSearch:
      if (app_list_main_view()->contents_view()->IsShowingSearchResults())
        delegate_->MaybeIncreaseSuggestedContentInfoShownCount();
      break;
    default:
      break;
  }
}

void AppListView::RecordStateTransitionForUma(AppListViewState new_state) {
  AppListStateTransitionSource transition =
      GetAppListStateTransitionSource(new_state);
  // kMaxAppListStateTransition denotes a transition we are not interested in
  // recording (ie. PEEKING->PEEKING).
  if (transition == kMaxAppListStateTransition)
    return;

  UMA_HISTOGRAM_ENUMERATION("Apps.AppListStateTransitionSource", transition,
                            kMaxAppListStateTransition);

  switch (transition) {
    case kPeekingToFullscreenAllApps:
    case KHalfToFullscreenSearch:
      base::RecordAction(base::UserMetricsAction("AppList_PeekingToFull"));
      break;

    case kFullscreenAllAppsToPeeking:
      base::RecordAction(base::UserMetricsAction("AppList_FullToPeeking"));
      break;

    default:
      break;
  }
}

void AppListView::MaybeCreateAccessibilityEvent(AppListViewState new_state) {
  if (new_state == app_list_state_)
    return;

  if (!delegate_->AppListTargetVisibility())
    return;

  switch (new_state) {
    case AppListViewState::kPeeking:
      a11y_announcer_->AnnouncePeekingState();
      break;
    case AppListViewState::kFullscreenAllApps:
      a11y_announcer_->AnnounceFullscreenState();
      break;
    case AppListViewState::kClosed:
    case AppListViewState::kHalf:
    case AppListViewState::kFullscreenSearch:
      break;
  }
}

void AppListView::EnsureWidgetBoundsMatchCurrentState() {
  const gfx::Rect new_target_bounds =
      GetPreferredWidgetBoundsForState(target_app_list_state_);
  aura::Window* window = GetWidget()->GetNativeView();
  if (new_target_bounds == window->GetTargetBounds())
    return;

  // Set the widget size to fit the new display metrics.
  GetWidget()->GetNativeView()->SetBounds(new_target_bounds);
  ResetSubpixelPositionOffset(GetWidget()->GetNativeView()->layer());

  // Update the widget bounds to accommodate the new work
  // area.
  SetState(target_app_list_state_);
}

int AppListView::GetRemainingBoundsAnimationDistance() const {
  return GetWidget()->GetLayer()->transform().To2dTranslation().y();
}

display::Display AppListView::GetDisplayNearestView() const {
  return display::Screen::GetScreen()->GetDisplayNearestView(
      GetWidget()->GetNativeWindow()->parent());
}

AppsContainerView* AppListView::GetAppsContainerView() {
  return app_list_main_view_->contents_view()->apps_container_view();
}

PagedAppsGridView* AppListView::GetRootAppsGridView() {
  return GetAppsContainerView()->apps_grid_view();
}

AppsGridView* AppListView::GetFolderAppsGridView() {
  return GetAppsContainerView()->app_list_folder_view()->items_grid_view();
}

AppListStateTransitionSource AppListView::GetAppListStateTransitionSource(
    AppListViewState target_state) const {
  switch (app_list_state_) {
    case AppListViewState::kClosed:
      // CLOSED->X transitions are not useful for UMA.
      return kMaxAppListStateTransition;
    case AppListViewState::kPeeking:
      switch (target_state) {
        case AppListViewState::kClosed:
          return kPeekingToClosed;
        case AppListViewState::kHalf:
          return kPeekingToHalf;
        case AppListViewState::kFullscreenAllApps:
          return kPeekingToFullscreenAllApps;
        case AppListViewState::kPeeking:
          // PEEKING->PEEKING is used when resetting the widget position after a
          // failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
        case AppListViewState::kFullscreenSearch:
          // PEEKING->FULLSCREEN_SEARCH is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }
    case AppListViewState::kHalf:
      switch (target_state) {
        case AppListViewState::kClosed:
          return kHalfToClosed;
        case AppListViewState::kPeeking:
          return kHalfToPeeking;
        case AppListViewState::kFullscreenSearch:
          return KHalfToFullscreenSearch;
        case AppListViewState::kHalf:
          // HALF->HALF is used when resetting the widget position after a
          // failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
        case AppListViewState::kFullscreenAllApps:
          // HALF->FULLSCREEN_ALL_APPS is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }

    case AppListViewState::kFullscreenAllApps:
      switch (target_state) {
        case AppListViewState::kClosed:
          return kFullscreenAllAppsToClosed;
        case AppListViewState::kPeeking:
          return kFullscreenAllAppsToPeeking;
        case AppListViewState::kFullscreenSearch:
          return kFullscreenAllAppsToFullscreenSearch;
        case AppListViewState::kHalf:
          // FULLSCREEN_ALL_APPS->HALF is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
        case AppListViewState::kFullscreenAllApps:
          // FULLSCREEN_ALL_APPS->FULLSCREEN_ALL_APPS is used when resetting the
          // widget positon after a failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
      }
    case AppListViewState::kFullscreenSearch:
      switch (target_state) {
        case AppListViewState::kClosed:
          return kFullscreenSearchToClosed;
        case AppListViewState::kFullscreenAllApps:
          return kFullscreenSearchToFullscreenAllApps;
        case AppListViewState::kFullscreenSearch:
          // FULLSCREEN_SEARCH->FULLSCREEN_SEARCH is used when resetting the
          // widget position after a failed state transition. Not useful for
          // UMA.
          return kMaxAppListStateTransition;
        case AppListViewState::kPeeking:
          // FULLSCREEN_SEARCH->PEEKING is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
        case AppListViewState::kHalf:
          // FULLSCREEN_SEARCH->HALF is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }
  }
}

views::View* AppListView::GetInitiallyFocusedView() {
  views::View* initial_view;
  if (IsShowingEmbeddedAssistantUI()) {
    // Assistant page will redirect focus to its subviews.
    auto* content = app_list_main_view_->contents_view();
    initial_view = content->GetPageView(content->GetActivePageIndex());
  } else {
    initial_view = app_list_main_view_->search_box_view()->search_box();
  }
  return initial_view;
}

void AppListView::OnScrollEvent(ui::ScrollEvent* event) {
  if (!HandleScroll(event->location(),
                    gfx::Vector2d(event->x_offset(), event->y_offset()),
                    event->type())) {
    return;
  }

  event->SetHandled();
  event->StopPropagation();
}

void AppListView::OnMouseEvent(ui::MouseEvent* event) {
  // Ignore events if the app list is closing or closed.
  if (app_list_state_ == AppListViewState::kClosed)
    return;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      event->SetHandled();
      if (is_in_drag_)
        return;
      initial_mouse_drag_point_ = event->root_location_f();
      break;
    case ui::ET_MOUSE_DRAGGED:
      event->SetHandled();
      if (is_side_shelf_ || delegate_->IsInTabletMode())
        return;
      if (!is_in_drag_ && event->IsOnlyLeftMouseButton()) {
        // Calculate the mouse drag offset to determine whether AppListView is
        // in drag.
        gfx::Vector2dF drag_distance =
            event->root_location_f() - initial_mouse_drag_point_;
        if (std::abs(drag_distance.y()) < kMouseDragThreshold)
          return;

        StartDrag(initial_mouse_drag_point_);
        SetIsInDrag(true);
        app_list_main_view_->contents_view()->UpdateYPositionAndOpacity();
      }

      if (!is_in_drag_)
        return;
      UpdateDrag(event->root_location_f());
      break;
    case ui::ET_MOUSE_RELEASED:
      event->SetHandled();
      initial_mouse_drag_point_ = gfx::PointF();
      if (!is_in_drag_) {
        HandleClickOrTap(event);
        return;
      }
      EndDrag(event->root_location_f());
      CloseKeyboardIfVisible();
      SetIsInDrag(false);
      break;
    case ui::ET_MOUSEWHEEL:
      if (HandleScroll(event->location(), event->AsMouseWheelEvent()->offset(),
                       ui::ET_MOUSEWHEEL)) {
        event->SetHandled();
      }
      break;
    default:
      break;
  }
}

void AppListView::OnGestureEvent(ui::GestureEvent* event) {
  // Ignore events if the app list is closing or closed.
  if (app_list_state_ == AppListViewState::kClosed)
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      SetIsInDrag(false);
      event->SetHandled();
      HandleClickOrTap(event);
      break;
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      // If the search box is active when we start our drag, let it know.
      if (search_box_view_->is_search_box_active())
        search_box_view_->NotifyGestureEvent();

      // Avoid scrolling events for the app list in tablet mode.
      if (is_side_shelf_ || delegate_->IsInTabletMode())
        return;
      // There may be multiple scroll begin events in one drag because the
      // relative location of the finger and widget is almost unchanged and
      // scroll begin event occurs when the relative location changes beyond a
      // threshold. So avoid resetting the initial drag point in drag.
      if (!is_in_drag_)
        StartDrag(event->root_location_f());
      SetIsInDrag(true);
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      // Avoid scrolling events for the app list in tablet mode.
      if (is_side_shelf_ || delegate_->IsInTabletMode())
        return;
      SetIsInDrag(true);
      last_fling_velocity_ = event->details().scroll_y();
      UpdateDrag(event->root_location_f());
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_END: {
      if (!is_in_drag_)
        break;
      // Avoid scrolling events for the app list in tablet mode.
      if (is_side_shelf_ || delegate_->IsInTabletMode())
        return;
      EndDrag(event->root_location_f());
      event->SetHandled();
      break;
    }
    default:
      break;
  }
}

void AppListView::OnKeyEvent(ui::KeyEvent* event) {
  RedirectKeyEventToSearchBox(event);
}

void AppListView::OnTabletModeChanged(bool started) {
  search_box_view_->OnTabletModeChanged(started);
  app_list_main_view_->contents_view()->OnTabletModeChanged(started);

  if (is_in_drag_) {
    SetIsInDrag(false);
    UpdateChildViewsYPositionAndOpacity();
  }

  // Refresh the state if the view is not in a fullscreen state.
  if (started && !is_fullscreen())
    SetState(app_list_state_);

  app_list_background_shield_->UpdateBackground(
      /*use_blur*/ is_background_blur_enabled_ && !started);

  // Update background color opacity.
  SetBackgroundShieldColor();
}

void AppListView::OnWallpaperColorsChanged() {
  SetBackgroundShieldColor();
  search_box_view_->OnWallpaperColorsChanged();
}

bool AppListView::ShouldScrollDismissAppList(const gfx::Point& location,
                                             const gfx::Vector2d& offset,
                                             ui::EventType type,
                                             bool is_in_vertical_bounds) {
  if (delegate_->IsInTabletMode())
    return false;

  if (GetAppsContainerView()->IsInFolderView() && is_in_vertical_bounds)
    return false;

  if (!is_side_shelf() && is_in_vertical_bounds)
    return false;

  if (is_side_shelf()) {
    // This offset will be adjusted for scrolling preferences, as well as
    // for shelf alignment. Positive values are toward the shelf.
    int adjusted_offset =
        delegate_->AdjustAppListViewScrollOffset(offset.x(), type);

    // If the magnitude is big enough and the scroll is toward the shelf,
    // dismiss the full screen AppList.
    if (adjusted_offset > AppListView::kAppListMinScrollToSwitchStates &&
        app_list_state_ == AppListViewState::kFullscreenAllApps) {
      return true;
    }
  } else {
    int adjusted_offset =
        delegate_->AdjustAppListViewScrollOffset(offset.y(), type);

    // If the event is a mousewheel event, the offset is always large
    // enough, otherwise the offset must be larger than the scroll
    // threshold to dismiss from full screen.
    if ((type == ui::ET_MOUSEWHEEL ||
         std::abs(adjusted_offset) >
             AppListView::kAppListMinScrollToSwitchStates) &&
        app_list_state_ == AppListViewState::kFullscreenAllApps &&
        adjusted_offset < 0) {
      return true;
    }

    // For upward touchpad or mousewheel scrolling, expand to full screen.
    if (app_list_state_ == AppListViewState::kPeeking && adjusted_offset < 0)
      return true;
  }
  return false;
}

bool AppListView::HandleScroll(const gfx::Point& location,
                               const gfx::Vector2d& offset,
                               ui::EventType type) {
  // Ignore 0-offset events to prevent spurious dismissal, see crbug.com/806338
  // The system generates 0-offset ET_SCROLL_FLING_CANCEL events during simple
  // touchpad mouse moves. Those may be passed via mojo APIs and handled here.
  if ((offset.y() == 0 && offset.x() == 0) || ShouldIgnoreScrollEvents())
    return false;

  // Don't forward scroll information if a folder is open. The folder view will
  // handle scroll events itself.
  if (GetAppsContainerView()->IsInFolderView())
    return false;

  PagedAppsGridView* apps_grid_view = GetRootAppsGridView();

  gfx::Point root_apps_grid_location(location);
  views::View::ConvertPointToTarget(this, apps_grid_view,
                                    &root_apps_grid_location);

  // For the purposes of whether or not to dismiss the AppList, we treat any
  // scroll to the left or the right of the apps grid as though it was in the
  // apps grid, as long as it is within the vertical bounds of the apps grid.
  bool is_in_vertical_bounds =
      root_apps_grid_location.y() > apps_grid_view->GetLocalBounds().y() &&
      root_apps_grid_location.y() < apps_grid_view->GetLocalBounds().bottom();

  // First see if we need to collapse the app list from this scroll when in a
  // side shelf alignment. We do this first because if this happens anywhere on
  // the app list or shelf, we're going to dismiss and not scroll.
  if (ShouldScrollDismissAppList(location, offset, type,
                                 is_in_vertical_bounds)) {
    Dismiss();
    return true;
  }

  // For upward touchpad or mousewheel scrolling, expand to full screen.
  // For downward, dismiss the peeking launcher.
  if (app_list_state_ == AppListViewState::kPeeking &&
      delegate_->AdjustAppListViewScrollOffset(offset.y(), type) > 0) {
    SetState(AppListViewState::kFullscreenAllApps);
    const AppListPeekingToFullscreenSource source =
        type == ui::ET_MOUSEWHEEL ? kMousewheelScroll : kMousepadScroll;
    UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram, source,
                              kMaxPeekingToFullscreen);
    return true;
  }

  // In fullscreen, forward events to `apps_grid_view`. For example, this allows
  // scroll events to the right of the page switcher (not inside the apps grid)
  // to switch pages.
  if (app_list_state_ == AppListViewState::kFullscreenAllApps &&
      is_in_vertical_bounds) {
    apps_grid_view->HandleScrollFromParentView(offset, type);
  }
  return true;
}

void AppListView::SetState(AppListViewState new_state) {
  AppListViewState new_state_override = new_state;
  ConvertAppListStateToFullscreenEquivalent(&new_state_override);

  target_app_list_state_ = new_state_override;

  // Update the contents view state to match the app list view state.
  // Updating the contents view state may cause a nested `SetState()` call.
  // Bind the current state update to a weak ptr that gets invalidated when
  // `SetState()` gets called again to detect whether `SetState()` got called
  // again.
  set_state_weak_factory_.InvalidateWeakPtrs();
  base::WeakPtr<AppListView> set_state_request =
      set_state_weak_factory_.GetWeakPtr();

  // Clear the drag state before closing the view.
  if (new_state_override == AppListViewState::kClosed)
    SetIsInDrag(false);

  SetChildViewsForStateTransition(new_state_override);

  // Bail out if `SetChildViewForStateTransition()` caused another call to
  // `SetState()`.
  if (!set_state_request)
    return;

  // Bail out if `WorkAreaInsets::SetPersistentDeskBarHeight(int height)` causes
  // another call to `SetState()`. Note, the persistent desks bar is created in
  // the primary display for now.
  if (Shell::HasInstance() &&
      WorkAreaInsets::ForWindow(Shell::GetPrimaryRootWindow())
          ->PersistentDeskBarHeightInChange() &&
      app_list_state_ == new_state_override) {
    return;
  }

  MaybeCreateAccessibilityEvent(new_state_override);

  // Prepare state transition notifier for the new state transition.
  state_transition_notifier_->Reset(new_state_override);

  StartAnimationForState(new_state_override);
  MaybeIncreasePrivacyInfoRowShownCounts(new_state_override);
  RecordStateTransitionForUma(new_state_override);
  app_list_state_ = new_state_override;
  if (delegate_)
    delegate_->OnViewStateChanged(new_state_override);

  if (is_in_drag_ && app_list_state_ != AppListViewState::kClosed)
    app_list_main_view_->contents_view()->UpdateYPositionAndOpacity();

  if (GetWidget()->IsActive()) {
    // Reset the focus to initially focused view. This should be
    // done before updating visibility of views, because setting
    // focused view invisible automatically moves focus to next
    // focusable view, which potentially causes bugs.
    GetInitiallyFocusedView()->RequestFocus();
  }

  UpdateWindowTitle();

  // Activate state transition notifier after the app list state has been
  // updated, to ensure any observers that handle app list view state
  // transitions don't end up updating app list state while another state
  // transition is in progress (in case the transition animations complete
  // synchronously).
  state_transition_notifier_->Activate();

  // Updates the visibility of app list items according to the change of
  // |app_list_state_|.
  GetAppsContainerView()->UpdateControlVisibility(app_list_state_, is_in_drag_);
}

void AppListView::UpdateWindowTitle() {
  if (!GetWidget())
    return;
  gfx::NativeView window = GetWidget()->GetNativeView();
  AppListState contents_view_state = delegate_->GetCurrentAppListPage();
  if (window) {
    if (contents_view_state == AppListState::kStateSearchResults ||
        contents_view_state == AppListState::kStateEmbeddedAssistant) {
      window->SetTitle(l10n_util::GetStringUTF16(
          IDS_APP_LIST_LAUNCHER_ACCESSIBILITY_ANNOUNCEMENT));
      return;
    }
    switch (target_app_list_state_) {
      case AppListViewState::kPeeking:
        window->SetTitle(l10n_util::GetStringUTF16(
            IDS_APP_LIST_SUGGESTED_APPS_ACCESSIBILITY_ANNOUNCEMENT));
        break;
      case AppListViewState::kFullscreenAllApps:
        window->SetTitle(l10n_util::GetStringUTF16(
            IDS_APP_LIST_ALL_APPS_ACCESSIBILITY_ANNOUNCEMENT));
        break;
      case AppListViewState::kClosed:
      case AppListViewState::kHalf:
      case AppListViewState::kFullscreenSearch:
        break;
    }
  }
}

void AppListView::OnAppListVisibilityWillChange(bool visible) {
  GetAppsContainerView()->OnAppListVisibilityWillChange(visible);
}

void AppListView::OnAppListVisibilityChanged(bool shown) {
  GetAppsContainerView()->OnAppListVisibilityChanged(shown);
}

base::TimeDelta AppListView::GetStateTransitionAnimationDuration(
    AppListViewState target_state) {
  if (is_side_shelf_ || (target_state == AppListViewState::kClosed &&
                         delegate_->ShouldDismissImmediately())) {
    return base::Milliseconds(kAppListAnimationDurationImmediateMs);
  }

  if (is_fullscreen() || target_state == AppListViewState::kFullscreenAllApps ||
      target_state == AppListViewState::kFullscreenSearch) {
    // Animate over more time to or from a fullscreen state, to maintain a
    // similar speed.
    return base::Milliseconds(kAppListAnimationDurationFromFullscreenMs);
  }

  return base::Milliseconds(kAppListAnimationDurationMs);
}

void AppListView::StartAnimationForState(AppListViewState target_state) {
  base::TimeDelta animation_duration =
      GetStateTransitionAnimationDuration(target_state);

  ApplyBoundsAnimation(target_state, animation_duration);
  app_list_main_view_->contents_view()->OnAppListViewTargetStateChanged(
      target_state);
  if (!is_in_drag_) {
    app_list_main_view_->contents_view()->AnimateToViewState(
        target_state, animation_duration);
  }
}

void AppListView::ApplyBoundsAnimation(AppListViewState target_state,
                                       base::TimeDelta duration_ms) {
  if (is_side_shelf_ || is_in_drag_) {
    // There is no animation in side shelf.
    UpdateAppListBackgroundYPosition(target_state);
    // Mark the state transition as complete directly, as no animations that
    // for `state_transition_notifier_` to observe are run in this case.
    state_transition_notifier_->SetTransitionDone();
    return;
  }

  gfx::Rect target_bounds = GetPreferredWidgetBoundsForState(target_state);

  // When closing the view should animate to the shelf bounds. The workspace
  // area will not reflect an autohidden shelf so ask for the proper bounds.
  const int y_for_closed_state = delegate_->GetTargetYForAppListHide(
      GetWidget()->GetNativeView()->GetRootWindow());
  if (target_state == AppListViewState::kClosed) {
    target_bounds.set_y(y_for_closed_state);
  }

  // Record the current transform before removing it because this bounds
  // animation could be pre-empting another bounds animation.
  ui::Layer* layer = GetWidget()->GetLayer();

  // Adjust the closed state y to account for auto-hidden shelf.
  const int current_bounds_y = app_list_state_ == AppListViewState::kClosed
                                   ? y_for_closed_state
                                   : layer->bounds().y();
  const int current_y_with_transform =
      current_bounds_y + GetRemainingBoundsAnimationDistance();

  const gfx::Transform current_shield_transform =
      app_list_background_shield_->layer()->transform();

  // Only report animation throughput for full state transitions - i.e. when the
  // starting app list view position matches the expected position for the
  // current app list state. The goal is to reduce noise introduced by partial
  // state transitions - for example
  // *   When interrupting another state transition half-way, in which case the
  //     layer has non-identity ransform.
  // *   Starting an animation after drag gesture, in which case bounds may not
  //     match the expected app list bounds in the current state.
  bool report_animation_throughput =
      layer->transform() == gfx::Transform() &&
      layer->bounds() == GetPreferredWidgetBoundsForState(app_list_state_);

  // Schedule the animation; set to the target bounds, and make the transform
  // to make this appear in the original location. Then set an empty transform
  // with the animation.
  layer->SetBounds(target_bounds);
  ResetSubpixelPositionOffset(layer);

  gfx::Transform transform;
  const int y_offset = current_y_with_transform - target_bounds.y();
  transform.Translate(0, y_offset);
  layer->SetTransform(transform);
  animation_end_timestamp_ = base::TimeTicks::Now() + duration_ms;

  // Reset animation metrics reporter when animation is started.
  ResetTransitionMetricsReporter();

  if (delegate_->IsInTabletMode() &&
      target_state != AppListViewState::kClosed) {
    DCHECK(target_state == AppListViewState::kFullscreenAllApps ||
           target_state == AppListViewState::kFullscreenSearch);
    TabletModeAnimationTransition transition_type =
        target_state == AppListViewState::kFullscreenAllApps
            ? TabletModeAnimationTransition::kEnterFullscreenAllApps
            : TabletModeAnimationTransition::kEnterFullscreenSearch;
    state_animation_metrics_reporter_->SetTabletModeAnimationTransition(
        transition_type);
  } else {
    state_animation_metrics_reporter_->SetTargetState(target_state);
  }

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  absl::optional<ui::AnimationThroughputReporter> reporter;
  if (report_animation_throughput) {
    reporter.emplace(
        animation.GetAnimator(),
        metrics_util::ForSmoothness(GetStateTransitionMetricsReportCallback()));
  }
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ui", "AppList::StateTransitionAnimations",
                                    state_transition_notifier_.get());
  animation.AddObserver(state_transition_notifier_.get());

  // In fullscreen state, or peeking state with restricted vertical space, the
  // background shield is translated upwards to ensure background radius is not
  // visible.
  // NOTE: layer->SetBounds() changes shield transform, so reset the transform
  // to the value before the |layer| bounds are set before starting the
  // animation.
  app_list_background_shield_->SetTransform(current_shield_transform);
  ui::ScopedLayerAnimationSettings shield_animation(
      app_list_background_shield_->layer()->GetAnimator());
  shield_animation.SetTransitionDuration(duration_ms);
  shield_animation.SetTweenType(gfx::Tween::EASE_OUT);
  shield_animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);

  gfx::Transform shield_transform;
  if (ShouldHideRoundedCorners(target_state, target_bounds)) {
    shield_transform.Translate(0, -(delegate_->GetShelfSize() / 2));
  }
  app_list_background_shield_->SetTransform(shield_transform);

  animation.SetTransitionDuration(duration_ms);
  animation.SetTweenType(gfx::Tween::EASE_OUT);
  layer->SetTransform(gfx::Transform());

  // Schedule animations of the rounded corners. When running on linux
  // workstation, the rounded corner animation sometimes looks out-of-sync. This
  // does not happen on actual devices.

  // TODO(mukai): fix the out-of-sync problem.
  app_list_background_shield_->UpdateBackgroundRadius(
      target_state, shelf_has_rounded_corners_, animation_end_timestamp_);
}

void AppListView::SetStateFromSearchBoxView(bool search_box_is_empty,
                                            bool triggered_by_contents_change) {
  switch (target_app_list_state_) {
    case AppListViewState::kPeeking:
      if (!search_box_is_empty || search_box_view()->is_search_box_active())
        SetState(AppListViewState::kHalf);
      break;
    case AppListViewState::kHalf:
      if (search_box_is_empty && !triggered_by_contents_change)
        SetState(AppListViewState::kPeeking);
      break;
    case AppListViewState::kFullscreenSearch:
      if (search_box_is_empty && !triggered_by_contents_change)
        SetState(AppListViewState::kFullscreenAllApps);
      break;
    case AppListViewState::kFullscreenAllApps:
      if (!search_box_is_empty ||
          (search_box_is_empty && triggered_by_contents_change)) {
        SetState(AppListViewState::kFullscreenSearch);
      }
      break;
    case AppListViewState::kClosed:
      // We clean search on app list close.
      break;
  }
}

void AppListView::UpdateYPositionAndOpacity(float y_position_in_root,
                                            float background_opacity) {
  DCHECK(!is_side_shelf_);
  if (app_list_state_ == AppListViewState::kClosed)
    return;

  if (GetWidget()->GetLayer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::TRANSFORM)) {
    GetWidget()->GetLayer()->GetAnimator()->StopAnimatingProperty(
        ui::LayerAnimationElement::TRANSFORM);
  }

  SetIsInDrag(true);

  presentation_time_recorder_->RequestNext();

  background_opacity_in_drag_ = background_opacity;
  gfx::Rect new_window_bounds = GetWidget()->GetNativeWindow()->bounds();
  display::Display display = GetDisplayNearestView();
  float app_list_y_position_in_root = std::min(
      std::max(y_position_in_root,
               static_cast<float>(display.GetWorkAreaInsets().top())),
      static_cast<float>(display.size().height() - delegate_->GetShelfSize()));

  gfx::NativeView native_view = GetWidget()->GetNativeView();
  new_window_bounds.set_y(static_cast<int>(app_list_y_position_in_root));
  native_view->SetBounds(new_window_bounds);
  native_view->layer()->SetSubpixelPositionOffset(gfx::Vector2dF(
      ComputeSubpixelOffset(display, new_window_bounds.x()),
      ComputeSubpixelOffset(display, app_list_y_position_in_root)));

  UpdateChildViewsYPositionAndOpacity();
}

void AppListView::OffsetYPositionOfAppList(int offset) {
  gfx::NativeView native_view = GetWidget()->GetNativeView();
  gfx::Transform transform;
  transform.Translate(0, offset);
  native_view->SetTransform(transform);
}

PaginationModel* AppListView::GetAppsPaginationModel() {
  return GetRootAppsGridView()->pagination_model();
}

gfx::Rect AppListView::GetAppInfoDialogBounds() const {
  gfx::Rect app_info_bounds(GetDisplayNearestView().work_area());
  app_info_bounds.ClampToCenteredSize(
      gfx::Size(kAppInfoDialogWidth, kAppInfoDialogHeight));
  return app_info_bounds;
}

void AppListView::SetIsInDrag(bool is_in_drag) {
  if (!is_in_drag && !delegate_->IsInTabletMode())
    presentation_time_recorder_.reset();

  if (is_in_drag == is_in_drag_)
    return;

  // Reset |last_fling_velocity_| if it was set during the drag.
  if (!is_in_drag)
    last_fling_velocity_ = 0;

  // Don't allow dragging to interrupt the close animation, it probably is not
  // intentional.
  if (app_list_state_ == AppListViewState::kClosed)
    return;

  is_in_drag_ = is_in_drag;

  if (is_in_drag && !delegate_->IsInTabletMode()) {
    presentation_time_recorder_.reset();
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        GetWidget()->GetCompositor(), kAppListDragInClamshellHistogram,
        kAppListDragInClamshellMaxLatencyHistogram);
  }

  GetAppsContainerView()->UpdateControlVisibility(target_app_list_state_,
                                                  is_in_drag_);
}

void AppListView::OnHomeLauncherGainingFocusWithoutAnimation() {
  if (GetFocusManager()->GetFocusedView() != GetInitiallyFocusedView())
    GetInitiallyFocusedView()->RequestFocus();
}

void AppListView::SelectInitialAppsPage() {
  if (GetAppsPaginationModel()->total_pages() > 0 &&
      GetAppsPaginationModel()->selected_page() != 0) {
    GetAppsPaginationModel()->SelectPage(0, false /* animate */);
  }
}
int AppListView::GetScreenBottom() const {
  return GetDisplayNearestView().bounds().bottom();
}

int AppListView::GetCurrentAppListHeight() const {
  if (!GetWidget())
    return delegate_->GetShelfSize();
  return GetScreenBottom() - GetWidget()->GetWindowBoundsInScreen().y();
}

float AppListView::GetAppListTransitionProgress(int flags) const {
  // During transition between home and overview in tablet mode, the app list
  // widget gets scaled down from full screen state - if this is the case,
  // the app list layout should match the current app list state, so return
  // the progress for the current app list state.
  const gfx::Transform transform = GetWidget()->GetLayer()->transform();
  if (delegate_->IsInTabletMode() && transform.IsScaleOrTranslation() &&
      !transform.IsIdentityOrTranslation()) {
    return GetTransitionProgressForState(app_list_state_);
  }

  int current_height = GetCurrentAppListHeight();
  if (flags & kProgressFlagWithTransform) {
    current_height -=
        GetWidget()->GetLayer()->transform().To2dTranslation().y();
  }

  const int fullscreen_height = GetFullscreenStateHeight();
  const int baseline_height = std::min(
      fullscreen_height,
      (flags & kProgressFlagSearchResults) ? kHalfHeight : kPeekingHeight);

  // If vertical space is limited, the baseline and fullscreen height might be
  // the same. To handle this case, if the height has reached the
  // baseline/fullscreen height, return either 1.0 or 2.0 progress, depending on
  // the current target state.
  if (baseline_height == fullscreen_height &&
      current_height >= fullscreen_height) {
    return GetTransitionProgressForState(app_list_state_);
  }

  if (current_height <= baseline_height) {
    // Currently transition progress is between closed and peeking state.
    // Calculate the progress of this transition.
    const float shelf_height =
        GetScreenBottom() - GetDisplayNearestView().work_area().bottom();

    // When screen is rotated, the current height might be smaller than shelf
    // height for just one moment, which results in negative progress. So force
    // the progress to be non-negative.
    return std::max(0.0f, (current_height - shelf_height) /
                              (baseline_height - shelf_height));
  }

  // Currently transition progress is between peeking and fullscreen state.
  // Calculate the progress of this transition.
  const float fullscreen_height_above_baseline =
      fullscreen_height - baseline_height;
  const float current_height_above_baseline = current_height - baseline_height;
  DCHECK_GT(fullscreen_height_above_baseline, 0);
  DCHECK_LE(current_height_above_baseline, fullscreen_height_above_baseline);
  return 1 + current_height_above_baseline / fullscreen_height_above_baseline;
}

int AppListView::GetHeightForState(AppListViewState state) const {
  switch (app_list_state_) {
    case AppListViewState::kFullscreenAllApps:
    case AppListViewState::kFullscreenSearch:
      return GetFullscreenStateHeight();
    case AppListViewState::kHalf:
      return std::min(GetFullscreenStateHeight(), kHalfHeight);
    case AppListViewState::kPeeking:
      return kPeekingHeight;
    case AppListViewState::kClosed:
      return 0;
  }
}

int AppListView::GetFullscreenStateHeight() const {
  const display::Display display = GetDisplayNearestView();
  const gfx::Rect display_bounds = display.bounds();
  return display_bounds.height() - display.work_area().y() + display_bounds.y();
}

AppListViewState AppListView::CalculateStateAfterShelfDrag(
    const ui::LocatedEvent& event_in_screen,
    float launcher_above_shelf_bottom_amount) const {
  AppListViewState app_list_state = AppListViewState::kPeeking;
  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START &&
      fabs(event_in_screen.AsGestureEvent()->details().velocity_y()) >
          kDragVelocityFromShelfThreshold) {
    // If the scroll sequence terminates with a fling, show the fullscreen app
    // list if the fling was fast enough and in the correct direction, otherwise
    // close it.
    app_list_state =
        event_in_screen.AsGestureEvent()->details().velocity_y() < 0
            ? AppListViewState::kFullscreenAllApps
            : AppListViewState::kClosed;
  } else {
    // Snap the app list to corresponding state according to the snapping
    // thresholds.
    if (delegate_->IsInTabletMode()) {
      app_list_state =
          launcher_above_shelf_bottom_amount > kDragSnapToFullscreenThreshold
              ? AppListViewState::kFullscreenAllApps
              : AppListViewState::kClosed;
    } else {
      if (launcher_above_shelf_bottom_amount <= kDragSnapToClosedThreshold) {
        app_list_state = AppListViewState::kClosed;
      } else if (launcher_above_shelf_bottom_amount <=
                 kDragSnapToPeekingThreshold) {
        app_list_state = AppListViewState::kPeeking;
      } else {
        app_list_state = AppListViewState::kFullscreenAllApps;
      }
    }
  }

  // Deal with the situation of dragging app list from shelf while typing in
  // the search box.
  if (app_list_state == AppListViewState::kFullscreenAllApps) {
    AppListState active_state =
        app_list_main_view_->contents_view()->GetActiveState();
    if (active_state == AppListState::kStateSearchResults)
      app_list_state = AppListViewState::kFullscreenSearch;
  }

  return app_list_state;
}

metrics_util::SmoothnessCallback
AppListView::GetStateTransitionMetricsReportCallback() {
  return state_animation_metrics_reporter_->GetReportCallback(
      delegate_->IsInTabletMode());
}

void AppListView::ResetTransitionMetricsReporter() {
  state_animation_metrics_reporter_->Reset();
}

void AppListView::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(GetWidget()->GetNativeView(), window);
  window->RemoveObserver(this);
}

void AppListView::OnWindowBoundsChanged(aura::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds,
                                        ui::PropertyChangeReason reason) {
  DCHECK_EQ(GetWidget()->GetNativeView(), window);

  gfx::Transform transform;
  if (ShouldHideRoundedCorners(target_app_list_state_, new_bounds))
    transform.Translate(0, -(delegate_->GetShelfSize() / 2));

  // Avoid setting new transform if the shield is animating to (or already has)
  // the target value.
  if (app_list_background_shield_->layer()->GetTargetTransform() != transform) {
    app_list_background_shield_->SetTransform(transform);
    app_list_background_shield_->SchedulePaint();
  }
}

void AppListView::OnBoundsAnimationCompleted(AppListViewState target_state) {
  const bool was_animation_interrupted =
      GetRemainingBoundsAnimationDistance() != 0;

  if (target_state == AppListViewState::kClosed) {
    // Close embedded Assistant UI if it is open, to reset the
    // |assistant_page_view| bounds and AppListState.
    auto* contents_view = app_list_main_view()->contents_view();
    if (contents_view->IsShowingEmbeddedAssistantUI())
      contents_view->ShowEmbeddedAssistantUI(false);
  }

  ui::ImplicitAnimationObserver* animation_observer =
      delegate_->GetAnimationObserver(target_state);
  if (animation_observer)
    animation_observer->OnImplicitAnimationsCompleted();

  // Layout if the animation was completed.
  if (!was_animation_interrupted)
    Layout();

  // NOTE: `target_state` may not match `app_list_state_` if
  // `OnBoundsAnimationCompleted()` gets called synchronously - for example,
  // for state changes during drag, and with side shelf.
  delegate_->OnStateTransitionAnimationCompleted(target_state,
                                                 was_animation_interrupted);
}

void AppListView::SetShelfHasRoundedCorners(bool shelf_has_rounded_corners) {
  if (shelf_has_rounded_corners_ == shelf_has_rounded_corners)
    return;
  shelf_has_rounded_corners_ = shelf_has_rounded_corners;
  absl::optional<base::TimeTicks> animation_end_timestamp;
  if (GetWidget() && GetWidget()->GetLayer()->GetAnimator()->is_animating()) {
    animation_end_timestamp = animation_end_timestamp_;
  }
  app_list_background_shield_->UpdateBackgroundRadius(
      target_app_list_state_, shelf_has_rounded_corners_,
      animation_end_timestamp);
}

void AppListView::UpdateChildViewsYPositionAndOpacity() {
  if (target_app_list_state_ == AppListViewState::kClosed)
    return;

  UpdateAppListBackgroundYPosition(target_app_list_state_);

  // Update the opacity of the background shield.
  SetBackgroundShieldColor();

  app_list_main_view_->contents_view()->UpdateYPositionAndOpacity();
}

void AppListView::RedirectKeyEventToSearchBox(ui::KeyEvent* event) {
  if (event->handled())
    return;

  // Allow text input inside the Assistant page.
  if (IsShowingEmbeddedAssistantUI())
    return;

  views::Textfield* search_box = search_box_view_->search_box();
  const bool is_search_box_focused = search_box->HasFocus();

  // Do not redirect the key event to the |search_box_| when focus is on a
  // text field.
  if (is_search_box_focused || IsFolderBeingRenamed())
    return;

  // Do not redirect the arrow keys in app list as they are are used for focus
  // traversal and app movement.
  if (IsArrowKeyEvent(*event) && !search_box_view_->is_search_box_active())
    return;

  // Redirect key event to |search_box_|.
  search_box->OnKeyEvent(event);
  if (event->handled()) {
    // Set search box focused if the key event is consumed.
    search_box->RequestFocus();
    return;
  }

  // Insert it into search box if the key event is a character. Released
  // key should not be handled to prevent inserting duplicate character.
  if (event->type() == ui::ET_KEY_PRESSED)
    search_box->InsertChar(*event);
}

void AppListView::OnScreenKeyboardShown(bool shown) {
  if (onscreen_keyboard_shown_ == shown)
    return;

  onscreen_keyboard_shown_ = shown;
  if (shown && GetAppsContainerView()->IsInFolderView()) {
    // Move the app list up to prevent folders being blocked by the
    // on-screen keyboard.
    const int folder_offset =
        GetAppsContainerView()->app_list_folder_view()->GetYOffsetForFolder();
    if (folder_offset != 0) {
      OffsetYPositionOfAppList(folder_offset);
      offset_to_show_folder_with_onscreen_keyboard_ = true;
    }
  } else if (offset_to_show_folder_with_onscreen_keyboard_) {
    // If the keyboard is closing or a folder isn't being shown, reset
    // the app list's position
    OffsetYPositionOfAppList(0);
    offset_to_show_folder_with_onscreen_keyboard_ = false;
  }

  if (!shown) {
    // When the virtual keyboard is hidden, it will attempt to restore the app
    // list bounds from when the keyboard was first shown - this might misplace
    // the app list view if its intended bounds changed in the mean time. To
    // avoid that, clear saved "restore bounds", and call SetState() to make
    // sure app list bounds match the current app list view state.
    GetWidget()->GetNativeView()->ClearProperty(
        wm::kVirtualKeyboardRestoreBoundsKey);
    EnsureWidgetBoundsMatchCurrentState();
  }
}

bool AppListView::CloseKeyboardIfVisible() {
  // TODO(ginko) abstract this function to be in
  // |keyboard::KeyboardUIController*|
  if (!keyboard::KeyboardUIController::HasInstance())
    return false;
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    keyboard_controller->HideKeyboardByUser();
    return true;
  }
  return false;
}

void AppListView::OnParentWindowBoundsChanged() {
  EnsureWidgetBoundsMatchCurrentState();
}

float AppListView::GetAppListBackgroundOpacityDuringDragging() {
  float top_of_applist = GetWidget()->GetWindowBoundsInScreen().y();
  const int shelf_height = delegate_->GetShelfSize();
  float dragging_height =
      std::max((GetScreenBottom() - shelf_height - top_of_applist), 0.f);
  float coefficient =
      std::min(dragging_height / (kNumOfShelfSize * shelf_height), 1.0f);
  float shield_opacity =
      is_background_blur_enabled_ ? kAppListOpacityWithBlur : kAppListOpacity;
  // Assume shelf is opaque when start to drag down the launcher.
  const float shelf_opacity = 1.0f;
  return coefficient * shield_opacity + (1 - coefficient) * shelf_opacity;
}

void AppListView::SetBackgroundShieldColor() {
  // There is a chance when AppListView::OnWallpaperColorsChanged is called
  // from AppListViewDelegate, the |app_list_background_shield_| is not
  // initialized.
  if (!app_list_background_shield_)
    return;

  // Opacity is set on the color instead of the layer because changing opacity
  // of the layer changes opacity of the blur effect, which is not desired.
  float color_opacity = kAppListOpacity;

  if (delegate_->IsInTabletMode()) {
    // The Homecher background should have an opacity of 0.
    color_opacity = 0;
  } else if (is_in_drag_) {
    // Allow a custom opacity while the AppListView is dragging to show a
    // gradual opacity change when dragging from the shelf.
    color_opacity = background_opacity_in_drag_;
  } else if (is_background_blur_enabled_) {
    color_opacity = kAppListOpacityWithBlur;
  }

  app_list_background_shield_->UpdateColor(
      GetBackgroundShieldColor(delegate_->GetWallpaperProminentColors(),
                               color_opacity, delegate_->IsInTabletMode()));
}

bool AppListView::ShouldIgnoreScrollEvents() {
  // When the app list is doing state change animation or the apps grid view is
  // in transition, ignore the scroll events to prevent triggering extra state
  // changes or transitions.
  if (is_in_drag())
    return true;
  if (app_list_state_ != AppListViewState::kPeeking &&
      app_list_state_ != AppListViewState::kFullscreenAllApps)
    return true;
  return GetWidget()->GetLayer()->GetAnimator()->is_animating() ||
         GetRootAppsGridView()->pagination_model()->has_transition();
}

int AppListView::GetPreferredWidgetYForState(AppListViewState state) const {
  // Note that app list container fills the screen, so we can treat the
  // container's y as the top of display.
  const display::Display display = GetDisplayNearestView();
  const gfx::Rect work_area_bounds = display.work_area();

  // The ChromeVox panel as well as the Docked Magnifier viewport affect the
  // workarea of the display. We need to account for that when applist is in
  // fullscreen to avoid being shown below them.
  const int fullscreen_height = work_area_bounds.y() - display.bounds().y();

  // Force fullscreen height if onscreen keyboard is shown to match the UI state
  // that's set by default when the onscreen keyboard is first shown.
  if (onscreen_keyboard_shown_ && state != AppListViewState::kClosed)
    return fullscreen_height;

  switch (state) {
    case AppListViewState::kPeeking:
      return display.bounds().height() - kPeekingHeight;
    case AppListViewState::kHalf:
      return std::max(work_area_bounds.y() - display.bounds().y(),
                      display.bounds().height() - kHalfHeight);
    case AppListViewState::kFullscreenAllApps:
    case AppListViewState::kFullscreenSearch:
      return fullscreen_height;
    case AppListViewState::kClosed:
      // Align the widget y with shelf y to avoid flicker in show animation. In
      // side shelf mode, the widget y is the top of work area because the
      // widget does not animate.
      return (is_side_shelf_ ? work_area_bounds.y()
                             : work_area_bounds.bottom()) -
             display.bounds().y();
  }
}

gfx::Rect AppListView::GetPreferredWidgetBoundsForState(
    AppListViewState state) {
  // Use parent's width instead of display width to avoid 1 px gap (See
  // https://crbug.com/884889).
  CHECK(GetWidget());
  aura::Window* parent = GetWidget()->GetNativeView()->parent();
  CHECK(parent);
  return delegate_->SnapBoundsToDisplayEdge(
      gfx::Rect(0, GetPreferredWidgetYForState(state), parent->bounds().width(),
                GetFullscreenStateHeight()));
}

void AppListView::UpdateAppListBackgroundYPosition(AppListViewState state) {
  const int app_list_background_corner_radius = delegate_->GetShelfSize() / 2;

  // Update the y position of the background shield.
  gfx::Transform transform;
  if (is_in_drag_) {
    // For the purpose of determining background shield offset, use progress
    // with kHalf baseline so the background shield does not start translating
    // up before it reaches kHalf height (which is larger than kPeeking height).
    // If the shield transform started at kPeeking height, the app list view
    // background would jump up when starting drag from the kHalf state.
    float app_list_transition_progress =
        GetAppListTransitionProgress(kProgressFlagSearchResults);
    if (app_list_transition_progress < 1 && !shelf_has_rounded_corners()) {
      const float shelf_height =
          GetScreenBottom() - GetDisplayNearestView().work_area().bottom();
      app_list_background_shield_->SetBackgroundRadius(
          GetBackgroundRadiusForAppListHeight(
              GetCurrentAppListHeight() - shelf_height,
              app_list_background_corner_radius));
    } else if (app_list_transition_progress >= 1 &&
               app_list_transition_progress <= 2) {
      // Translate background shield so that it ends drag at a y position
      // according to the background radius in peeking and fullscreen.
      transform.Translate(0, -app_list_background_corner_radius *
                                 (app_list_transition_progress - 1));
    }
  } else if (ShouldHideRoundedCorners(state, GetBoundsInScreen())) {
    transform.Translate(0, -app_list_background_corner_radius);
  }

  // Avoid setting new transform if the shield is animating to (or already has)
  // the target value.
  if (app_list_background_shield_->layer()->GetTargetTransform() != transform)
    app_list_background_shield_->SetTransform(transform);
}

void AppListView::OnTabletModeAnimationTransitionNotified(
    TabletModeAnimationTransition animation_transition) {
  state_animation_metrics_reporter_->SetTabletModeAnimationTransition(
      animation_transition);
}

void AppListView::EndDragFromShelf(AppListViewState app_list_state) {
  SetIsInDrag(false);

  if (app_list_state == AppListViewState::kClosed ||
      target_app_list_state_ == AppListViewState::kClosed) {
    Dismiss();
  } else {
    SetState(app_list_state);
  }
  UpdateChildViewsYPositionAndOpacity();
}

void AppListView::ResetSubpixelPositionOffset(ui::Layer* layer) {
  const display::Display display = GetDisplayNearestView();
  const gfx::Rect& bounds = layer->bounds();
  layer->SetSubpixelPositionOffset(
      gfx::Vector2dF(ComputeSubpixelOffset(display, bounds.x()),
                     ComputeSubpixelOffset(display, bounds.y())));
}

}  // namespace ash
