// Copyright 2012 The Chromium Authors
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
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/trace_event/trace_event.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/ime_util_chromeos.h"

namespace ash {

namespace {

// The size of app info dialog in fullscreen app list.
constexpr int kAppInfoDialogWidth = 512;
constexpr int kAppInfoDialogHeight = 384;

// The duration of app list animations when they should run immediately.
constexpr int kAppListAnimationDurationImmediateMs = 0;

// The number of minutes that must pass for the current app list page to reset
// to the first page.
constexpr int kAppListPageResetTimeLimitMinutes = 20;

// When true, immdeidately fires the page reset timer upon starting.
bool skip_page_reset_timer_for_testing = false;

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

  // Sets tablet animation transition type for metrics.
  void SetTabletModeAnimationTransition(
      TabletModeAnimationTransition transition) {
    tablet_transition_ = transition;
  }

  // Resets the target state and animation type for metrics.
  void Reset();

  // Gets a callback to report smoothness.
  metrics_util::SmoothnessCallback GetReportCallback() {
    return base::BindRepeating(&StateAnimationMetricsReporter::RecordMetrics,
                               std::move(tablet_transition_));
  }

 private:
  static void RecordMetrics(
      absl::optional<TabletModeAnimationTransition> transition,
      int value);

  absl::optional<TabletModeAnimationTransition> tablet_transition_;
};

void AppListView::StateAnimationMetricsReporter::Reset() {
  tablet_transition_.reset();
}

// static
void AppListView::StateAnimationMetricsReporter::RecordMetrics(
    absl::optional<TabletModeAnimationTransition> tablet_transition,
    int value) {
  UMA_HISTOGRAM_PERCENTAGE("Apps.StateTransition.AnimationSmoothness", value);

  // It can't ensure the target transition is properly set. Simply give up
  // reporting per-state metrics in that case. See https://crbug.com/954907.
  if (!tablet_transition)
    return;
  switch (*tablet_transition) {
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
  DCHECK(!app_list_main_view_);
  DCHECK(!search_box_view_);

  a11y_announcer_ = std::make_unique<AppListA11yAnnouncer>(
      AddChildView(std::make_unique<views::View>()));

  auto app_list_main_view = std::make_unique<AppListMainView>(delegate_, this);
  search_box_view_ = new SearchBoxView(app_list_main_view.get(), delegate_,
                                       /*is_app_list_bubble=*/false);
  search_box_view_->InitializeForFullscreenLauncher();

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

  // Directs A11y focus ring from AppListView's descendants to search box view
  // without focusing on the whole app list window when using search + arrow
  // button.
  GetViewAccessibility().OverrideNextFocus(search_box_widget);
  GetViewAccessibility().OverridePreviousFocus(search_box_widget);
}

void AppListView::Show(AppListViewState preferred_state) {
  if (!time_shown_.has_value())
    time_shown_ = base::Time::Now();

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));

  UpdateWidget();

  if (!disable_contents_reset_when_showing_)
    app_list_main_view_->contents_view()->ResetForShow();

  SetState(preferred_state);
  DCHECK(is_fullscreen());

  // Ensures that the launcher won't open underneath the a11y keyboard.
  CloseKeyboardIfVisible();

  app_list_main_view_->ShowAppListWhenReady();

  UMA_HISTOGRAM_TIMES("Apps.AppListCreationTime",
                      base::Time::Now() - time_shown_.value());
  time_shown_ = absl::nullopt;
}

void AppListView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  app_list_main_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
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
      Back();
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
  if (app_list_visibility) {
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
    // Home launcher is shown on top of wallpaper with transparent background.
    // So trigger the wallpaper context menu for the same events.
    gfx::Point onscreen_location(event->location());
    ConvertPointToScreen(this, &onscreen_location);
    delegate_->ShowWallpaperContextMenu(
        onscreen_location, event->IsGestureEvent() ? ui::MENU_SOURCE_TOUCH
                                                   : ui::MENU_SOURCE_MOUSE);
    return;
  }

  if (search_box_view_->is_search_box_active())
    search_box_view_->ClearSearchAndDeactivateSearchBox();
}

