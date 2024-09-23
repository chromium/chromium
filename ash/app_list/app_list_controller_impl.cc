// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include <string_view>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_badge_controller.h"
#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/apps_collections_controller.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/feature_discovery_duration_reporter.h"
#include "ash/public/cpp/feature_discovery_metric_util.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/barrier_closure.h"
#include "base/callback_list.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/wm/core/scoped_animation_disabler.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

using assistant::AssistantEntryPoint;
using assistant::AssistantExitPoint;

namespace {

constexpr char kHomescreenAnimationHistogram[] =
    "Ash.Homescreen.AnimationSmoothness";

// The target scale to which (or from which) the home launcher will animate when
// overview is being shown (or hidden) using fade transitions while home
// launcher is shown.
constexpr float kOverviewFadeAnimationScale = 0.92f;

// The home launcher animation duration for transitions that accompany overview
// fading transitions.
constexpr base::TimeDelta kOverviewFadeAnimationDuration =
    base::Milliseconds(350);

// The app id for the settings app used for testing quick app access.
constexpr char kOsSettingsAppId[] = "odknhmnlageboeamepcngndbggdpaobj";

// Update layer animation settings for launcher scale and opacity animation that
// runs on overview mode change.
void UpdateOverviewSettings(base::TimeDelta duration,
                            ui::ScopedLayerAnimationSettings* settings) {
  settings->SetTransitionDuration(kOverviewFadeAnimationDuration);
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

// Layer animation observer that waits for layer animator to schedule, and
// complete animations. When all animations complete, it fires |callback| and
// deletes itself.
class WindowAnimationsCallback : public ui::LayerAnimationObserver {
 public:
  WindowAnimationsCallback(base::OnceClosure callback,
                           ui::LayerAnimator* animator)
      : callback_(std::move(callback)), animator_(animator) {
    subscription_ = animator_->AddSequenceScheduledCallback(
        base::BindRepeating(&WindowAnimationsCallback::OnSequenceScheduled,
                            base::Unretained(this)));
  }
  WindowAnimationsCallback(const WindowAnimationsCallback&) = delete;
  WindowAnimationsCallback& operator=(const WindowAnimationsCallback&) = delete;
  ~WindowAnimationsCallback() override = default;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    FireCallbackIfDone();
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    FireCallbackIfDone();
  }
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  void OnDetachedFromSequence(ui::LayerAnimationSequence* sequence) override {
    FireCallbackIfDone();
  }

 private:
  void OnSequenceScheduled(ui::LayerAnimationSequence* sequence) {
    // LayerAnimationSequence::RemoveObserver is called by the ancestor during
    // destruction.
    sequence->AddObserver(this);
  }

  // Fires the callback if all scheduled animations completed (either ended or
  // got aborted).
  void FireCallbackIfDone() {
    if (!callback_ || animator_->is_animating())
      return;
    std::move(callback_).Run();
    delete this;
  }

  base::OnceClosure callback_;
  raw_ptr<ui::LayerAnimator>
      animator_;  // Owned by the layer that is animating.
  base::CallbackListSubscription subscription_;
};

// Minimizes all windows in |windows| that aren't in the home screen container,
// and are not in |windows_to_ignore|. Done in reverse order to preserve the mru
// ordering.
// Returns true if any windows are minimized.
bool MinimizeAllWindows(const aura::Window::Windows& windows,
                        const aura::Window::Windows& windows_to_ignore) {
  aura::Window* container = Shell::Get()->GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_HomeScreenContainer);
  aura::Window::Windows windows_to_minimize;
  for (aura::Window* window : base::Reversed(windows)) {
    if (!container->Contains(window) &&
        !base::Contains(windows_to_ignore, window) &&
        !WindowState::Get(window)->IsMinimized()) {
      windows_to_minimize.push_back(window);
    }
  }

  window_util::MinimizeAndHideWithoutAnimation(windows_to_minimize);
  return !windows_to_minimize.empty();
}

TabletModeAnimationTransition CalculateAnimationTransitionForMetrics(
    HomeLauncherAnimationTrigger trigger,
    bool launcher_should_show) {
  switch (trigger) {
    case HomeLauncherAnimationTrigger::kHideForWindow:
      return TabletModeAnimationTransition::kHideHomeLauncherForWindow;
    case HomeLauncherAnimationTrigger::kLauncherButton:
      return TabletModeAnimationTransition::kHomeButtonShow;
    case HomeLauncherAnimationTrigger::kOverviewModeFade:
      return launcher_should_show
                 ? TabletModeAnimationTransition::kFadeOutOverview
                 : TabletModeAnimationTransition::kFadeInOverview;
  }
}

PrefService* GetLastActiveUserPrefService() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

// Gets the MRU window shown over the applist when in tablet mode.
// Returns nullptr if no windows are shown over the applist.
aura::Window* GetTopVisibleWindow() {
  std::vector<raw_ptr<aura::Window, VectorExperimental>> window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          DesksMruType::kActiveDesk);
  for (aura::Window* window : window_list) {
    if (!window->TargetVisibility() || WindowState::Get(window)->IsMinimized())
      continue;

    // Floated windows can be tucked offscreen in tablet mode. Their target
    // visibility is true but the app list is fully visible under them.
    if (WindowState::Get(window)->IsFloated() &&
        Shell::Get()->float_controller()->IsFloatedWindowTuckedForTablet(
            window)) {
      continue;
    }

    return window;
  }
  return nullptr;
}

void LogAppListShowSource(AppListShowSource show_source, bool app_list_bubble) {
  if (app_list_bubble) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListBubbleShowSource", show_source);
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListShowSource", show_source);
}

std::optional<TabletModeAnimationTransition>
GetTransitionFromMetricsAnimationInfo(
    std::optional<HomeLauncherAnimationInfo> animation_info) {
  if (!animation_info.has_value())
    return std::nullopt;

  return CalculateAnimationTransitionForMetrics(animation_info->trigger,
                                                animation_info->showing);
}

bool IsKioskSession() {
  return Shell::Get()->session_controller()->IsRunningInAppMode();
}

void MaybeLogWelcomeTourInteraction(AppListShowSource show_source) {
  if (features::IsWelcomeTourEnabled() &&
      IsAppListShowSourceUserTriggered(show_source)) {
    welcome_tour_metrics::RecordInteraction(
        GetLastActiveUserPrefService(),
        welcome_tour_metrics::Interaction::kLauncher);
  }
}

bool IsAssistantExitPointScreenshot(
    std::optional<assistant::AssistantExitPoint> exit_point) {
  return exit_point == AssistantExitPoint::kScreenshot;
}

bool IsAssistantExitPointInsideLauncher(
    std::optional<assistant::AssistantExitPoint> exit_point) {
  return exit_point == AssistantExitPoint::kBackInLauncher ||
         exit_point == AssistantExitPoint::kLauncherSearchIphChip;
}

}  // namespace

AppListControllerImpl::AppListControllerImpl()
    : model_provider_(std::make_unique<AppListModelProvider>()),
      fullscreen_presenter_(std::make_unique<AppListPresenterImpl>(this)),
      bubble_presenter_(std::make_unique<AppListBubblePresenter>(this)),
      badge_controller_(std::make_unique<AppListBadgeController>()),
      apps_collections_controller_(
          std::make_unique<AppsCollectionsController>()) {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  session_controller->AddObserver(this);

  // In case of crash-and-restart case where session state starts with ACTIVE
  // and does not change to trigger OnSessionStateChanged(), notify the current
  // session state here to ensure that the app list is shown.
  OnSessionStateChanged(session_controller->GetSessionState());

  Shell* shell = Shell::Get();
  WallpaperController::Get()->AddObserver(this);
  shell->AddShellObserver(this);
  shell->overview_controller()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
  AssistantState::Get()->AddObserver(this);
  shell->display_manager()->AddDisplayManagerObserver(this);
  AssistantController::Get()->AddObserver(this);
  AssistantUiController::Get()->GetModel()->AddObserver(this);
  FeatureDiscoveryDurationReporter::GetInstance()->AddObserver(this);
}

AppListControllerImpl::~AppListControllerImpl() {
  if (tracked_app_window_) {
    tracked_app_window_->RemoveObserver(this);
    tracked_app_window_ = nullptr;
  }

  if (has_session_started_)
    RecordMetricsOnSessionEnd();

  // If this is being destroyed before the Shell starts shutting down, first
  // remove this from objects it's observing.
  if (!is_shutdown_)
    Shutdown();

  if (client_)
    client_->OnAppListControllerDestroyed();

  // Dismiss the window before `fullscreen_presenter_` is reset, because
  // Dimiss() may call back into this object and access `fullscreen_presenter_`.
  fullscreen_presenter_->Dismiss(base::TimeTicks());
}

