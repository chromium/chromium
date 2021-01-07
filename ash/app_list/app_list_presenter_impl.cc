// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_impl.h"

#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

constexpr std::array<int, 6> kIdsOfContainersThatWontHideAppList = {
    kShellWindowId_AppListContainer,     kShellWindowId_HomeScreenContainer,
    kShellWindowId_MenuContainer,        kShellWindowId_SettingBubbleContainer,
    kShellWindowId_ShelfBubbleContainer, kShellWindowId_ShelfContainer,
};

inline ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Callback from the compositor when it presented a valid frame. Used to
// record UMA of input latency.
void DidPresentCompositorFrame(base::TimeTicks event_time_stamp,
                               bool is_showing,
                               const gfx::PresentationFeedback& feedback) {
  const base::TimeTicks present_time = feedback.timestamp;
  if (present_time.is_null() || event_time_stamp.is_null() ||
      present_time < event_time_stamp) {
    return;
  }
  const base::TimeDelta input_latency = present_time - event_time_stamp;
  if (is_showing) {
    UMA_HISTOGRAM_TIMES(kAppListShowInputLatencyHistogram, input_latency);
  } else {
    UMA_HISTOGRAM_TIMES(kAppListHideInputLatencyHistogram, input_latency);
  }
}

// Implicit animation observer that runs a scoped closure runner, and deletes
// itself when the observed implicit animations complete.
class CallbackRunnerLayerAnimationObserver
    : public ui::ImplicitAnimationObserver {
 public:
  explicit CallbackRunnerLayerAnimationObserver(
      base::ScopedClosureRunner closure_runner)
      : closure_runner_(std::move(closure_runner)) {}
  ~CallbackRunnerLayerAnimationObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    closure_runner_.RunAndReset();
    delete this;
  }

 private:
  base::ScopedClosureRunner closure_runner_;
};

}  // namespace

AppListPresenterImpl::AppListPresenterImpl(
    std::unique_ptr<AppListPresenterDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  delegate_->SetPresenter(this);
}

AppListPresenterImpl::~AppListPresenterImpl() {
  Dismiss(base::TimeTicks());
  // Ensures app list view goes before the controller since pagination model
  // lives in the controller and app list view would access it on destruction.
  if (view_) {
    view_->GetAppsPaginationModel()->RemoveObserver(this);
    if (view_->GetWidget())
      view_->GetWidget()->CloseNow();
  }
  CHECK(!IsInObserverList());
}

aura::Window* AppListPresenterImpl::GetWindow() const {
  return is_target_visibility_show_ && view_
             ? view_->GetWidget()->GetNativeWindow()
             : nullptr;
}

void AppListPresenterImpl::Show(AppListViewState preferred_state,
                                int64_t display_id,
                                base::TimeTicks event_time_stamp) {
  if (is_target_visibility_show_) {
    // Launcher is always visible on the internal display when home launcher is
    // enabled in tablet mode.
    if (delegate_->IsTabletMode() || display_id == GetDisplayId())
      return;

    Dismiss(event_time_stamp);
  }

  if (!delegate_->GetRootWindowForDisplayId(display_id)) {
    LOG(ERROR) << "Root window does not exist for display: " << display_id;
    return;
  }

  is_target_visibility_show_ = true;
  OnVisibilityWillChange(GetTargetVisibility(), display_id);
  RequestPresentationTime(display_id, event_time_stamp);

  if (!view_) {
    // Note |delegate_| outlives the AppListView.
    AppListView* view = new AppListView(delegate_->GetAppListViewDelegate());
    delegate_->Init(view, display_id);
    SetView(view);
    view_->GetWidget()->GetNativeWindow()->TrackOcclusionState();
  }
  delegate_->ShowForDisplay(preferred_state, display_id);

  OnVisibilityChanged(GetTargetVisibility(), display_id);
}

