// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_impl.h"

#include <optional>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_presenter_event_filter.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

using assistant::AssistantExitPoint;

// The target scale to which (or from which) the fullscreen launcher will
// animate between tablet <-> clamshell mode transition.
constexpr float kFullscreenLauncherFadeAnimationScale = 0.92f;

// The fade in/out animation duration for tablet <-> clamshell mode transition.
constexpr base::TimeDelta kFullscreenLauncherTransitionDuration =
    base::Milliseconds(350);

// Callback from the compositor when it presented a valid frame. Used to
// record UMA of input latency.
void DidPresentCompositorFrame(base::TimeTicks event_time_stamp,
                               bool is_showing,
                               const viz::FrameTimingDetails& details) {
  base::TimeTicks presentation_timestamp =
      details.presentation_feedback.timestamp;
  if (presentation_timestamp.is_null() || event_time_stamp.is_null() ||
      presentation_timestamp < event_time_stamp) {
    return;
  }
  const base::TimeDelta input_latency =
      presentation_timestamp - event_time_stamp;
  if (is_showing) {
    UMA_HISTOGRAM_TIMES("Apps.AppListShow.InputLatency", input_latency);
  } else {
    UMA_HISTOGRAM_TIMES("Apps.AppListHide.InputLatency", input_latency);
  }
}

// Invokes `complete_callback_` at the end of animation.
class FullscreenLauncherAnimationObserver
    : public ui::ImplicitAnimationObserver,
      public ui::LayerObserver {
 public:
  // Invoked with `true` if animation was aborted.
  using AnimationCompleteCallback = base::OnceCallback<void(bool)>;

  FullscreenLauncherAnimationObserver(
      ui::Layer* layer,
      AnimationCompleteCallback complete_callback)
      : layer_(layer), complete_callback_(std::move(complete_callback)) {
    DCHECK(layer_);
    layer_->AddObserver(this);
  }

  FullscreenLauncherAnimationObserver(
      const FullscreenLauncherAnimationObserver& other) = delete;
  FullscreenLauncherAnimationObserver& operator=(
      const FullscreenLauncherAnimationObserver& other) = delete;

  ~FullscreenLauncherAnimationObserver() override {
    StopObservingImplicitAnimations();
    layer_->RemoveObserver(this);
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    const bool aborted =
        WasAnimationAbortedForProperty(
            ui::LayerAnimationElement::AnimatableProperty::TRANSFORM) ||
        WasAnimationAbortedForProperty(
            ui::LayerAnimationElement::AnimatableProperty::OPACITY);
    std::move(complete_callback_).Run(aborted);
    delete this;
  }

  // ui::LayerObserver overrides:
  void LayerDestroyed(ui::Layer* layer) override {
    // Old `AppListView`'s layer can be cloned and then destroyed by
    // `ScreenRotationAnimator`. In this case run `complete_callback_` and
    // destroy `this`.
    std::move(complete_callback_).Run(false);
    delete this;
  }

 private:
  const raw_ptr<ui::Layer> layer_;
  AnimationCompleteCallback complete_callback_;
};