// static
void AppListControllerImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kLauncherFeedbackOnContinueSectionSent, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kLauncherContinueSectionHidden, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterTimePref(prefs::kLauncherLastContinueRequestTime,
                             base::Time());
  registry->RegisterBooleanPref(prefs::kLauncherUseLongContinueDelay, false);

  // The prefs for launcher search controls.
  registry->RegisterDictionaryPref(prefs::kLauncherSearchCategoryControlStatus);

  registry->RegisterTimePref(prefs::kLauncherSearchLastFileScanLogTime,
                             base::Time());

  // The prefs for apps collections experiment.
  registry->RegisterIntegerPref(
      prefs::kLauncherAppsCollectionsExperimentArm,
      static_cast<int>(
          AppsCollectionsController::ExperimentalArm::kDefaultValue));
}

void AppListControllerImpl::SetClient(AppListClient* client) {
  client_ = client;
  apps_collections_controller_->SetClient(client);
}

AppListClient* AppListControllerImpl::GetClient() {
  DCHECK(client_);
  return client_;
}

AppListNotifier* AppListControllerImpl::GetNotifier() {
  if (!client_)
    return nullptr;
  return client_->GetNotifier();
}

std::unique_ptr<ScopedIphSession>
AppListControllerImpl::CreateLauncherSearchIphSession() {
  if (!client_) {
    return nullptr;
  }
  return client_->CreateLauncherSearchIphSession();
}

void AppListControllerImpl::SetActiveModel(
    int profile_id,
    AppListModel* model,
    SearchModel* search_model,
    QuickAppAccessModel* quick_app_access_model) {
  profile_id_ = profile_id;
  model_provider_->SetActiveModel(model, search_model, quick_app_access_model);
  UpdateSearchBoxUiVisibilities();
}

void AppListControllerImpl::ClearActiveModel() {
  profile_id_ = kAppListInvalidProfileID;
  model_provider_->ClearActiveModel();
  UpdateSearchBoxUiVisibilities();
}

void AppListControllerImpl::DismissAppList() {
  if (tracked_app_window_) {
    tracked_app_window_->RemoveObserver(this);
    tracked_app_window_ = nullptr;
  }

  // Don't check tablet mode here. This function can be called during tablet
  // mode transitions and we always want to close anyway.
  bubble_presenter_->Dismiss();

  fullscreen_presenter_->Dismiss(base::TimeTicks());
}

void AppListControllerImpl::ShowAppList(AppListShowSource source) {
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  last_open_source_ = source;
  Show(GetDisplayIdToShowAppListOn(), source, base::TimeTicks(),
       /*should_record_metrics=*/true);
}

AppListShowSource AppListControllerImpl::LastAppListShowSource() {
  DCHECK(last_open_source_.has_value());
  return last_open_source_.value();
}

aura::Window* AppListControllerImpl::GetWindow() {
  if (IsInTabletMode()) {
    return fullscreen_presenter_->GetWindow();
  }
  return bubble_presenter_->GetWindow();
}

bool AppListControllerImpl::IsVisible(
    const std::optional<int64_t>& display_id) {
  return last_visible_ && (!display_id.has_value() ||
                           display_id.value() == last_visible_display_id_);
}

bool AppListControllerImpl::IsVisible() {
  return IsVisible(std::nullopt);
}

void AppListControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (IsKioskSession())
    return;

  if (!IsInTabletMode()) {
    DismissAppList();
    return;
  }

  // The app list is not dismissed before switching user, suggestion chips will
  // not be shown. So reset app list state and trigger an initial search here to
  // update the suggestion results.
  if (fullscreen_presenter_->GetView()) {
    fullscreen_presenter_->GetView()->CloseOpenedPage();
    fullscreen_presenter_->GetView()->search_box_view()->ClearSearch();
  }
}

void AppListControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state == session_manager::SessionState::ACTIVE)
    has_session_started_ = true;

  const bool in_clamshell = !IsInTabletMode();
  if (state != session_manager::SessionState::ACTIVE || IsKioskSession()) {
    if (in_clamshell)
      DismissAppList();
    return;
  }

  if (base::FeatureList::IsEnabled(features::kQuickAppAccessTestUI)) {
    SetHomeButtonQuickApp(kOsSettingsAppId);
  }

  if (in_clamshell)
    return;

  // Show the app list after signing in in tablet mode. For metrics, the app
  // list is not considered shown since the browser window is shown over app
  // list upon login.
  if (!fullscreen_presenter_->GetTargetVisibility())
    ShowHomeScreen(AppListShowSource::kTabletMode);

  // Hide app list UI initially to prevent app list from flashing in background
  // while the initial app window is being shown.
  if (!last_target_visible_ && !ShouldHomeLauncherBeVisible())
    fullscreen_presenter_->SetViewVisibility(false);
  else
    OnVisibilityChanged(true, last_visible_display_id_);
}