void AppListPresenterImpl::Dismiss(base::TimeTicks event_time_stamp) {
  if (!is_target_visibility_show_)
    return;

  // If the app list target visibility is shown, there should be an existing
  // view.
  DCHECK(view_);

  is_target_visibility_show_ = false;
  RequestPresentationTime(GetDisplayId(), event_time_stamp);

  // Hide the active window if it is a transient descendant of |view_|'s widget.
  aura::Window* window = view_->GetWidget()->GetNativeWindow();
  aura::Window* active_window =
      ::wm::GetActivationClient(window->GetRootWindow())->GetActiveWindow();
  if (active_window) {
    aura::Window* transient_parent =
        ::wm::TransientWindowManager::GetOrCreate(active_window)
            ->transient_parent();
    while (transient_parent) {
      if (window == transient_parent) {
        active_window->Hide();
        break;
      }
      transient_parent =
          ::wm::TransientWindowManager::GetOrCreate(transient_parent)
              ->transient_parent();
    }
  }

  // The dismissal may have occurred in response to the app list losing
  // activation. Otherwise, our widget is currently active. When the animation
  // completes we'll hide the widget, changing activation. If a menu is shown
  // before the animation completes then the activation change triggers the menu
  // to close. By deactivating now we ensure there is no activation change when
  // the animation completes and any menus stay open.
  if (view_->GetWidget()->IsActive())
    view_->GetWidget()->Deactivate();

  delegate_->OnClosing();

  OnVisibilityWillChange(GetTargetVisibility(), GetDisplayId());
  view_->SetState(AppListViewState::kClosed);
  base::RecordAction(base::UserMetricsAction("Launcher_Dismiss"));
}

void AppListPresenterImpl::SetViewVisibility(bool visible) {
  if (!view_)
    return;
  view_->SetVisible(visible);
  view_->search_box_view()->SetVisible(visible);
}

bool AppListPresenterImpl::HandleCloseOpenFolder() {
  return is_target_visibility_show_ && view_ && view_->HandleCloseOpenFolder();
}

ShelfAction AppListPresenterImpl::ToggleAppList(
    int64_t display_id,
    AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  bool request_fullscreen = show_source == kSearchKeyFullscreen ||
                            show_source == kShelfButtonFullscreen;
  // Dismiss or show based on the target visibility because the show/hide
  // animation can be reversed.
  if (is_target_visibility_show_ && GetDisplayId() == display_id) {
    if (request_fullscreen) {
      if (view_->app_list_state() == AppListViewState::kPeeking) {
        view_->SetState(AppListViewState::kFullscreenAllApps);
        return SHELF_ACTION_APP_LIST_SHOWN;
      } else if (view_->app_list_state() == AppListViewState::kHalf) {
        view_->SetState(AppListViewState::kFullscreenSearch);
        return SHELF_ACTION_APP_LIST_SHOWN;
      }
    }
    Dismiss(event_time_stamp);
    return SHELF_ACTION_APP_LIST_DISMISSED;
  }
  Show(request_fullscreen ? AppListViewState::kFullscreenAllApps
                          : AppListViewState::kPeeking,
       display_id, event_time_stamp);
  return SHELF_ACTION_APP_LIST_SHOWN;
}

bool AppListPresenterImpl::IsVisibleDeprecated() const {
  return delegate_->IsVisible(GetDisplayId());
}

bool AppListPresenterImpl::IsAtLeastPartiallyVisible() const {
  const auto* window = GetWindow();
  return window &&
         window->occlusion_state() == aura::Window::OcclusionState::VISIBLE;
}

bool AppListPresenterImpl::GetTargetVisibility() const {
  return is_target_visibility_show_;
}

void AppListPresenterImpl::UpdateYPositionAndOpacity(float y_position_in_screen,
                                                     float background_opacity) {
  if (!is_target_visibility_show_)
    return;

  if (view_)
    view_->UpdateYPositionAndOpacity(y_position_in_screen, background_opacity);
}

void AppListPresenterImpl::EndDragFromShelf(AppListViewState app_list_state) {
  if (view_)
    view_->EndDragFromShelf(app_list_state);
}

void AppListPresenterImpl::ProcessMouseWheelOffset(
    const gfx::Point& location,
    const gfx::Vector2d& scroll_offset_vector) {
  if (view_)
    view_->HandleScroll(location, scroll_offset_vector, ui::ET_MOUSEWHEEL);
}