void AppListView::SetChildViewsForStateTransition(
    AppListViewState target_state) {
  if (target_state == AppListViewState::kFullscreenSearch) {
    return;
  }

  if (GetAppsContainerView()->IsInFolderView())
    GetAppsContainerView()->ResetForShowApps();

  // Do not update the contents view state on closing.
  if (target_state != AppListViewState::kClosed) {
    app_list_main_view_->contents_view()->SetActiveState(
        AppListState::kStateApps, /*animate=*/true);
  }
}

void AppListView::RecordStateTransitionForUma(AppListViewState new_state) {
  AppListStateTransitionSource transition =
      GetAppListStateTransitionSource(new_state);
  // kMaxAppListStateTransition denotes a transition we are not interested in
  // recording (ie. FullscreenAllApps->FullscreenAllApps).
  if (transition == kMaxAppListStateTransition)
    return;

  UMA_HISTOGRAM_ENUMERATION("Apps.AppListStateTransitionSource", transition,
                            kMaxAppListStateTransition);
}

void AppListView::MaybeCreateAccessibilityEvent(AppListViewState new_state) {
  if (new_state == app_list_state_ || !delegate_->AppListTargetVisibility())
    return;

  if (new_state == AppListViewState::kFullscreenAllApps)
    a11y_announcer_->AnnounceAppListShown();
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
  // TODO(https://crbug.com/1356661): Remove peeking and half launcher
  // transitions.
  switch (app_list_state_) {
    case AppListViewState::kClosed:
      // CLOSED->X transitions are not useful for UMA.
      return kMaxAppListStateTransition;
    case AppListViewState::kFullscreenAllApps:
      switch (target_state) {
        case AppListViewState::kClosed:
          return kFullscreenAllAppsToClosed;
        case AppListViewState::kFullscreenSearch:
          return kFullscreenAllAppsToFullscreenSearch;
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
    // TODO(https://crbug.com/1356661): Consider not marking ET_MOUSE_DRAGGED as
    // handled here.
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
      event->SetHandled();
      break;
    case ui::ET_MOUSE_RELEASED:
      event->SetHandled();
      HandleClickOrTap(event);
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
      event->SetHandled();
      HandleClickOrTap(event);
      break;
    default:
      break;
  }
}

void AppListView::OnKeyEvent(ui::KeyEvent* event) {
  RedirectKeyEventToSearchBox(event);
}

void AppListView::OnWallpaperColorsChanged() {
  search_box_view_->OnWallpaperColorsChanged();
}

bool AppListView::HandleScroll(const gfx::Point& location,
                               const gfx::Vector2d& offset,
                               ui::EventType type) {
  if (ShouldIgnoreScrollEvents())
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

  // Forward events to `apps_grid_view`. This allows scroll events to the right
  // of the page switcher (not inside the apps grid) to switch pages.
  if (is_in_vertical_bounds) {
    apps_grid_view->HandleScrollFromParentView(offset, type);
  }
  return true;
}

void AppListView::SetState(AppListViewState new_state) {
  target_app_list_state_ = new_state;

  // Update the contents view state to match the app list view state.
  // Updating the contents view state may cause a nested `SetState()` call.
  // Bind the current state update to a weak ptr that gets invalidated when
  // `SetState()` gets called again to detect whether `SetState()` got called
  // again.
  set_state_weak_factory_.InvalidateWeakPtrs();
  base::WeakPtr<AppListView> set_state_request =
      set_state_weak_factory_.GetWeakPtr();

  SetChildViewsForStateTransition(new_state);

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
      app_list_state_ == new_state) {
    return;
  }

  MaybeCreateAccessibilityEvent(new_state);

  // Prepare state transition notifier for the new state transition.
  state_transition_notifier_->Reset(new_state);

  StartAnimationForState(new_state);
  RecordStateTransitionForUma(new_state);
  app_list_state_ = new_state;
  if (delegate_)
    delegate_->OnViewStateChanged(new_state);

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
  GetAppsContainerView()->UpdateControlVisibility(app_list_state_);
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
      case AppListViewState::kFullscreenAllApps:
        window->SetTitle(l10n_util::GetStringUTF16(
            IDS_APP_LIST_ALL_APPS_ACCESSIBILITY_ANNOUNCEMENT));
        break;
      case AppListViewState::kClosed:
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
  if (target_state == AppListViewState::kClosed &&
      delegate_->ShouldDismissImmediately()) {
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

  if (!app_list_features::IsAnimateScaleOnTabletModeTransitionEnabled())
    ApplyBoundsAnimation(target_state, animation_duration);

  app_list_main_view_->contents_view()->OnAppListViewTargetStateChanged(
      target_state);
  app_list_main_view_->contents_view()->AnimateToViewState(target_state,
                                                           animation_duration);
}

void AppListView::ApplyBoundsAnimation(AppListViewState target_state,
                                       base::TimeDelta duration_ms) {
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

  // Only report animation throughput for full state transitions - i.e. when the
  // starting app list view position matches the expected position for the
  // current app list state. The goal is to reduce noise introduced by partial
  // state transitions - for example
  // *   When interrupting another state transition half-way, in which case the
  //     layer has non-identity ransform.
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

  if (target_state != AppListViewState::kClosed) {
    DCHECK(target_state == AppListViewState::kFullscreenAllApps ||
           target_state == AppListViewState::kFullscreenSearch);
    TabletModeAnimationTransition transition_type =
        target_state == AppListViewState::kFullscreenAllApps
            ? TabletModeAnimationTransition::kEnterFullscreenAllApps
            : TabletModeAnimationTransition::kEnterFullscreenSearch;
    state_animation_metrics_reporter_->SetTabletModeAnimationTransition(
        transition_type);
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
  animation.SetTransitionDuration(duration_ms);
  animation.SetTweenType(gfx::Tween::EASE_OUT);
  layer->SetTransform(gfx::Transform());
}

void AppListView::SetStateFromSearchBoxView(bool search_box_is_empty,
                                            bool triggered_by_contents_change) {
  switch (target_app_list_state_) {
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

int AppListView::GetHeightForState(AppListViewState state) const {
  switch (app_list_state_) {
    case AppListViewState::kFullscreenAllApps:
    case AppListViewState::kFullscreenSearch:
      return GetFullscreenStateHeight();
    case AppListViewState::kClosed:
      return 0;
  }
}

int AppListView::GetFullscreenStateHeight() const {
  const display::Display display = GetDisplayNearestView();
  const gfx::Rect display_bounds = display.bounds();
  return display_bounds.height() - display.work_area().y() + display_bounds.y();
}

metrics_util::SmoothnessCallback
AppListView::GetStateTransitionMetricsReportCallback() {
  return state_animation_metrics_reporter_->GetReportCallback();
}

void AppListView::ResetTransitionMetricsReporter() {
  state_animation_metrics_reporter_->Reset();
}

void AppListView::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(GetWidget()->GetNativeView(), window);
  window->RemoveObserver(this);
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
  // for state changes with side shelf.
  delegate_->OnStateTransitionAnimationCompleted(target_state,
                                                 was_animation_interrupted);
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
      GetAppsContainerView()->app_list_folder_view()->UpdateShadowBounds();
      offset_to_show_folder_with_onscreen_keyboard_ = true;
    }
  } else if (offset_to_show_folder_with_onscreen_keyboard_) {
    // If the keyboard is closing or a folder isn't being shown, reset
    // the app list's position
    OffsetYPositionOfAppList(0);
    GetAppsContainerView()->app_list_folder_view()->UpdateShadowBounds();
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

bool AppListView::ShouldIgnoreScrollEvents() {
  if (app_list_state_ != AppListViewState::kFullscreenAllApps)
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
    case AppListViewState::kFullscreenAllApps:
    case AppListViewState::kFullscreenSearch:
      return fullscreen_height;
    case AppListViewState::kClosed:
      if (app_list_features::IsAnimateScaleOnTabletModeTransitionEnabled())
        return fullscreen_height;
      // Align the widget y with shelf y to avoid flicker in show animation.
      return work_area_bounds.bottom() - display.bounds().y();
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

void AppListView::OnTabletModeAnimationTransitionNotified(
    TabletModeAnimationTransition animation_transition) {
  state_animation_metrics_reporter_->SetTabletModeAnimationTransition(
      animation_transition);
}

void AppListView::ResetSubpixelPositionOffset(ui::Layer* layer) {
  const display::Display display = GetDisplayNearestView();
  const gfx::Rect& bounds = layer->bounds();
  layer->SetSubpixelPositionOffset(
      gfx::Vector2dF(ComputeSubpixelOffset(display, bounds.x()),
                     ComputeSubpixelOffset(display, bounds.y())));
}

}  // namespace ash