void AppListControllerImpl::OnUserSessionAdded(const AccountId& account_id) {
  if (!client_)
    return;

  ash::ReportPrefSortOrderOnSessionStart(client_->GetPermanentSortingOrder(),
                                         IsInTabletMode());

  auto* prefs =
      Shell::Get()->session_controller()->GetUserPrefServiceForUser(account_id);
  if (features::IsLauncherNudgeSessionResetEnabled()) {
    AppListNudgeController::ResetPrefsForNewUserSession(prefs);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Methods used in Ash

bool AppListControllerImpl::GetTargetVisibility(
    const std::optional<int64_t>& display_id) const {
  return last_target_visible_ &&
         (!display_id.has_value() ||
          display_id.value() == last_target_visible_display_id_);
}

void AppListControllerImpl::Show(int64_t display_id,
                                 AppListShowSource show_source,
                                 base::TimeTicks event_time_stamp,
                                 bool should_record_metrics) {
  if (IsKioskSession())
    return;

  last_open_source_ = show_source;
  if (should_record_metrics)
    LogAppListShowSource(show_source, !IsInTabletMode());

  // Checking `should_record_metrics` is redundant here, since this helper
  // function never logs metrics when the app list was shown by tablet mode.
  MaybeLogWelcomeTourInteraction(show_source);

  if (IsInTabletMode()) {
    fullscreen_presenter_->Show(AppListViewState::kFullscreenAllApps,
                                display_id, event_time_stamp, show_source);
    return;
  }

  bubble_presenter_->Show(display_id);
}

void AppListControllerImpl::UpdateAppListWithNewTemporarySortOrder(
    const std::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure) {
  TRACE_EVENT0("ui",
               "AppListControllerImpl::UpdateAppListWithNewTemporarySortOrder");
  if (new_order) {
    RecordAppListSortAction(*new_order, IsInTabletMode());

    FeatureDiscoveryDurationReporter* reporter =
        FeatureDiscoveryDurationReporter::GetInstance();
    reporter->MaybeFinishObservation(feature_discovery::TrackableFeature::
                                         kAppListReorderAfterEducationNudge);
    reporter->MaybeFinishObservation(
        feature_discovery::TrackableFeature::
            kAppListReorderAfterEducationNudgePerTabletMode);
    reporter->MaybeFinishObservation(feature_discovery::TrackableFeature::
                                         kAppListReorderAfterSessionActivation);
  }

  // Adapt the bubble app list to the new sorting order. NOTE: the bubble app
  // list is visible only in clamshell mode. Therefore do not animate in tablet
  // mode.
  const bool is_tablet_mode = IsInTabletMode();
  bubble_presenter_->UpdateForNewSortingOrder(
      new_order, !is_tablet_mode && animate,
      is_tablet_mode ? base::NullCallback()
                     : std::move(update_position_closure));

  // Adapt the fullscreen app list to the new sorting order. NOTE: the full
  // screen app list is visible only in tablet mode. Therefore do not animate in
  // clamshell mode.
  fullscreen_presenter_->UpdateForNewSortingOrder(
      new_order, is_tablet_mode && animate,
      is_tablet_mode ? std::move(update_position_closure)
                     : base::NullCallback());

  // Notify the AppsCollectionsController that there was a reorder.
  apps_collections_controller_->SetAppsReordered();
}

ShelfAction AppListControllerImpl::ToggleAppList(
    int64_t display_id,
    AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return SHELF_ACTION_APP_LIST_DISMISSED;
  }

  if (IsKioskSession())
    return SHELF_ACTION_APP_LIST_DISMISSED;

  if (IsInTabletMode()) {
    bool handled = GoHome(display_id);

    // Perform the "back" action for the app list.
    if (!handled) {
      Back();
      return SHELF_ACTION_APP_LIST_BACK;
    }
    LogAppListShowSource(show_source, /*app_list_bubble=*/false);
    last_open_source_ = show_source;

    MaybeLogWelcomeTourInteraction(show_source);

    return SHELF_ACTION_APP_LIST_SHOWN;
  }

  ShelfAction action = bubble_presenter_->Toggle(display_id);
  if (action == SHELF_ACTION_APP_LIST_SHOWN) {
    LogAppListShowSource(show_source, /*app_list_bubble=*/true);
    last_open_source_ = show_source;

    MaybeLogWelcomeTourInteraction(show_source);
  }
  return action;
}

bool AppListControllerImpl::GoHome(int64_t display_id) {
  if (IsKioskSession())
    return false;

  DCHECK(IsInTabletMode());

  if (fullscreen_presenter_->IsShowingEmbeddedAssistantUI()) {
    // OnHomeLauncherAnimationComplete() may not be called if the
    // `foreground_windows` is empty. Call AssistantUiController::CloseUi() here
    // directly.
    AssistantUiController::Get()->CloseUi(AssistantExitPoint::kLauncherClose);
    fullscreen_presenter_->ShowEmbeddedAssistantUI(false);
  }

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  const bool split_view_active = split_view_controller->InSplitViewMode();

  // The home screen opens for the current active desk, there's no need to
  // minimize windows in the inactive desks.
  aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);

  // The foreground window or windows (for split mode) - the windows that will
  // not be minimized without animations (instead they will be animated into the
  // home screen).
  std::vector<raw_ptr<aura::Window, VectorExperimental>> foreground_windows;
  if (split_view_active) {
    foreground_windows = {split_view_controller->primary_window(),
                          split_view_controller->secondary_window()};
    std::erase_if(foreground_windows,
                  [](aura::Window* window) { return !window; });
  } else if (!windows.empty() && !WindowState::Get(windows[0])->IsMinimized()) {
    foreground_windows.push_back(windows[0]);
  }

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (split_view_active) {
    // If overview session is active (e.g. on one side of the split view), end
    // it immediately, to prevent overview UI being visible while transitioning
    // to home screen.
    if (overview_controller->InOverviewSession()) {
      overview_controller->EndOverview(OverviewEndAction::kEnterHomeLauncher,
                                       OverviewEnterExitType::kImmediateExit);
    }

    // End split view mode.
    split_view_controller->EndSplitView(
        SplitViewController::EndReason::kHomeLauncherPressed);
  }

  // If overview is active (if overview was active in split view, it exited by
  // this point), just fade it out to home screen.
  if (overview_controller->InOverviewSession()) {
    overview_controller->EndOverview(OverviewEndAction::kEnterHomeLauncher,
                                     OverviewEnterExitType::kFadeOutExit);
    return true;
  }

  // First minimize all inactive windows.
  const bool window_minimized =
      MinimizeAllWindows(windows, foreground_windows /*windows_to_ignore*/);

  if (foreground_windows.empty())
    return window_minimized;

  {
    // Disable window animations before updating home launcher target
    // position. Calling OnHomeLauncherPositionChanged() can cause
    // display work area update, and resulting cross-fade window bounds change
    // animation can interfere with WindowTransformToHomeScreenAnimation
    // visuals.
    //
    // TODO(crbug.com/40656009): This can be removed once transitions
    // between in-app state and home do not cause work area updates.
    std::vector<std::unique_ptr<wm::ScopedAnimationDisabler>>
        animation_disablers;
    for (aura::Window* window : foreground_windows) {
      animation_disablers.push_back(
          std::make_unique<wm::ScopedAnimationDisabler>(window));
    }

    OnHomeLauncherPositionChanged(/*percent_shown=*/100, display_id);
  }

  StartTrackingAnimationSmoothness(display_id);

  base::RepeatingClosure window_transforms_callback = base::BarrierClosure(
      foreground_windows.size(),
      base::BindOnce(&AppListControllerImpl::OnGoHomeWindowAnimationsEnded,
                     weak_ptr_factory_.GetWeakPtr(), display_id));

  // Minimize currently active windows, but this time, using animation.
  // Home screen will show when all the windows are done minimizing.
  for (aura::Window* foreground_window : foreground_windows) {
    if (::wm::WindowAnimationsDisabled(foreground_window)) {
      WindowState::Get(foreground_window)->Minimize();
      window_transforms_callback.Run();
    } else {
      // Create animator observer that will fire |window_transforms_callback|
      // once the window layer stops animating - it deletes itself when
      // animations complete.
      new WindowAnimationsCallback(window_transforms_callback,
                                   foreground_window->layer()->GetAnimator());
      WindowState::Get(foreground_window)->Minimize();
    }
  }

  return true;
}

bool AppListControllerImpl::ShouldHomeLauncherBeVisible() const {
  if (!IsInTabletMode() || IsKioskSession()) {
    return false;
  }

  if (home_launcher_transition_state_ ==
      HomeLauncherTransitionState::kMostlyShown) {
    return true;
  }

  return !Shell::Get()->overview_controller()->InOverviewSession() &&
         !GetTopVisibleWindow();
}

void AppListControllerImpl::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  if (!IsInTabletMode()) {
    DismissAppList();
  }
}

void AppListControllerImpl::OnShellDestroying() {
  // Stop observing at the beginning of ~Shell to avoid unnecessary work during
  // Shell shutdown.
  Shutdown();
}

void AppListControllerImpl::OnOverviewModeStarting() {
  const OverviewEnterExitType overview_enter_type =
      Shell::Get()
          ->overview_controller()
          ->overview_session()
          ->enter_exit_overview_type();

  const bool animate =
      IsHomeScreenVisible() &&
      overview_enter_type == OverviewEnterExitType::kFadeInEnter;

  UpdateForOverviewModeChange(/*show_home_launcher=*/false, animate);

  if (IsInTabletMode()) {
    const int64_t display_id = last_visible_display_id_;
    OnVisibilityWillChange(false /*shown*/, display_id);
  } else {
    DismissAppList();
  }
}

void AppListControllerImpl::OnOverviewModeStartingAnimationComplete(
    bool canceled) {
  if (!IsInTabletMode()) {
    return;
  }

  // If overview start was canceled, overview end animations are about to start.
  // Preemptively update the target app list visibility.
  if (canceled) {
    OnVisibilityWillChange(!GetTopVisibleWindow(), last_visible_display_id_);
    return;
  }

  OnVisibilityChanged(false /* shown */, last_visible_display_id_);
}

void AppListControllerImpl::OnOverviewModeEnding(OverviewSession* session) {
  // The launcher will be shown after overview mode finishes animating, in
  // OnOverviewModeEndingAnimationComplete(). Overview however is nullptr by
  // the time the animations are finished, so cache the exit type here.
  overview_exit_type_ = std::make_optional(session->enter_exit_overview_type());

  // If the overview is fading out, start the home launcher animation in
  // parallel. Otherwise the transition will be initiated in
  // OnOverviewModeEndingAnimationComplete().
  if (session->enter_exit_overview_type() ==
      OverviewEnterExitType::kFadeOutExit) {
    UpdateForOverviewModeChange(/*show_home_launcher=*/true, /*animate=*/true);

    // Make sure the window visibility is updated, in case it was previously
    // hidden due to overview being shown.
    UpdateHomeScreenVisibility();
  }

  if (!IsInTabletMode()) {
    return;
  }

  // Overview state might end during home launcher transition - if that is the
  // case, respect the final state set by in-progress home launcher transition.
  if (home_launcher_transition_state_ != HomeLauncherTransitionState::kFinished)
    return;

  OnVisibilityWillChange(!GetTopVisibleWindow() /*shown*/,
                         last_visible_display_id_);
}

void AppListControllerImpl::OnOverviewModeEnded() {
  if (!IsInTabletMode()) {
    return;
  }

  // Overview state might end during home launcher transition - if that is the
  // case, respect the final state set by in-progress home launcher transition.
  if (home_launcher_transition_state_ != HomeLauncherTransitionState::kFinished)
    return;
  OnVisibilityChanged(!GetTopVisibleWindow(), last_visible_display_id_);
}