void AppListPresenterImpl::UpdateScaleAndOpacityForHomeLauncher(
    float scale,
    float opacity,
    base::Optional<TabletModeAnimationTransition> transition,
    UpdateHomeLauncherAnimationSettingsCallback callback) {
  if (!view_)
    return;

  ui::Layer* layer = view_->GetWidget()->GetNativeWindow()->layer();
  if (!delegate_->IsTabletMode()) {
    // In clamshell mode, set the opacity of the AppList immediately to
    // instantly hide it. Opacity of the AppList is reset when it is shown
    // again.
    layer->SetOpacity(opacity);
    return;
  }

  if (layer->GetAnimator()->is_animating()) {
    layer->GetAnimator()->StopAnimating();

    // Reset the animation metrics reporter when the animation is interrupted.
    view_->ResetTransitionMetricsReporter();
  }

  base::Optional<ui::ScopedLayerAnimationSettings> settings;
  if (!callback.is_null()) {
    settings.emplace(layer->GetAnimator());
    callback.Run(&settings.value());

    // Disable suggestion chips blur during animations to improve performance.
    base::ScopedClosureRunner blur_disabler =
        view_->app_list_main_view()
            ->contents_view()
            ->apps_container_view()
            ->DisableSuggestionChipsBlur();
    // The observer will delete itself when the animations are completed.
    settings->AddObserver(
        new CallbackRunnerLayerAnimationObserver(std::move(blur_disabler)));
  }

  // The animation metrics reporter will run for opacity and transform
  // animations separately - to avoid reporting duplicated values, add the
  // reported for transform animation only.
  layer->SetOpacity(opacity);

  base::Optional<ui::AnimationThroughputReporter> reporter;
  if (settings.has_value() && transition.has_value()) {
    view_->OnTabletModeAnimationTransitionNotified(*transition);
    reporter.emplace(settings->GetAnimator(),
                     metrics_util::ForSmoothness(
                         view_->GetStateTransitionMetricsReportCallback()));
  }

  gfx::Transform transform =
      gfx::GetScaleTransform(gfx::Rect(layer->size()).CenterPoint(), scale);
  layer->SetTransform(transform);
}

void AppListPresenterImpl::ShowEmbeddedAssistantUI(bool show) {
  if (view_)
    view_->app_list_main_view()->contents_view()->ShowEmbeddedAssistantUI(show);
}

bool AppListPresenterImpl::IsShowingEmbeddedAssistantUI() const {
  if (view_) {
    return view_->app_list_main_view()
        ->contents_view()
        ->IsShowingEmbeddedAssistantUI();
  }

  return false;
}

void AppListPresenterImpl::SetExpandArrowViewVisibility(bool show) {
  if (view_) {
    view_->app_list_main_view()->contents_view()->SetExpandArrowViewVisibility(
        show);
  }
}

