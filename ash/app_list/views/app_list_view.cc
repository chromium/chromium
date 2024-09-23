// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_view.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

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
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
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

// The number of minutes that must pass for the current app list page to reset
// to the first page.
constexpr int kAppListPageResetTimeLimitMinutes = 20;

// When true, immediately fires the page reset timer upon starting.
bool skip_page_reset_timer_for_testing = false;

// This view forwards the focus to the search box widget by providing it as a
// FocusTraversable when a focus search is provided.
class SearchBoxFocusHost : public views::View {
  METADATA_HEADER(SearchBoxFocusHost, views::View)
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

 private:
  raw_ptr<views::Widget> search_box_widget_;
};

BEGIN_METADATA(SearchBoxFocusHost)
END_METADATA

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
      std::optional<TabletModeAnimationTransition> transition,
      int value);

  std::optional<TabletModeAnimationTransition> tablet_transition_;
};

void AppListView::StateAnimationMetricsReporter::Reset() {
  tablet_transition_.reset();
}

// static
void AppListView::StateAnimationMetricsReporter::RecordMetrics(
    std::optional<TabletModeAnimationTransition> tablet_transition,
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
      state_animation_metrics_reporter_(
          std::make_unique<StateAnimationMetricsReporter>()) {
  CHECK(delegate);
  // Default role of WidgetDelegate is ax::mojom::Role::kWindow which traps
  // ChromeVox focus within the root view. Assign ax::mojom::Role::kGroup here
  // to allow the focus to move from elements in app list view to search box.
  // TODO(pbos): Should this be necessary with the SetNextFocus() used
  // below?
  SetAccessibleWindowRole(ax::mojom::Role::kGroup);
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
}

void AppListView::InitContents() {
  DCHECK(!app_list_main_view_);
  DCHECK(!search_box_view_);

  a11y_announcer_ = std::make_unique<AppListA11yAnnouncer>(
      AddChildView(std::make_unique<views::View>()));

  auto app_list_main_view = std::make_unique<AppListMainView>(delegate_, this);
  auto search_box_view =
      std::make_unique<SearchBoxView>(app_list_main_view.get(), delegate_,
                                      /*is_app_list_bubble=*/false);
  search_box_view->InitializeForFullscreenLauncher();

  // Assign |app_list_main_view_| and |search_box_view_| here since they are
  // accessed during Init().
  app_list_main_view_ = AddChildView(std::move(app_list_main_view));
  search_box_view_ = AddChildView(std::move(search_box_view));
  app_list_main_view_->Init(0, search_box_view_);
}

void AppListView::InitWidget(gfx::NativeView parent) {
  DCHECK(!GetWidget());
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
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
  time_shown_ = std::nullopt;
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

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
    case ui::VKEY_BROWSER_BACK:
      Back();
      break;
    default:
      NOTREACHED();
  }

  // Don't let DialogClientView handle the accelerator.
  return true;
}

void AppListView::Layout(PassKey) {
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
  return gfx::Insets::TLBR(0, 0, delegate_->GetSystemShelfInsetsInTabletMode(),
                           0);
}

void AppListView::UpdateWidget() {
  // Set native view's bounds directly to avoid screen position controller
  // setting bounds in the display where the widget has the largest
  // intersection.
  GetWidget()->GetNativeView()->SetBounds(GetPreferredWidgetBounds());
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
       (event->AsGestureEvent()->type() == ui::EventType::kGestureLongPress ||
        event->AsGestureEvent()->type() == ui::EventType::kGestureLongTap ||
        event->AsGestureEvent()->type() ==
            ui::EventType::kGestureTwoFingerTap)) ||
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

void AppListView::MaybeCreateAccessibilityEvent(AppListViewState new_state) {
  if (new_state == app_list_state_ || !delegate_->AppListTargetVisibility())
    return;

  if (new_state == AppListViewState::kFullscreenAllApps)
    a11y_announcer_->AnnounceAppListShown();
}

void AppListView::EnsureWidgetBoundsMatchCurrentState() {
  const gfx::Rect new_target_bounds = GetPreferredWidgetBounds();
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
    // TODO(https://crbug.com/1356661): Consider not marking kMouseDragged as
    // handled here.
    case ui::EventType::kMousePressed:
    case ui::EventType::kMouseDragged:
      event->SetHandled();
      break;
    case ui::EventType::kMouseReleased:
      event->SetHandled();
      HandleClickOrTap(event);
      break;
    case ui::EventType::kMousewheel:
      if (HandleScroll(event->location(), event->AsMouseWheelEvent()->offset(),
                       ui::EventType::kMousewheel)) {
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
    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureLongPress:
    case ui::EventType::kGestureLongTap:
    case ui::EventType::kGestureTwoFingerTap:
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

  MaybeCreateAccessibilityEvent(new_state);

  app_list_main_view_->contents_view()->OnAppListViewTargetStateChanged(
      new_state);
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
  if (event->type() == ui::EventType::kKeyPressed) {
    search_box->InsertChar(*event);
  }
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

gfx::Rect AppListView::GetPreferredWidgetBounds() {
  // Use parent's width instead of display width to avoid 1 px gap (See
  // https://crbug.com/884889).
  CHECK(GetWidget());
  aura::Window* parent = GetWidget()->GetNativeView()->parent();
  CHECK(parent);

  // Note that app list container fills the screen, so we can treat the
  // container's y as the top of display.
  const display::Display display = GetDisplayNearestView();
  const gfx::Rect work_area_bounds = display.work_area();

  // The ChromeVox panel as well as the Docked Magnifier viewport affect the
  // workarea of the display. We need to account for that when applist is in
  // fullscreen to avoid being shown below them.
  const int preferred_widget_y = work_area_bounds.y() - display.bounds().y();

  return delegate_->SnapBoundsToDisplayEdge(
      gfx::Rect(0, preferred_widget_y, parent->bounds().width(),
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

BEGIN_METADATA(AppListView)
END_METADATA

}  // namespace ash