void AppListControllerImpl::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  DCHECK(overview_exit_type_.has_value());

  // For kFadeOutExit OverviewEnterExitType, the home animation is scheduled in
  // OnOverviewModeEnding(), so there is nothing else to do at this point.
  if (canceled || *overview_exit_type_ == OverviewEnterExitType::kFadeOutExit) {
    overview_exit_type_ = std::nullopt;
    return;
  }

  const bool animate =
      *overview_exit_type_ == OverviewEnterExitType::kFadeOutExit;
  overview_exit_type_ = std::nullopt;

  UpdateForOverviewModeChange(/*show_home_launcher=*/true, animate);

  // Make sure the window visibility is updated, in case it was previously
  // hidden due to overview being shown.
  UpdateHomeScreenVisibility();
}

void AppListControllerImpl::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  UpdateHomeScreenVisibility();
}

void AppListControllerImpl::OnChangedToInTabletMode() {
  if (IsKioskSession()) {
    return;
  }

  // Reset the keyboard traversal mode to prevent using the value saved in
  // clamshell mode.
  SetKeyboardTraversalMode(false);

  bubble_presenter_->Dismiss();

  // Show the app list if the tablet mode starts.
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::ACTIVE) {
    ShowHomeScreen(AppListShowSource::kTabletMode);
  }
  UpdateFullscreenLauncherContainer();

  // If the app list is visible before the transition to tablet mode,
  // AppListPresenter relies on the active window change to detect the app list
  // view got hidden behind a window. Though, app list UI moving behind an app
  // window does not always cause an active window change:
  // *   If the app list is still being shown - given that app list takes focus
  //     from the top window only when it's fully shown, the focus will remain
  //     within the app window throughout the tablet mode transition.
  // *   If the assistant UI is visible before the tablet mode transition - the
  //     assistant will keep the focus during transition, even though the app
  //     window will be shown over the app list view.
  // Ensure the app list visibility is properly updated if the app list is
  // hidden behind a window at this point.
  if (last_target_visible_ && !ShouldHomeLauncherBeVisible()) {
    OnVisibilityChanged(false, last_visible_display_id_);
  }
}

void AppListControllerImpl::OnChangedToInClamshellMode() {
  // Reset the keyboard traversal mode to prevent using the value saved last
  // time in tablet mode.
  SetKeyboardTraversalMode(false);

  aura::Window* window = fullscreen_presenter_->GetWindow();
  base::AutoReset<bool> auto_reset(
      &should_dismiss_immediately_,
      window && RootWindowController::ForWindow(window)
                    ->GetShelfLayoutManager()
                    ->HasVisibleWindow());
  UpdateFullscreenLauncherContainer();

  // Dismiss the app list if the tablet mode ends.
  DismissAppList();
}

void AppListControllerImpl::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      // Do nothing when the tablet state is still in the process of transition.
      break;
    case display::TabletState::kInTabletMode:
      OnChangedToInTabletMode();
      break;
    case display::TabletState::kInClamshellMode:
      OnChangedToInClamshellMode();
      break;
  }
}

void AppListControllerImpl::OnWallpaperPreviewStarted() {
  in_wallpaper_preview_ = true;
  UpdateHomeScreenVisibility();
}

void AppListControllerImpl::OnWallpaperPreviewEnded() {
  in_wallpaper_preview_ = false;
  UpdateHomeScreenVisibility();
}

void AppListControllerImpl::OnKeyboardVisibilityChanged(const bool is_visible) {
  onscreen_keyboard_shown_ = is_visible;
  AppListView* app_list_view = fullscreen_presenter_->GetView();
  if (app_list_view)
    app_list_view->OnScreenKeyboardShown(is_visible);
}

void AppListControllerImpl::OnAssistantStatusChanged(
    assistant::AssistantStatus status) {
  UpdateSearchBoxUiVisibilities();
}

void AppListControllerImpl::OnAssistantSettingsEnabled(bool enabled) {
  UpdateSearchBoxUiVisibilities();
}

void AppListControllerImpl::OnAssistantFeatureAllowedChanged(
    assistant::AssistantAllowedState state) {
  UpdateSearchBoxUiVisibilities();
}

void AppListControllerImpl::OnDidApplyDisplayChanges() {
  // Entering tablet mode triggers a display configuration change when we
  // automatically switch to mirror mode. Switching to mirror mode happens
  // asynchronously (see DisplayConfigurationObserver::OnTabletModeStarted()).
  // This may result in the removal of a window tree host, as in the example of
  // switching to tablet mode while Unified Desktop mode is on; the Unified host
  // will be destroyed and the Home Launcher (which was created earlier when we
  // entered tablet mode) will be dismissed.
  // To avoid crashes, we must ensure that the Home Launcher shown status is as
  // expected if it's enabled and we're still in tablet mode.
  // https://crbug.com/900956.
  const bool should_be_shown =
      IsInTabletMode() &&
      Shell::Get()->session_controller()->GetSessionState() ==
          session_manager::SessionState::ACTIVE;

  if (!should_be_shown ||
      should_be_shown == fullscreen_presenter_->GetTargetVisibility()) {
    return;
  }
  ShowHomeScreen(AppListShowSource::kTabletMode);
}

void AppListControllerImpl::OnAssistantReady() {
  UpdateSearchBoxUiVisibilities();
}

void AppListControllerImpl::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  const bool is_old_visibility_closing =
      (old_visibility == AssistantVisibility::kClosing);

  switch (new_visibility) {
    case AssistantVisibility::kVisible:
      DVLOG(1) << "Assistant becoming visible";
      if (!IsVisible() || is_old_visibility_closing) {
        std::optional<AppListView::ScopedContentsResetDisabler> disabler;
        if (is_old_visibility_closing) {
          // Avoid resetting the contents view when the transition to close the
          // Assistant ui is going to be reversed.
          if (fullscreen_presenter_->GetView())
            disabler.emplace(fullscreen_presenter_->GetView());

          // Reset `close_assistant_ui_runner_` because the Assistant ui is
          // going to show.
          DCHECK(close_assistant_ui_runner_);
          IgnoreResult(close_assistant_ui_runner_.Release());
        }

        Show(GetDisplayIdToShowAppListOn(),
             AppListShowSource::kAssistantEntryPoint, base::TimeTicks(),
             /*should_record_metrics=*/true);
      }
      if (!IsInTabletMode()) {
        bubble_presenter_->ShowEmbeddedAssistantUI();
      } else {
        if (!fullscreen_presenter_->IsShowingEmbeddedAssistantUI() ||
            is_old_visibility_closing) {
          fullscreen_presenter_->ShowEmbeddedAssistantUI(true);
        }

        // Make sure that app list views are visible - they might get hidden
        // during session startup, and the app list visibility might not have
        // yet changed to visible by this point. https://crbug.com/1040751
        fullscreen_presenter_->SetViewVisibility(true);
      }
      break;
    case AssistantVisibility::kClosed:
      if (!IsShowingEmbeddedAssistantUI())
        break;

      // When Launcher is closing, we do not want to call
      // |ShowEmbeddedAssistantUI(false)|, which will show previous state page
      // in Launcher and make the UI flash.
      if (IsInTabletMode()) {
        std::optional<ContentsView::ScopedSetActiveStateAnimationDisabler>
            set_active_state_animation_disabler;
        // When taking a screenshot by Assistant, we do not want to animate to
        // the final state. Otherwise the screenshot may have transient state
        // during the animation. In tablet mode, we want to go back to
        // kStateApps immediately, i.e. skipping the animation in
        // |SetActiveStateInternal|, which are called from
        // |ShowEmbeddedAssistantUI(false)| and
        // |ClearSearchAndDeactivateSearchBox()|.
        if (IsAssistantExitPointScreenshot(exit_point)) {
          set_active_state_animation_disabler.emplace(
              fullscreen_presenter_->GetView()
                  ->app_list_main_view()
                  ->contents_view());
        }

        fullscreen_presenter_->ShowEmbeddedAssistantUI(false);

        if (!IsAssistantExitPointInsideLauncher(exit_point)) {
          fullscreen_presenter_->GetView()
              ->search_box_view()
              ->ClearSearchAndDeactivateSearchBox();
        }
      } else if (!IsAssistantExitPointInsideLauncher(exit_point)) {
        // Similarly, when taking a screenshot by Assistant in clamshell mode,
        // we do not want to dismiss launcher with animation. Otherwise the
        // screenshot may have transient state during the animation.
        base::AutoReset<bool> auto_reset(
            &should_dismiss_immediately_,
            IsAssistantExitPointScreenshot(exit_point));
        DismissAppList();
      }
      break;
    case AssistantVisibility::kClosing:
      break;
  }
}