void AppListPresenterImpl::OnTabletModeChanged(bool started) {
  if (started) {
    if (GetTargetVisibility())
      view_->OnTabletModeChanged(true);
  } else {
    if (IsVisibleDeprecated())
      view_->OnTabletModeChanged(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, private:

void AppListPresenterImpl::SetView(AppListView* view) {
  DCHECK(view_ == nullptr);
  DCHECK(is_target_visibility_show_);

  view_ = view;
  views::Widget* widget = view_->GetWidget();
  widget->AddObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);
  view_->GetAppsPaginationModel()->AddObserver(this);

  // Sync the |onscreen_keyboard_shown_| in case |view_| is not initiated when
  // the on-screen is shown.
  view_->set_onscreen_keyboard_shown(delegate_->GetOnScreenKeyboardShown());
}

void AppListPresenterImpl::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->RemoveObserver(this);

  view_->GetAppsPaginationModel()->RemoveObserver(this);

  view_ = nullptr;
}

int64_t AppListPresenterImpl::GetDisplayId() const {
  views::Widget* widget = view_ ? view_->GetWidget() : nullptr;
  if (!widget)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(widget->GetNativeView())
      .id();
}

void AppListPresenterImpl::OnVisibilityChanged(bool visible,
                                               int64_t display_id) {
  delegate_->OnVisibilityChanged(visible, display_id);
}

void AppListPresenterImpl::OnVisibilityWillChange(bool visible,
                                                  int64_t display_id) {
  delegate_->OnVisibilityWillChange(visible, display_id);
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl,  aura::client::FocusChangeObserver implementation:

void AppListPresenterImpl::OnWindowFocused(aura::Window* gained_focus,
                                           aura::Window* lost_focus) {
  if (!view_ || !is_target_visibility_show_)
    return;

  int gained_focus_container_id = kShellWindowId_Invalid;
  if (gained_focus) {
    gained_focus_container_id = gained_focus->id();
    const aura::Window* container =
        delegate_->GetContainerForWindow(gained_focus);
    if (container)
      gained_focus_container_id = container->id();
  }
  aura::Window* applist_window = view_->GetWidget()->GetNativeView();
  const aura::Window* applist_container = applist_window->parent();

  // An AppList dialog window, or a child window of the system tray, may
  // take focus from the AppList window. Don't consider this a visibility
  // change since the app list is still visible for the most part.
  const bool gained_focus_hides_app_list =
      gained_focus_container_id != kShellWindowId_Invalid &&
      !base::Contains(kIdsOfContainersThatWontHideAppList,
                      gained_focus_container_id);

  const bool app_list_gained_focus = applist_window->Contains(gained_focus);
  const bool app_list_lost_focus =
      gained_focus ? gained_focus_hides_app_list
                   : (lost_focus && applist_container->Contains(lost_focus));
  // Either the app list has just gained focus, in which case it is already
  // visible or will very soon be, or it has neither gained nor lost focus
  // and it might still be partially visible just because the focused window
  // doesn't occlude it completely.
  const bool visible = app_list_gained_focus ||
                       (IsAtLeastPartiallyVisible() && !app_list_lost_focus);

  if (delegate_->IsTabletMode()) {
    if (visible != delegate_->IsVisible(GetDisplayId())) {
      if (app_list_gained_focus)
        view_->OnHomeLauncherGainingFocusWithoutAnimation();

      OnVisibilityChanged(visible, GetDisplayId());
    }
  }

  if (app_list_gained_focus)
    base::RecordAction(base::UserMetricsAction("AppList_WindowFocused"));

  if (app_list_lost_focus && !switches::ShouldNotDismissOnBlur() &&
      !delegate_->IsTabletMode()) {
    Dismiss(base::TimeTicks());
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, ui::ImplicitAnimationObserver implementation:

void AppListPresenterImpl::OnImplicitAnimationsCompleted() {
  StopObservingImplicitAnimations();

  // This class observes the closing animation only.
  OnVisibilityChanged(GetTargetVisibility(), GetDisplayId());

  if (is_target_visibility_show_) {
    view_->GetWidget()->Activate();
  } else {
    // Hide the widget so it can be re-shown without re-creating it.
    view_->GetWidget()->Hide();
    delegate_->OnClosed();
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, views::WidgetObserver implementation:

void AppListPresenterImpl::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(view_->GetWidget(), widget);
  if (is_target_visibility_show_)
    Dismiss(base::TimeTicks());
  ResetView();
}

void AppListPresenterImpl::OnWidgetDestroyed(views::Widget* widget) {
  delegate_->OnClosed();
}

void AppListPresenterImpl::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  DCHECK_EQ(view_->GetWidget(), widget);
  OnVisibilityChanged(visible, GetDisplayId());
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, PaginationModelObserver implementation:

void AppListPresenterImpl::TotalPagesChanged(int previous_page_count,
                                             int new_page_count) {}

void AppListPresenterImpl::SelectedPageChanged(int old_selected,
                                               int new_selected) {
  current_apps_page_ = new_selected;
}

void AppListPresenterImpl::RequestPresentationTime(
    int64_t display_id,
    base::TimeTicks event_time_stamp) {
  if (event_time_stamp.is_null())
    return;
  aura::Window* root_window = delegate_->GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return;
  ui::Compositor* compositor = root_window->layer()->GetCompositor();
  if (!compositor)
    return;
  compositor->RequestPresentationTimeForNextFrame(
      base::BindOnce(&DidPresentCompositorFrame, event_time_stamp,
                     is_target_visibility_show_));
}

}  // namespace ash