void UpdateTabletModeTransitionAnimationSettings(
    FullscreenLauncherAnimationObserver* animation_observer,
    ui::ScopedLayerAnimationSettings* settings) {
  settings->SetTransitionDuration(kFullscreenLauncherTransitionDuration);
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  if (animation_observer)
    settings->AddObserver(animation_observer);
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

constexpr std::array<int, 8>
    AppListPresenterImpl::kIdsOfContainersThatWontHideAppList;

AppListPresenterImpl::AppListPresenterImpl(AppListControllerImpl* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

AppListPresenterImpl::~AppListPresenterImpl() {
  if (view_ && view_->GetWidget()) {
    view_->GetWidget()->CloseNow();
  }
  CHECK(!views::WidgetObserver::IsInObserverList());
}

aura::Window* AppListPresenterImpl::GetWindow() const {
  return is_target_visibility_show_ && view_
             ? view_->GetWidget()->GetNativeWindow()
             : nullptr;
}

void AppListPresenterImpl::Show(AppListViewState preferred_state,
                                int64_t display_id,
                                base::TimeTicks event_time_stamp,
                                std::optional<AppListShowSource> show_source) {
  if (is_target_visibility_show_)
    return;

  if (!Shell::Get()->GetRootWindowForDisplayId(display_id)) {
    LOG(ERROR) << "Root window does not exist for display: " << display_id;
    return;
  }

  // TODO(https://crbug.com/1307871): Remove this when the linked crash gets
  // diagnosed - the crash is possible if app list gets dismissed while being
  // shown. `showing_app_list_` in intended to catch this case.
  showing_app_list_ = true;

  is_target_visibility_show_ = true;
  RequestPresentationTime(display_id, event_time_stamp);

  if (!view_) {
    AppListView* view = new AppListView(controller_);
    view->InitView(
        controller_->GetFullscreenLauncherContainerForDisplayId(display_id));
    SetView(view);
    view_->GetWidget()->GetNativeWindow()->TrackOcclusionState();
  }

  OnVisibilityWillChange(GetTargetVisibility(), display_id);
  controller_->UpdateFullscreenLauncherContainer(display_id);

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shelf* shelf =
      Shelf::ForWindow(view_->GetWidget()->GetNativeView()->GetRootWindow());
  shelf->shelf_layout_manager()->UpdateAutoHideState();

  // If presenter is observing a shelf instance different than `shelf`, it's
  // because the app list view on the associated display is closing. It's safe
  // to remove this observation (given that shelf background changes should not
  // affect appearance of a closing app list view).
  shelf_observer_.Reset();
  shelf_observer_.Observe(shelf);

  std::unique_ptr<AppListView::ScopedAccessibilityAnnouncementLock>
      scoped_accessibility_lock;

  // App list view state accessibility alerts should be suppressed when the app
  // list view is shown by the assistant. The assistant UI should handle its
  // own accessibility notifications.
  if (show_source && *show_source == AppListShowSource::kAssistantEntryPoint) {
    scoped_accessibility_lock =
        std::make_unique<AppListView::ScopedAccessibilityAnnouncementLock>(
            view_);
  }

  auto* layer = view_->GetWidget()->GetNativeWindow()->layer();

  bool has_aborted_animation = false;
  if (layer->GetAnimator()->is_animating()) {
    layer->GetAnimator()->AbortAllAnimations();
    // Mark that animation was aborted in order to keep initial opacity and
    // scale values in sync.
    has_aborted_animation = true;
  }
  // `0.01f` prevents a DCHECK error (widgets cannot be shown when visible and
  // fully transparent at the same time).
  const float initial_opacity =
      layer->opacity() == 0.0f ? 0.01f : layer->opacity();
  layer->SetOpacity(initial_opacity);

  view_->Show(preferred_state);

  // If there was no aborted dismiss animation before - set the initial value,
  // otherwise smoothly continue where it was aborted.
  if (!has_aborted_animation) {
    layer->SetTransform(
        gfx::GetScaleTransform(gfx::Rect(layer->size()).CenterPoint(),
                               kFullscreenLauncherFadeAnimationScale));
  }
  FullscreenLauncherAnimationObserver::AnimationCompleteCallback
      animation_complete_callback = base::BindOnce(
          &AppListPresenterImpl::OnTabletToClamshellTransitionAnimationDone,
          weak_ptr_factory_.GetWeakPtr(), /*target_visibility=*/true);
  auto* animation_observer = new FullscreenLauncherAnimationObserver(
      layer, std::move(animation_complete_callback));
  UpdateScaleAndOpacityForHomeLauncher(
      1.0f, 1.0f, std::nullopt,
      base::BindRepeating(&UpdateTabletModeTransitionAnimationSettings,
                          animation_observer));

  SnapAppListBoundsToDisplayEdge();

  event_filter_ =
      std::make_unique<AppListPresenterEventFilter>(controller_, this, view_);
  controller_->ViewShown(display_id);
  showing_app_list_ = false;

  OnVisibilityChanged(GetTargetVisibility(), display_id);
}

void AppListPresenterImpl::Dismiss(base::TimeTicks event_time_stamp) {
  if (!is_target_visibility_show_)
    return;

  // If the app list target visibility is shown, there should be an existing
  // view.
  DCHECK(view_);

  // TODO(https://crbug.com/1307871): Remove this when the linked crash gets
  // diagnosed.
  CHECK(!showing_app_list_);

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

  event_filter_.reset();

  if (view_->search_box_view()->is_search_box_active()) {
    // Close the virtual keyboard before the app list view is dismissed.
    // Otherwise if the browser is behind the app list view, after the latter is
    // closed, IME is updated because of the changed focus. Consequently,
    // the virtual keyboard is hidden for the wrong IME instance, which may
    // bring troubles when restoring the virtual keyboard (see
    // https://crbug.com/944233).
    keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();
  }

  controller_->ViewClosing();

  OnVisibilityWillChange(GetTargetVisibility(), GetDisplayId());

  if (!view_->GetWidget()->GetNativeWindow()->is_destroying()) {
    auto* const layer = view_->GetWidget()->GetNativeWindow()->layer();
    FullscreenLauncherAnimationObserver::AnimationCompleteCallback
        animation_complete_callback = base::BindOnce(
            &AppListPresenterImpl::OnTabletToClamshellTransitionAnimationDone,
            weak_ptr_factory_.GetWeakPtr(), /*target_visibility=*/false);
    auto* animation_observer = new FullscreenLauncherAnimationObserver(
        layer, std::move(animation_complete_callback));
    // Aborts show animation (if it's running, noop otherwise). This helps to
    // run dismiss animation smoothly from the aborted scale/opacity points.
    layer->GetAnimator()->AbortAllAnimations();
    UpdateScaleAndOpacityForHomeLauncher(
        kFullscreenLauncherFadeAnimationScale, 0.0f, std::nullopt,
        base::BindRepeating(&UpdateTabletModeTransitionAnimationSettings,
                            animation_observer));
    view_->SetState(AppListViewState::kClosed);
  }

  base::RecordAction(base::UserMetricsAction("Launcher_Dismiss"));
}

void AppListPresenterImpl::SetViewVisibility(bool visible) {
  if (!view_)
    return;
  view_->OnAppListVisibilityWillChange(visible);
  view_->SetVisible(visible);
}

bool AppListPresenterImpl::HandleCloseOpenFolder() {
  return is_target_visibility_show_ && view_ && view_->HandleCloseOpenFolder();
}

void AppListPresenterImpl::UpdateForNewSortingOrder(
    const std::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure) {
  if (!view_)
    return;

  base::OnceClosure done_closure;
  if (animate) {
    // The search box should ignore a11y events during the reorder animation
    // so that the announcement of app list reorder is made before that of
    // focus change.
    SetViewIgnoredForAccessibility(view_->search_box_view(), true);

    // Focus on the search box before starting the reorder animation to prevent
    // focus moving through app list items as they're being hidden for order
    // update animation.
    view_->search_box_view()->search_box()->RequestFocus();

    done_closure =
        base::BindOnce(&AppListPresenterImpl::OnAppListReorderAnimationDone,
                       weak_ptr_factory_.GetWeakPtr());
  }

  view_->app_list_main_view()
      ->contents_view()
      ->apps_container_view()
      ->UpdateForNewSortingOrder(new_order, animate,
                                 std::move(update_position_closure),
                                 std::move(done_closure));
}

void AppListPresenterImpl::UpdateContinueSectionVisibility() {
  if (!view_)
    return;

  view_->app_list_main_view()
      ->contents_view()
      ->apps_container_view()
      ->UpdateContinueSectionVisibility();
}

bool AppListPresenterImpl::IsVisibleDeprecated() const {
  return controller_->IsVisible(GetDisplayId());
}

bool AppListPresenterImpl::IsAtLeastPartiallyVisible() const {
  const auto* window = GetWindow();
  return window &&
         window->GetOcclusionState() == aura::Window::OcclusionState::VISIBLE;
}

bool AppListPresenterImpl::GetTargetVisibility() const {
  return is_target_visibility_show_;
}

void AppListPresenterImpl::UpdateScaleAndOpacityForHomeLauncher(
    float scale,
    float opacity,
    std::optional<TabletModeAnimationTransition> transition,
    UpdateHomeLauncherAnimationSettingsCallback callback) {
  // Exiting from overview in clamshell mode should not affect the hidden
  // fullscreen launcher.
  if (!view_ || view_->app_list_state() == AppListViewState::kClosed)
    return;

  ui::Layer* layer = view_->GetWidget()->GetNativeWindow()->layer();

  if (layer->GetAnimator()->is_animating()) {
    layer->GetAnimator()->StopAnimating();

    // Reset the animation metrics reporter when the animation is interrupted.
    view_->ResetTransitionMetricsReporter();
  }

  std::optional<ui::ScopedLayerAnimationSettings> settings;
  if (!callback.is_null()) {
    settings.emplace(layer->GetAnimator());
    callback.Run(&settings.value());
  }

  // The animation metrics reporter will run for opacity and transform
  // animations separately - to avoid reporting duplicated values, add the
  // reported for transform animation only.
  layer->SetOpacity(opacity);

  std::optional<ui::AnimationThroughputReporter> reporter;
  if (settings.has_value() && transition.has_value()) {
    view_->OnTabletModeAnimationTransitionNotified(*transition);
    reporter.emplace(settings->GetAnimator(),
                     metrics_util::ForSmoothnessV3(
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

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, private:

void AppListPresenterImpl::SetView(AppListView* view) {
  DCHECK(view_ == nullptr);
  DCHECK(is_target_visibility_show_);

  view_ = view;
  views::Widget* widget = view_->GetWidget();
  widget->AddObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);

  // Sync the |onscreen_keyboard_shown_| in case |view_| is not initiated when
  // the on-screen is shown.
  view_->set_onscreen_keyboard_shown(controller_->onscreen_keyboard_shown());
}

void AppListPresenterImpl::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->RemoveObserver(this);

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
  controller_->OnVisibilityChanged(visible, display_id);
}

void AppListPresenterImpl::OnVisibilityWillChange(bool visible,
                                                  int64_t display_id) {
  controller_->OnVisibilityWillChange(visible, display_id);
}

void AppListPresenterImpl::OnClosed() {
  if (!is_target_visibility_show_)
    shelf_observer_.Reset();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl,  aura::client::FocusChangeObserver implementation:

void AppListPresenterImpl::OnWindowFocused(aura::Window* gained_focus,
                                           aura::Window* lost_focus) {
  // Do not focus app list window in the Kiosk mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  if (!view_ || !is_target_visibility_show_)
    return;

  int gained_focus_container_id = kShellWindowId_Invalid;
  if (gained_focus) {
    gained_focus_container_id = gained_focus->GetId();
    const aura::Window* container = ash::GetContainerForWindow(gained_focus);
    if (container)
      gained_focus_container_id = container->GetId();
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

  const bool app_list_gained_focus = applist_window->Contains(gained_focus) ||
                                     applist_container->Contains(gained_focus);
  const bool app_list_lost_focus =
      gained_focus ? gained_focus_hides_app_list
                   : (lost_focus && applist_container->Contains(lost_focus));
  // Either the app list has just gained focus, in which case it is already
  // visible or will very soon be, or it has neither gained nor lost focus
  // and it might still be partially visible just because the focused window
  // doesn't occlude it completely.
  const bool visible = app_list_gained_focus ||
                       (IsAtLeastPartiallyVisible() && !app_list_lost_focus);

  if (visible != controller_->IsVisible(GetDisplayId())) {
    if (app_list_gained_focus)
      view_->OnHomeLauncherGainingFocusWithoutAnimation();

    OnVisibilityChanged(visible, GetDisplayId());
  } else {
    // In tablet mode, when Assistant UI lost focus after other new App window
    // opened, we should reset the view.
    if (app_list_lost_focus && IsShowingEmbeddedAssistantUI())
      view_->Back();
  }

  if (app_list_gained_focus)
    base::RecordAction(base::UserMetricsAction("AppList_WindowFocused"));
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
  OnClosed();
}

void AppListPresenterImpl::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  DCHECK_EQ(view_->GetWidget(), widget);
  OnVisibilityChanged(visible, GetDisplayId());
}

void AppListPresenterImpl::RequestPresentationTime(
    int64_t display_id,
    base::TimeTicks event_time_stamp) {
  if (event_time_stamp.is_null())
    return;
  aura::Window* root_window =
      Shell::Get()->GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return;
  ui::Compositor* compositor = root_window->layer()->GetCompositor();
  if (!compositor)
    return;
  compositor->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindOnce(&DidPresentCompositorFrame, event_time_stamp,
                     is_target_visibility_show_));
}

////////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver implementation:

void AppListPresenterImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!GetWindow())
    return;

  view_->OnParentWindowBoundsChanged();
  SnapAppListBoundsToDisplayEdge();
}

void AppListPresenterImpl::OnShelfShuttingDown() {
  shelf_observer_.Reset();
}

void AppListPresenterImpl::SnapAppListBoundsToDisplayEdge() {
  CHECK(view_ && view_->GetWidget());
  aura::Window* window = view_->GetWidget()->GetNativeView();
  const gfx::Rect bounds =
      controller_->SnapBoundsToDisplayEdge(window->bounds());
  window->SetBounds(bounds);
}

void AppListPresenterImpl::OnAppListReorderAnimationDone() {
  if (!view_)
    return;

  // Re-enable the search box to handle a11y events.
  SetViewIgnoredForAccessibility(view_->search_box_view(), false);
}

void AppListPresenterImpl::OnTabletToClamshellTransitionAnimationDone(
    bool target_visibility,
    bool aborted) {
  if (!view_)
    return;

  auto* window = view_->GetWidget()->GetNativeWindow();

  if (!aborted) {
    if (target_visibility) {
      view_->DeprecatedLayoutImmediately();
    } else if (!target_visibility && !window->is_destroying()) {
      window->Hide();
      OnClosed();
    }
  }

  controller_->OnStateTransitionAnimationCompleted(view_->app_list_state(),
                                                   aborted);
}

}  // namespace ash