void AppListControllerImpl::OnHomeLauncherAnimationComplete(
    bool shown,
    int64_t display_id) {
  home_launcher_transition_state_ = HomeLauncherTransitionState::kFinished;

  AssistantUiController::Get()->CloseUi(
      shown ? AssistantExitPoint::kLauncherOpen
            : AssistantExitPoint::kLauncherClose);

  // Animations can be reversed (e.g. in a drag). Let's ensure the target
  // visibility is correct first.
  OnVisibilityChanged(shown, display_id);
}

void AppListControllerImpl::OnHomeLauncherPositionChanged(int percent_shown,
                                                          int64_t display_id) {
  const bool mostly_shown = percent_shown >= 50;
  home_launcher_transition_state_ =
      mostly_shown ? HomeLauncherTransitionState::kMostlyShown
                   : HomeLauncherTransitionState::kMostlyHidden;
  OnVisibilityWillChange(mostly_shown, display_id);
}

aura::Window* AppListControllerImpl::GetHomeScreenWindow() const {
  return fullscreen_presenter_->GetWindow();
}

void AppListControllerImpl::UpdateScaleAndOpacityForHomeLauncher(
    float scale,
    float opacity,
    std::optional<HomeLauncherAnimationInfo> animation_info,
    UpdateAnimationSettingsCallback callback) {
  DCHECK(!animation_info.has_value() || !callback.is_null());

  fullscreen_presenter_->UpdateScaleAndOpacityForHomeLauncher(
      scale, opacity,
      GetTransitionFromMetricsAnimationInfo(std::move(animation_info)),
      std::move(callback));
}

void AppListControllerImpl::Back() {
  fullscreen_presenter_->GetView()->Back();
}

void AppListControllerImpl::SetKeyboardTraversalMode(bool engaged) {
  if (keyboard_traversal_engaged_ == engaged)
    return;

  keyboard_traversal_engaged_ = engaged;
  AssistantUiController::Get()->SetKeyboardTraversalMode(engaged);

  // No need to schedule paint for bubble presenter.
  if (bubble_presenter_->IsShowing())
    return;

  AppListView* app_list_view = fullscreen_presenter_->GetView();
  // May be null in tests of bubble presenter.
  if (!app_list_view)
    return;
  views::View* focused_view =
      app_list_view->GetFocusManager()->GetFocusedView();

  if (!focused_view)
    return;

  // When the search box has focus, it is actually the textfield that has focus.
  // As such, the |SearchBoxView| must be told to repaint directly.
  if (focused_view == app_list_view->search_box_view()->search_box()) {
    app_list_view->search_box_view()->UpdateSearchBoxFocusPaint();
  } else if (AppListToastView::IsToastButton(focused_view)) {
    // Toast button can become focused after app list sorting, so make sure the
    // focus ring appears correctly when updating `keyboard_traversal_engaged_`.
    focused_view->SchedulePaint();
  } else {
    // Ensure that when an app list item's focus ring is triggered by key
    // events, the item is selected.
    // TODO(https://crbug.com/1262236): class name comparision and static cast
    // should be avoided in the production code. Find a better way to guarantee
    // the item's selection status.
    if (std::string_view(focused_view->GetClassName()) ==
        std::string_view(AppListItemView::kViewClassName)) {
      static_cast<AppListItemView*>(focused_view)->EnsureSelected();
    }

    focused_view->SchedulePaint();
  }
}

bool AppListControllerImpl::IsShowingEmbeddedAssistantUI() const {
  return bubble_presenter_->IsShowingEmbeddedAssistantUI() ||
         fullscreen_presenter_->IsShowingEmbeddedAssistantUI();
}

void AppListControllerImpl::SetStateTransitionAnimationCallbackForTesting(
    StateTransitionAnimationCallback callback) {
  state_transition_animation_callback_ = std::move(callback);
}

void AppListControllerImpl::SetHomeLauncherAnimationCallbackForTesting(
    HomeLauncherAnimationCallback callback) {
  home_launcher_animation_callback_ = std::move(callback);
}

void AppListControllerImpl::RecordShelfAppLaunched() {
  RecordAppListAppLaunched(
      AppListLaunchedFrom::kLaunchedFromShelf,
      recorded_app_list_view_state_.value_or(GetAppListViewState()),
      IsInTabletMode(), recorded_app_list_visibility_.value_or(last_visible_));
  recorded_app_list_view_state_ = std::nullopt;
  recorded_app_list_visibility_ = std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
// Methods of |client_|:

void AppListControllerImpl::StartAssistant(
    assistant::AssistantEntryPoint entry_point) {
  AssistantUiController::Get()->ShowUi(entry_point);
  UpdateSearchBoxUiVisibilities();
}

void AppListControllerImpl::EndAssistant(
    assistant::AssistantExitPoint exit_point) {
  AssistantUiController::Get()->CloseUi(exit_point);
}

std::vector<AppListSearchControlCategory>
AppListControllerImpl::GetToggleableCategories() const {
  if (client_) {
    return client_->GetToggleableCategories();
  }
  return std::vector<AppListSearchControlCategory>();
}

void AppListControllerImpl::StartSearch(const std::u16string& raw_query) {
  if (client_) {
    std::u16string query;
    base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
    client_->StartSearch(query);
  }
}

void AppListControllerImpl::StartZeroStateSearch(base::OnceClosure callback,
                                                 base::TimeDelta timeout) {
  if (client_)
    client_->StartZeroStateSearch(std::move(callback), timeout);
}

void AppListControllerImpl::OpenSearchResult(const std::string& result_id,
                                             int event_flags,
                                             AppListLaunchedFrom launched_from,
                                             AppListLaunchType launch_type,
                                             int suggestion_index,
                                             bool launch_as_default) {
  SearchModel* search_model = GetSearchModel();
  SearchResult* result = search_model->FindSearchResult(result_id);
  if (!result)
    return;

  if (launch_type == AppListLaunchType::kAppSearchResult) {
    switch (launched_from) {
      case AppListLaunchedFrom::kLaunchedFromSearchBox:
      case AppListLaunchedFrom::kLaunchedFromRecentApps:
        RecordAppLaunched(launched_from);
        break;
      case AppListLaunchedFrom::kLaunchedFromGrid:
      case AppListLaunchedFrom::kLaunchedFromShelf:
      case AppListLaunchedFrom::kLaunchedFromContinueTask:
      case AppListLaunchedFrom::kLaunchedFromQuickAppAccess:
      case AppListLaunchedFrom::kLaunchedFromAppsCollections:
      case AppListLaunchedFrom::kLaunchedFromDiscoveryChip:
        break;
      case AppListLaunchedFrom::DEPRECATED_kLaunchedFromSuggestionChip:
        NOTREACHED();
    }
  }

  const bool is_tablet_mode = IsInTabletMode();
  switch (launched_from) {
    case AppListLaunchedFrom::kLaunchedFromSearchBox:
      switch (launch_type) {
        case AppListLaunchType::kSearchResult:
          RecordLauncherWorkflowMetrics(AppListUserAction::kOpenSearchResult,
                                        is_tablet_mode, last_show_timestamp_);
          break;
        case AppListLaunchType::kAppSearchResult:
          RecordLauncherWorkflowMetrics(AppListUserAction::kOpenAppSearchResult,
                                        is_tablet_mode, last_show_timestamp_);
          break;
        case AppListLaunchType::kApp:
          NOTREACHED();
      }
      break;
    case AppListLaunchedFrom::kLaunchedFromContinueTask:
      RecordLauncherWorkflowMetrics(AppListUserAction::kOpenContinueSectionTask,
                                    is_tablet_mode, last_show_timestamp_);
      break;
    case AppListLaunchedFrom::kLaunchedFromGrid:
    case AppListLaunchedFrom::kLaunchedFromRecentApps:
    case AppListLaunchedFrom::kLaunchedFromShelf:
    case AppListLaunchedFrom::DEPRECATED_kLaunchedFromSuggestionChip:
    case AppListLaunchedFrom::kLaunchedFromQuickAppAccess:
    case AppListLaunchedFrom::kLaunchedFromAppsCollections:
    case AppListLaunchedFrom::kLaunchedFromDiscoveryChip:
      NOTREACHED();
  }

  base::RecordAction(base::UserMetricsAction("AppList_OpenSearchResult"));

  if (client_) {
    client_->OpenSearchResult(profile_id_, result_id, event_flags,
                              launched_from, launch_type, suggestion_index,
                              launch_as_default);
  }

  ResetHomeLauncherIfShown();
}

void AppListControllerImpl::InvokeSearchResultAction(
    const std::string& result_id,
    SearchResultActionType action) {
  if (client_)
    client_->InvokeSearchResultAction(result_id, action);
}

void AppListControllerImpl::ViewShown(int64_t display_id) {
  UpdateSearchBoxUiVisibilities();

  // Note that IsHomeScreenVisible() might still return false at this point, as
  // the home screen visibility takes into account whether the app list view is
  // obscured by an app window, or overview UI. This method gets called when the
  // app list view widget visibility changes (regardless of whether anything is
  // stacked above the home screen).
  aura::Window* window = GetHomeScreenWindow();
  split_view_observation_.Observe(SplitViewController::Get(window));
  UpdateHomeScreenVisibility();

  // Ensure search box starts fresh with no ring each time it opens.
  keyboard_traversal_engaged_ = false;
}

bool AppListControllerImpl::AppListTargetVisibility() const {
  return last_target_visible_;
}

void AppListControllerImpl::ViewClosing() {
  split_view_observation_.Reset();
}

void AppListControllerImpl::ActivateItem(const std::string& id,
                                         int event_flags,
                                         AppListLaunchedFrom launched_from,
                                         bool is_app_above_the_fold) {
  RecordAppLaunched(launched_from);

  const bool is_tablet_mode = IsInTabletMode();
  switch (launched_from) {
    case AppListLaunchedFrom::kLaunchedFromGrid:
      RecordLauncherWorkflowMetrics(AppListUserAction::kAppLaunchFromAppsGrid,
                                    is_tablet_mode, last_show_timestamp_);
      break;
    case AppListLaunchedFrom::kLaunchedFromRecentApps:
      RecordLauncherWorkflowMetrics(AppListUserAction::kAppLaunchFromRecentApps,
                                    is_tablet_mode, last_show_timestamp_);
      break;
    case AppListLaunchedFrom::kLaunchedFromQuickAppAccess:
    // Metrics for quick app launch already recorded at RecordApplaunched().
    case AppListLaunchedFrom::kLaunchedFromAppsCollections:
    // Metrics for apps collections launch recorded by the
    // AppListViewDelegate.
    case AppListLaunchedFrom::kLaunchedFromDiscoveryChip:
      // Metrics for discovery chip already recorded at RecordApplaunched().
      break;
    case AppListLaunchedFrom::kLaunchedFromContinueTask:
    case AppListLaunchedFrom::kLaunchedFromSearchBox:
    case AppListLaunchedFrom::kLaunchedFromShelf:
    case AppListLaunchedFrom::DEPRECATED_kLaunchedFromSuggestionChip:
      NOTREACHED();
  }

  if (client_)
    client_->ActivateItem(profile_id_, id, event_flags, launched_from,
                          is_app_above_the_fold);

  ResetHomeLauncherIfShown();
}

void AppListControllerImpl::GetContextMenuModel(
    const std::string& id,
    AppListItemContext item_context,
    GetContextMenuModelCallback callback) {
  if (client_)
    client_->GetContextMenuModel(profile_id_, id, item_context,
                                 std::move(callback));
}

void AppListControllerImpl::ShowWallpaperContextMenu(
    const gfx::Point& onscreen_location,
    ui::MenuSourceType source_type) {
  Shell::Get()->ShowContextMenu(onscreen_location, source_type);
}

bool AppListControllerImpl::KeyboardTraversalEngaged() {
  return keyboard_traversal_engaged_;
}

bool AppListControllerImpl::CanProcessEventsOnApplistViews() {
  // Do not allow processing events during overview or while overview is
  // finished but still animating out.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession() ||
      overview_controller->IsCompletingShutdownAnimations()) {
    return false;
  }

  return true;
}

bool AppListControllerImpl::ShouldDismissImmediately() {
  return should_dismiss_immediately_;
}

AssistantViewDelegate* AppListControllerImpl::GetAssistantViewDelegate() {
  return Shell::Get()->assistant_controller()->view_delegate();
}

void AppListControllerImpl::OnSearchResultVisibilityChanged(
    const std::string& id,
    bool visibility) {
  if (client_)
    client_->OnSearchResultVisibilityChanged(id, visibility);
}

bool AppListControllerImpl::IsAssistantAllowedAndEnabled() const {
  if (!Shell::Get()->assistant_controller()->IsAssistantReady())
    return false;

  auto* state = AssistantState::Get();
  return state->settings_enabled().value_or(false) &&
         state->allowed_state() == assistant::AssistantAllowedState::ALLOWED &&
         state->assistant_status() != assistant::AssistantStatus::NOT_READY;
}

void AppListControllerImpl::OnStateTransitionAnimationCompleted(
    AppListViewState state,
    bool was_animation_interrupted) {
  if (!was_animation_interrupted &&
      !state_transition_animation_callback_.is_null()) {
    state_transition_animation_callback_.Run(state);
  }

  MaybeCloseAssistant();
}

void AppListControllerImpl::MaybeCloseAssistant() {
  if (close_assistant_ui_runner_)
    close_assistant_ui_runner_.RunAndReset();
}

AppListViewState AppListControllerImpl::GetAppListViewState() const {
  return app_list_view_state_;
}

void AppListControllerImpl::OnViewStateChanged(AppListViewState state) {
  DVLOG(1) << __PRETTY_FUNCTION__ << " " << state;
  app_list_view_state_ = state;

  for (auto& observer : observers_)
    observer.OnViewStateChanged(state);

  if (state == AppListViewState::kClosed)
    ScheduleCloseAssistant();
}

void AppListControllerImpl::ScheduleCloseAssistant() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  // Close the Assistant in asynchronous way if the app list is going to be
  // closed while the Assistant is visible. If the app list close animation is
  // not reversed, `close_assistant_ui_runner_` runs at the end of the animation
  // to actually close the Assistant.
  const bool is_assistant_ui_visible =
      (AssistantUiController::Get()->GetModel()->visibility() ==
       AssistantVisibility::kVisible);
  if (is_assistant_ui_visible) {
    std::optional<base::ScopedClosureRunner> runner =
        AssistantUiController::Get()->CloseUi(
            AssistantExitPoint::kLauncherClose);
    DCHECK(runner);
    DCHECK(!close_assistant_ui_runner_);
    close_assistant_ui_runner_.ReplaceClosure(runner->Release());
  }
}

void AppListControllerImpl::LoadIcon(const std::string& app_id) {
  if (client_)
    client_->LoadIcon(profile_id_, app_id);
}

bool AppListControllerImpl::HasValidProfile() const {
  return profile_id_ != kAppListInvalidProfileID;
}

bool AppListControllerImpl::ShouldHideContinueSection() const {
  PrefService* prefs = GetLastActiveUserPrefService();
  return prefs->GetBoolean(prefs::kLauncherContinueSectionHidden);
}

void AppListControllerImpl::SetHideContinueSection(bool hide) {
  PrefService* prefs = GetLastActiveUserPrefService();
  bool is_hidden = prefs->GetBoolean(prefs::kLauncherContinueSectionHidden);
  if (hide == is_hidden)
    return;
  prefs->SetBoolean(prefs::kLauncherContinueSectionHidden, hide);
  fullscreen_presenter_->UpdateContinueSectionVisibility();
  bubble_presenter_->UpdateContinueSectionVisibility();
}

bool AppListControllerImpl::IsCategoryEnabled(
    AppListSearchControlCategory category) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetDict(prefs::kLauncherSearchCategoryControlStatus)
      .FindBool(GetAppListControlCategoryName(category))
      .value_or(true);
}

void AppListControllerImpl::SetCategoryEnabled(
    AppListSearchControlCategory category,
    bool enabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  ScopedDictPrefUpdate pref_update(prefs,
                                   prefs::kLauncherSearchCategoryControlStatus);
  pref_update->Set(GetAppListControlCategoryName(category), enabled);
}

void AppListControllerImpl::RecordAppsDefaultVisibility(
    const std::vector<std::string>& apps_above_the_fold,
    const std::vector<std::string>& apps_below_the_fold,
    bool is_apps_collections_page) {
  if (client_) {
    client_->RecordAppsDefaultVisibility(
        apps_above_the_fold, apps_below_the_fold, is_apps_collections_page);
  }
}

void AppListControllerImpl::GetAppLaunchedMetricParams(
    AppLaunchedMetricParams* metric_params) {
  metric_params->app_list_view_state = GetAppListViewState();
  metric_params->is_tablet_mode = IsInTabletMode();
  metric_params->app_list_shown = last_visible_;
  metric_params->launcher_show_timestamp = last_show_timestamp_;
}

gfx::Rect AppListControllerImpl::SnapBoundsToDisplayEdge(
    const gfx::Rect& bounds) {
  AppListView* app_list_view = fullscreen_presenter_->GetView();
  DCHECK(app_list_view && app_list_view->GetWidget());
  aura::Window* window = app_list_view->GetWidget()->GetNativeView();
  return screen_util::SnapBoundsToDisplayEdge(bounds, window);
}

AppListState AppListControllerImpl::GetCurrentAppListPage() const {
  return app_list_page_;
}

void AppListControllerImpl::OnAppListPageChanged(AppListState page) {
  const AppListState old_page = app_list_page_;
  if (old_page == page)
    return;

  app_list_page_ = page;

  if (!fullscreen_presenter_)
    return;

  UpdateFullscreenLauncherContainer();

  if (page == AppListState::kStateEmbeddedAssistant) {
    // ShowUi() will be no-op if the Assistant UI is already visible.
    AssistantUiController::Get()->ShowUi(AssistantEntryPoint::kUnspecified);
    return;
  }

  if (old_page == AppListState::kStateEmbeddedAssistant) {
    // CloseUi() will be no-op if the Assistant UI is already closed.
    AssistantUiController::Get()->CloseUi(AssistantExitPoint::kBackInLauncher);
  }
}

int AppListControllerImpl::GetShelfSize() {
  return ShelfConfig::Get()->GetSystemShelfSizeInTabletMode();
}

int AppListControllerImpl::GetSystemShelfInsetsInTabletMode() {
  return ShelfConfig::Get()->GetTabletModeShelfInsetsAndRecordUMA();
}

bool AppListControllerImpl::IsInTabletMode() const {
  return display::Screen::GetScreen()->InTabletMode();
}

void AppListControllerImpl::RecordAppLaunched(
    AppListLaunchedFrom launched_from) {
  RecordAppListAppLaunched(launched_from, GetAppListViewState(),
                           IsInTabletMode(), last_visible_);
}

void AppListControllerImpl::AddObserver(AppListControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListControllerImpl::RemoveObserver(
    AppListControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListControllerImpl::OnVisibilityChanged(bool visible,
                                                int64_t display_id) {
  // In the Kiosk session we should never show the app list.
  CHECK(!visible || !IsKioskSession());

  if (client_) {
    client_->RecalculateWouldTriggerLauncherSearchIph();
  }

  DVLOG(1) << __PRETTY_FUNCTION__ << " visible " << visible << " display_id "
           << display_id;
  // Focus and app visibility changes while finishing home launcher state
  // animation may cause OnVisibilityChanged() to be called before the home
  // launcher state transition finished - delay the visibility change until
  // the home launcher stops animating, so observers do not miss the animation
  // state update.
  if (home_launcher_transition_state_ !=
      HomeLauncherTransitionState::kFinished) {
    OnVisibilityWillChange(visible, display_id);
    return;
  }

  bool real_visibility = visible;
  // HomeLauncher is only visible when no other app windows are visible,
  // unless we are in the process of animating to (or dragging) the home
  // launcher.
  if (IsInTabletMode()) {
    UpdateTrackedAppWindow();

    if (tracked_app_window_)
      real_visibility = false;

    // When transitioning to/from overview, ensure the AppList window is not in
    // the process of being hidden.
    aura::Window* app_list_window = GetWindow();
    real_visibility &= app_list_window && app_list_window->TargetVisibility();
  }

  OnVisibilityWillChange(real_visibility, display_id);

  // Skip adjacent same changes.
  if (last_visible_ == real_visibility &&
      last_visible_display_id_ == display_id) {
    return;
  }

  last_visible_display_id_ = display_id;

  if (visible)
    last_show_timestamp_ = base::TimeTicks::Now();

  AppListView* const app_list_view = fullscreen_presenter_->GetView();
  if (app_list_view) {
    app_list_view->UpdatePageResetTimer(real_visibility);

    if (!real_visibility) {
      app_list_view->search_box_view()->ClearSearchAndDeactivateSearchBox();
      // Reset the app list contents state, so the app list is in initial state
      // when the app list visibility changes again.
      app_list_view->app_list_main_view()->contents_view()->ResetForShow();
    }
  }

  // Notify chrome of visibility changes.
  if (last_visible_ != real_visibility) {
    // When showing the launcher with the virtual keyboard enabled, one
    // feature called "transient blur" (which means that if focus was lost but
    // regained a few seconds later, we would show the virtual keyboard again)
    // may show the virtual keyboard, which is not what we want. So hide the
    // virtual keyboard explicitly when the launcher shows.
    if (real_visibility)
      keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();

    if (client_)
      client_->OnAppListVisibilityChanged(real_visibility);

    last_visible_ = real_visibility;

    // Updates AppsContainerView in `fullscreen_presenter_`.
    if (app_list_view)
      app_list_view->OnAppListVisibilityChanged(real_visibility);

    for (auto& observer : observers_)
      observer.OnAppListVisibilityChanged(real_visibility, display_id);

    // Record whether the continue section is hidden by the user.
    if (real_visibility)
      RecordHideContinueSectionMetric();

    if (!home_launcher_animation_callback_.is_null())
      home_launcher_animation_callback_.Run(real_visibility);
  }
}

void AppListControllerImpl::OnWindowVisibilityChanging(aura::Window* window,
                                                       bool visible) {
  if (visible || window != tracked_app_window_)
    return;

  UpdateTrackedAppWindow();

  if (!tracked_app_window_ && ShouldHomeLauncherBeVisible())
    OnVisibilityChanged(true, last_visible_display_id_);
}

void AppListControllerImpl::OnWindowDestroyed(aura::Window* window) {
  if (window != tracked_app_window_)
    return;

  tracked_app_window_ = nullptr;
}

void AppListControllerImpl::OnVisibilityWillChange(bool visible,
                                                   int64_t display_id) {
  // In the Kiosk session we should never show the app list.
  CHECK(!visible || !IsKioskSession());

  bool real_target_visibility = visible;
  // HomeLauncher is only visible when no other app windows are visible,
  // unless we are in the process of animating to (or dragging) the home
  // launcher.
  if (IsInTabletMode() && home_launcher_transition_state_ ==
                              HomeLauncherTransitionState::kFinished) {
    real_target_visibility &= !GetTopVisibleWindow();
  }

  // Skip adjacent same changes.
  if (last_target_visible_ == real_target_visibility &&
      last_target_visible_display_id_ == display_id) {
    return;
  }

  // Notify chrome of target visibility changes.
  if (last_target_visible_ != real_target_visibility) {
    last_target_visible_ = real_target_visibility;
    last_target_visible_display_id_ = display_id;

    if (real_target_visibility && fullscreen_presenter_->GetView())
      fullscreen_presenter_->SetViewVisibility(true);

    if (client_)
      client_->OnAppListVisibilityWillChange(real_target_visibility);

    for (auto& observer : observers_) {
      observer.OnAppListVisibilityWillChange(real_target_visibility,
                                             display_id);
    }

    // The virtual keyboard should be hidden before the bubble launcher
    // calculating the work area.
    if (real_target_visibility) {
      keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Private used only:

AppListModel* AppListControllerImpl::GetModel() {
  return model_provider_->model();
}

SearchModel* AppListControllerImpl::GetSearchModel() {
  return model_provider_->search_model();
}

void AppListControllerImpl::UpdateSearchBoxUiVisibilities() {
  GetSearchModel()->search_box()->SetShowAssistantButton(
      IsAssistantAllowedAndEnabled());

  if (!client_) {
    return;
  }

  client_->RecalculateWouldTriggerLauncherSearchIph();
}

int64_t AppListControllerImpl::GetDisplayIdToShowAppListOn() {
  if (IsInTabletMode() && !Shell::Get()->display_manager()->IsInUnifiedMode()) {
    return display::HasInternalDisplay()
               ? display::Display::InternalDisplayId()
               : display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(Shell::GetRootWindowForNewWindows())
      .id();
}

void AppListControllerImpl::ResetHomeLauncherIfShown() {
  if (!IsInTabletMode() || !fullscreen_presenter_->IsVisibleDeprecated()) {
    return;
  }

  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible())
    keyboard_controller->HideKeyboardByUser();
  fullscreen_presenter_->GetView()->CloseOpenedPage();

  // Refresh the suggestion chips with empty query.
  // TODO(crbug.com/40204937): Switch to client_->StartZeroStateSearch()?
  StartSearch(std::u16string());
}

void AppListControllerImpl::ShowHomeScreen(AppListShowSource show_source) {
  DCHECK(IsInTabletMode());

  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() ||
      IsKioskSession())
    return;

  // App list is only considered shown for metrics if there are currently no
  // other visible windows shown over the app list after the tablet
  // transition.
  bool should_record_metrics = !GetTopVisibleWindow();

  Show(GetDisplayIdToShowAppListOn(), show_source, base::TimeTicks(),
       should_record_metrics);
  UpdateHomeScreenVisibility();

  aura::Window* window = GetHomeScreenWindow();
  if (window)
    Shelf::ForWindow(window)->MaybeUpdateShelfBackground();
  last_open_source_ = show_source;
}

void AppListControllerImpl::UpdateHomeScreenVisibility() {
  if (!IsInTabletMode()) {
    return;
  }

  aura::Window* window = GetHomeScreenWindow();
  if (!window)
    return;

  if (ShouldShowHomeScreen())
    window->Show();
  else
    window->Hide();
}

bool AppListControllerImpl::ShouldShowHomeScreen() const {
  if (IsKioskSession() || in_window_dragging_ || in_wallpaper_preview_)
    return false;

  aura::Window* window = GetHomeScreenWindow();
  if (!window)
    return false;

  if (!IsInTabletMode()) {
    return false;
  }
  if (Shell::Get()->overview_controller()->InOverviewSession()) {
    return false;
  }

  return !SplitViewController::Get(window)->InSplitViewMode();
}

void AppListControllerImpl::UpdateForOverviewModeChange(bool show_home_launcher,
                                                        bool animate) {
  // Force the home view into the expected initial state without animation,
  // except when transitioning out from home launcher. Gesture handling for
  // the gesture to move to overview can update the scale before triggering
  // transition to overview - undoing these changes here would make the UI
  // jump during the transition.
  if (animate && show_home_launcher) {
    UpdateScaleAndOpacityForHomeLauncher(kOverviewFadeAnimationScale,
                                         /*opacity=*/0.0f,
                                         /*animation_info=*/std::nullopt,
                                         /*callback=*/base::NullCallback());
  }

  // Hide all transient child windows in the app list (e.g. uninstall dialog)
  // before starting the overview mode transition, and restore them when
  // reshowing the app list.
  aura::Window* app_list_window =
      Shell::Get()->app_list_controller()->GetHomeScreenWindow();
  if (app_list_window) {
    for (aura::Window* child : wm::GetTransientChildren(app_list_window)) {
      if (show_home_launcher)
        child->Show();
      else
        child->Hide();
    }
  }

  std::optional<HomeLauncherAnimationInfo> animation_info =
      animate ? std::make_optional<HomeLauncherAnimationInfo>(
                    HomeLauncherAnimationTrigger::kOverviewModeFade,
                    show_home_launcher)
              : std::nullopt;
  UpdateAnimationSettingsCallback animation_settings_updater =
      animate ? base::BindRepeating(&UpdateOverviewSettings,
                                    kOverviewFadeAnimationDuration)
              : base::NullCallback();
  const float target_scale =
      show_home_launcher ? 1.0f : kOverviewFadeAnimationScale;
  const float target_opacity = show_home_launcher ? 1.0f : 0.0f;
  UpdateScaleAndOpacityForHomeLauncher(target_scale, target_opacity,
                                       std::move(animation_info),
                                       animation_settings_updater);
}

void AppListControllerImpl::UpdateFullscreenLauncherContainer(
    std::optional<int64_t> display_id) {
  aura::Window* window = fullscreen_presenter_->GetWindow();
  if (!window)
    return;

  aura::Window* parent_window =
      GetFullscreenLauncherContainerForDisplayId(display_id);
  if (parent_window && !parent_window->Contains(window)) {
    parent_window->AddChild(window);
    // Release focus if the launcher is moving behind apps, and there is app
    // window showing. Note that the app list can be shown behind apps in
    // tablet mode only.
    if (IsInTabletMode() && !ShouldHomeLauncherBeVisible()) {
      WindowState* const window_state = WindowState::Get(window);
      if (window_state->IsActive())
        window_state->Deactivate();
    }
  }
}

aura::Window* AppListControllerImpl::GetFullscreenLauncherContainerForDisplayId(
    std::optional<int64_t> display_id) {
  aura::Window* root_window = nullptr;
  if (display_id.has_value()) {
    root_window = Shell::GetRootWindowForDisplayId(display_id.value());
  } else if (fullscreen_presenter_->GetWindow()) {
    root_window = fullscreen_presenter_->GetWindow()->GetRootWindow();
  }

  return root_window
             ? root_window->GetChildById(GetFullscreenLauncherContainerId())
             : nullptr;
}

void AppListControllerImpl::Shutdown() {
  DCHECK(!is_shutdown_);
  is_shutdown_ = true;

  // Cancel any pending assistant UI close requests to avoid attempts to update
  // assistant UI state mid shutdown (possibly after assistant has started
  // shutting down).
  IgnoreResult(close_assistant_ui_runner_.Release());

  // Always shutdown the bubble presenter.
  bubble_presenter_->Shutdown();

  Shell* shell = Shell::Get();
  AssistantController::Get()->RemoveObserver(this);
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  shell->display_manager()->RemoveDisplayManagerObserver(this);
  AssistantState::Get()->RemoveObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  shell->overview_controller()->RemoveObserver(this);
  shell->RemoveShellObserver(this);
  WallpaperController::Get()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
  FeatureDiscoveryDurationReporter::GetInstance()->RemoveObserver(this);

  badge_controller_->Shutdown();
}

bool AppListControllerImpl::IsHomeScreenVisible() {
  return IsInTabletMode() && IsVisible();
}

void AppListControllerImpl::OnWindowDragStarted() {
  in_window_dragging_ = true;
  UpdateHomeScreenVisibility();

  // Dismiss Assistant if it's running when a window drag starts.
  if (fullscreen_presenter_->IsShowingEmbeddedAssistantUI())
    fullscreen_presenter_->ShowEmbeddedAssistantUI(false);
}

void AppListControllerImpl::OnWindowDragEnded(bool animate) {
  in_window_dragging_ = false;
  UpdateHomeScreenVisibility();
  if (ShouldShowHomeScreen())
    UpdateForOverviewModeChange(/*show_home_launcher=*/true, animate);
}

void AppListControllerImpl::UpdateTrackedAppWindow() {
  // Do not want to observe new windows or further update
  // |tracked_app_window_| once Shutdown() has been called.
  aura::Window* top_window = !is_shutdown_ ? GetTopVisibleWindow() : nullptr;
  if (tracked_app_window_ == top_window)
    return;

  if (tracked_app_window_)
    tracked_app_window_->RemoveObserver(this);
  tracked_app_window_ = top_window;
  if (tracked_app_window_)
    tracked_app_window_->AddObserver(this);
}

void AppListControllerImpl::RecordAppListState() {
  recorded_app_list_view_state_ = GetAppListViewState();
  recorded_app_list_visibility_ = last_visible_;
}

void AppListControllerImpl::StartTrackingAnimationSmoothness(
    int64_t display_id) {
  auto* root_window = Shell::GetRootWindowForDisplayId(display_id);
  auto* compositor = root_window->layer()->GetCompositor();
  smoothness_tracker_ = compositor->RequestNewThroughputTracker();
  smoothness_tracker_->Start(
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
        UMA_HISTOGRAM_PERCENTAGE(kHomescreenAnimationHistogram, smoothness);
      })));
}

void AppListControllerImpl::RecordAnimationSmoothness() {
  if (!smoothness_tracker_)
    return;
  smoothness_tracker_->Stop();
  smoothness_tracker_.reset();
}

void AppListControllerImpl::OnGoHomeWindowAnimationsEnded(int64_t display_id) {
  RecordAnimationSmoothness();
  OnHomeLauncherAnimationComplete(/*shown=*/true, display_id);
}

void AppListControllerImpl::OnReporterActivated() {
  FeatureDiscoveryDurationReporter::GetInstance()->MaybeActivateObservation(
      feature_discovery::TrackableFeature::
          kAppListReorderAfterSessionActivation);
}

int AppListControllerImpl::GetFullscreenLauncherContainerId() const {
  const bool should_show_behind_apps =
      app_list_page_ != AppListState::kStateEmbeddedAssistant;
  return should_show_behind_apps ? kShellWindowId_HomeScreenContainer
                                 : kShellWindowId_AppListContainer;
}

int AppListControllerImpl::GetPreferredBubbleWidth(
    aura::Window* root_window) const {
  DCHECK(bubble_presenter_);

  return bubble_presenter_->GetPreferredBubbleWidth(root_window);
}

bool AppListControllerImpl::SetHomeButtonQuickApp(const std::string& app_id) {
  if (!features::IsHomeButtonQuickAppAccessEnabled()) {
    return false;
  }
  return model_provider_->quick_app_access_model()->SetQuickApp(app_id);
}

}  // namespace ash
