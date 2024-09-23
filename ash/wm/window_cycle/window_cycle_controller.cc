// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/metrics/task_switch_metrics_recorder.h"
#include "ash/metrics/task_switch_source.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_cycle/window_cycle_event_filter.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kAltTabDesksSwitchDistanceHistogramName[] =
    "Ash.WindowCycleController.DesksSwitchDistance";
constexpr char kAltTabInitialModeHistogramName[] =
    "Ash.WindowCycleController.InitialMode";
constexpr char kAltTabItemsHistogramName[] = "Ash.WindowCycleController.Items";
constexpr char kAltTabSwitchModeHistogramName[] =
    "Ash.WindowCycleController.SwitchMode";
constexpr char kAltTabModeSwitchSourceHistogramName[] =
    "Ash.WindowCycleController.ModeSwitchSource";
constexpr char kSameAppWindowCycleIsSameAppHistogramName[] =
    "Ash.WindowCycleController.SameApp.IsSameApp";
constexpr char kSameAppWindowCycleDeskModeHistogramName[] =
    "Ash.WindowCycleController.SameApp.DeskMode";

// Enumeration of the alt-tab modes to record initial mode and mode switch.
// Note that these values are persisted to histograms so existing values should
// remain unchanged and new values should be added to the end.
enum class AltTabMode {
  // The window list includes all windows from all desks.
  kAllDesks,
  // The window list only includes windows from the active desk.
  kCurrentDesk,
  kMaxValue = kCurrentDesk,
};

// Returns the most recently active window from the |window_list| or nullptr
// if the list is empty.
aura::Window* GetActiveWindow(
    const WindowCycleController::WindowList& window_list) {
  return window_list.empty() ? nullptr : window_list[0];
}

void ReportPossibleDesksSwitchStats(int active_desk_container_id_before_cycle) {
  // Report only for users who have 2 or more desks, since we're only interested
  // in seeing how users of Virtual Desks use window cycling.
  auto* desks_controller = DesksController::Get();
  if (!desks_controller)
    return;

  if (desks_controller->desks().size() < 2)
    return;

  // Note that this functions is called while a potential desk switch animation
  // is starting, in this case we want the target active desk (i.e. the soon-to-
  // be active desk after the animation finishes).
  const int active_desk_container_id_after_cycle =
      desks_controller->GetTargetActiveDesk()->container_id();
  DCHECK_NE(active_desk_container_id_before_cycle, kShellWindowId_Invalid);
  DCHECK_NE(active_desk_container_id_after_cycle, kShellWindowId_Invalid);

  // Note that the desks containers IDs are consecutive. See
  // |ash::ShellWindowId|.
  const int desks_switch_distance =
      std::abs(active_desk_container_id_after_cycle -
               active_desk_container_id_before_cycle);
  base::UmaHistogramExactLinear(kAltTabDesksSwitchDistanceHistogramName,
                                desks_switch_distance,
                                desks_util::kDesksUpperLimit);
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

WindowCycleController::~WindowCycleController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
bool WindowCycleController::CanCycle() {
  return !Shell::Get()->session_controller()->IsScreenLocked() &&
         !Shell::IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned() &&
         !window_util::IsAnyWindowDragged() &&
         !Shell::Get()->desks_controller()->AreDesksBeingModified();
}

// static
void WindowCycleController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAltTabPerDesk, DesksMruType::kAllDesks);
}

void WindowCycleController::HandleCycleWindow(WindowCyclingDirection direction,
                                              bool same_app_only) {
  if (!CanCycle())
    return;

  const bool should_start_alt_tab = !IsCycling();
  if (should_start_alt_tab)
    StartCycling(same_app_only);

  Step(direction, /*starting_alt_tab_or_switching_mode=*/should_start_alt_tab);
}

void WindowCycleController::HandleKeyboardNavigation(
    KeyboardNavDirection direction) {
  // If the UI is not shown yet, discard the event.
  if (!CanCycle() || !IsCycling() || !window_cycle_list_->cycle_view() ||
      !IsValidKeyboardNavigation(direction)) {
    return;
  }

  switch (direction) {
    // Pressing the Up arrow key moves the focus from the window cycle list
    // to the tab slider button.
    case KeyboardNavDirection::kUp:
      DCHECK(!IsTabSliderFocused() && IsInteractiveAltTabModeAllowed());
      window_cycle_list_->SetFocusTabSlider(true);
      // Focusing the alt-tab mode button announces the current mode.
      Shell::Get()
          ->accessibility_controller()
          ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringUTF8(
              IsAltTabPerActiveDesk()
                  ? IDS_ASH_ALT_TAB_CURRENT_DESK_MODE_SELECTED_TITLE
                  : IDS_ASH_ALT_TAB_ALL_DESKS_MODE_SELECTED_TITLE));
      break;
    // Pressing the Down arrow key does the opposite of the Up arrow key.
    case KeyboardNavDirection::kDown: {
      DCHECK(IsTabSliderFocused());
      window_cycle_list_->SetFocusTabSlider(false);
      aura::Window* target_window = window_cycle_list_->GetTargetWindow();
      // Cannot press the Down arrow key if there is no window.
      DCHECK(target_window);
      // Announce the selected window in the window cycle list.
      Shell::Get()
          ->accessibility_controller()
          ->TriggerAccessibilityAlertWithMessage(
              l10n_util::GetStringFUTF8(IDS_ASH_ALT_TAB_WINDOW_SELECTED_TITLE,
                                        target_window->GetTitle()));
      break;
    }
    // Pressing the Left or Right arrow keys cycles through the window list
    // or switches alt-tab mode depending on which component is focused.
    case KeyboardNavDirection::kRight:
    case KeyboardNavDirection::kLeft:
      if (!IsTabSliderFocused()) {
        // Cycling through the window list if focusing the window.
        HandleCycleWindow(direction == KeyboardNavDirection::kRight
                              ? WindowCyclingDirection::kForward
                              : WindowCyclingDirection::kBackward);
      } else {
        // Switch the mode if focusing the button. Navigating right triggers
        // the right button corresponding to the active desk mode. On the other
        // hand, navigating left enables the all-desk mode.
        OnModeChanged(direction == KeyboardNavDirection::kRight,
                      ModeSwitchSource::kKeyboard);
      }
      break;
    case KeyboardNavDirection::kInvalid:
    default:
      NOTREACHED();
  }
}

void WindowCycleController::Drag(float delta_x) {
  DCHECK(window_cycle_list_);
  window_cycle_list_->Drag(delta_x);
}

void WindowCycleController::StartFling(float velocity_x) {
  DCHECK(window_cycle_list_);
  window_cycle_list_->StartFling(velocity_x);
}

void WindowCycleController::StartCycling(bool same_app_only) {
  Shell* shell = Shell::Get();

  // Close the wallpaper preview if it is open to prevent visual glitches where
  // the window view item for the preview is transparent
  // (http://crbug.com/895265).
  shell->wallpaper_controller()->MaybeClosePreviewWallpaper();
  shell->event_rewriter_controller()->SetAltDownRemappingEnabled(false);

  // End overview as the window cycle list takes over window switching.
  shell->overview_controller()->EndOverview(
      OverviewEndAction::kStartedWindowCycle);

  // Close all desk bars as the window cycle list takes over window switching.
  if (auto* desk_bar_controller =
          shell->desks_controller()->desk_bar_controller()) {
    desk_bar_controller->CloseAllDeskBars();
  }

  WindowCycleController::WindowList window_list = CreateWindowList();
  SaveCurrentActiveDeskAndWindow(window_list);
  window_cycle_list_ =
      std::make_unique<WindowCycleList>(window_list, same_app_only);
  event_filter_ = std::make_unique<WindowCycleEventFilter>();
  base::UmaHistogramBoolean(kSameAppWindowCycleIsSameAppHistogramName,
                            same_app_only);
  if (!same_app_only) {
    base::RecordAction(base::UserMetricsAction("WindowCycleController_Cycle"));
    base::UmaHistogramCounts100(kAltTabItemsHistogramName, window_list.size());
    if (IsInteractiveAltTabModeAllowed()) {
      // When alt-tab interactive mode is available, report the initial alt-tab
      // mode which indicates the user's preferred mode.
      base::UmaHistogramEnumeration(kAltTabInitialModeHistogramName,
                                    IsAltTabPerActiveDesk()
                                        ? AltTabMode::kCurrentDesk
                                        : AltTabMode::kAllDesks);
    }
  }

  desks_observation_.Observe(DesksController::Get());
}

void WindowCycleController::CompleteCycling() {
  DCHECK(window_cycle_list_);
  window_cycle_list_->set_user_did_accept(true);
  StopCycling();
}

void WindowCycleController::CancelCycling() {
  StopCycling();
}

void WindowCycleController::MaybeResetCycleList() {
  if (!IsCycling())
    return;

  WindowCycleController::WindowList window_list = CreateWindowList();
  SaveCurrentActiveDeskAndWindow(window_list);

  DCHECK(window_cycle_list_);
  window_cycle_list_->ReplaceWindows(window_list);
}

void WindowCycleController::SetFocusedWindow(aura::Window* window) {
  if (!IsCycling())
    return;

  DCHECK(window_cycle_list_);
  window_cycle_list_->SetFocusedWindow(window);
}

bool WindowCycleController::IsEventInCycleView(
    const ui::LocatedEvent* event) const {
  return window_cycle_list_ && window_cycle_list_->IsEventInCycleView(event);
}

aura::Window* WindowCycleController::GetWindowAtPoint(
    const ui::LocatedEvent* event) {
  return window_cycle_list_ ? window_cycle_list_->GetWindowAtPoint(event)
                            : nullptr;
}

bool WindowCycleController::IsEventInTabSliderContainer(
    const ui::LocatedEvent* event) const {
  return window_cycle_list_ &&
         window_cycle_list_->IsEventInTabSliderContainer(event);
}

bool WindowCycleController::IsWindowListVisible() const {
  return window_cycle_list_ && window_cycle_list_->ShouldShowUi();
}

bool WindowCycleController::IsInteractiveAltTabModeAllowed() const {
  return Shell::Get()->desks_controller()->GetNumberOfDesks() > 1;
}

bool WindowCycleController::IsAltTabPerActiveDesk() const {
  return IsInteractiveAltTabModeAllowed() && active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs::kAltTabPerDesk);
}

bool WindowCycleController::IsSwitchingMode() const {
  return IsInteractiveAltTabModeAllowed() && is_switching_mode_;
}

bool WindowCycleController::IsTabSliderFocused() const {
  return IsInteractiveAltTabModeAllowed() &&
         window_cycle_list_->IsTabSliderFocused();
}

void WindowCycleController::OnModeChanged(bool per_desk,
                                          ModeSwitchSource source) {
  DCHECK(IsInteractiveAltTabModeAllowed() && IsCycling());
  // Save to the active user prefs.
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    // Can be null in tests.
    return;
  }
  // Avoid an unnecessary update if any.
  if (per_desk == prefs->GetBoolean(prefs::kAltTabPerDesk))
    return;
  prefs->SetBoolean(prefs::kAltTabPerDesk, per_desk);

  // Report the alt-tab mode the user switches to and the source of switch.
  if (!window_cycle_list_->same_app_only()) {
    base::UmaHistogramEnumeration(
        kAltTabSwitchModeHistogramName,
        per_desk ? AltTabMode::kCurrentDesk : AltTabMode::kAllDesks);
    base::UmaHistogramEnumeration(kAltTabModeSwitchSourceHistogramName, source);
  }

  // Announce the new mode and the updated window selection via ChromeVox.
  aura::Window* target_window = window_cycle_list_->GetTargetWindow();
  const std::string mode_switched_string = l10n_util::GetStringUTF8(
      per_desk ? IDS_ASH_ALT_TAB_CURRENT_DESK_MODE_SELECTED_TITLE
               : IDS_ASH_ALT_TAB_ALL_DESKS_MODE_SELECTED_TITLE);
  // A ChromeVox string announcing the selected window in the window cycle list
  // or no recent items if there's no window in the list.
  const std::string window_selected_string =
      target_window
          ? l10n_util::GetStringFUTF8(IDS_ASH_ALT_TAB_WINDOW_SELECTED_TITLE,
                                      target_window->GetTitle())
          : l10n_util::GetStringUTF8(IDS_ASH_OVERVIEW_NO_RECENT_ITEMS);
  switch (source) {
    case ModeSwitchSource::kClick:
      Shell::Get()
          ->accessibility_controller()
          ->TriggerAccessibilityAlertWithMessage(base::JoinString(
              {mode_switched_string, window_selected_string}, " "));
      // If the user clicks the mode button, remove the focus from it.
      window_cycle_list_->SetFocusTabSlider(false);
      break;
    case ModeSwitchSource::kKeyboard:
      // Additionally, during keyboard navigation, notify that the user can
      // press the Down arrow key to navigate among the cycle windows if the
      // list is not empty.
      Shell::Get()
          ->accessibility_controller()
          ->TriggerAccessibilityAlertWithMessage(base::JoinString(
              {mode_switched_string, window_selected_string,
               target_window ? l10n_util::GetStringUTF8(
                                   IDS_ASH_ALT_TAB_FOCUS_WINDOW_LIST_TITLE)
                             : std::string()},
              " "));
      break;
    default:
      NOTREACHED();
  }
}

void WindowCycleController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void WindowCycleController::OnDeskAdded(const Desk* desk, bool from_undo) {
  CancelCycling();
}

void WindowCycleController::OnDeskRemoved(const Desk* desk) {
  CancelCycling();
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

WindowCycleController::WindowList WindowCycleController::CreateWindowList() {
  WindowList window_list = BuildWindowListForWindowCycling(
      IsAltTabPerActiveDesk() ? kActiveDesk : kAllDesks);

  // Window cycle list windows will handle showing their transient related
  // windows, so if a window in |window_list| has a transient root also in
  // |window_list|, we can remove it as the transient root will handle showing
  // the window.
  window_util::EnsureTransientRoots(&window_list);
  return window_list;
}

MruWindowTracker::WindowList
WindowCycleController::BuildWindowListForWindowCycling(
    DesksMruType desks_mru_type) {
  const auto window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleWithPipList(
          desks_mru_type);

  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  if (!snap_group_controller) {
    return window_list;
  }

  MruWindowTracker::WindowList adjusted_window_list;
  for (aura::Window* window : window_list) {
    // The latter-activated window in a snap group should have been added. Skip
    // inserting to avoid duplicates.
    if (base::Contains(adjusted_window_list, window)) {
      continue;
    }

    if (SnapGroup* snap_group =
            snap_group_controller->GetSnapGroupForGivenWindow(window)) {
      // Insert the windows if they belong to a group following the order of the
      // actual window layout, i.e. primary snapped window comes first followed
      // by the secondary snapped window.
      adjusted_window_list.push_back(
          snap_group->GetPhysicallyLeftOrTopWindow());
      adjusted_window_list.push_back(
          snap_group->GetPhysicallyRightOrBottomWindow());
    } else {
      adjusted_window_list.push_back(window);
    }
  }

  return adjusted_window_list;
}

void WindowCycleController::SaveCurrentActiveDeskAndWindow(
    const WindowCycleController::WindowList& window_list) {
  active_desk_container_id_before_cycle_ =
      desks_util::GetActiveDeskContainerId();
  active_window_before_window_cycle_ = GetActiveWindow(window_list);
}

void WindowCycleController::Step(WindowCyclingDirection direction,
                                 bool starting_alt_tab_or_switching_mode) {
  DCHECK(window_cycle_list_);
  window_cycle_list_->Step(direction, starting_alt_tab_or_switching_mode);
}

void WindowCycleController::StopCycling() {
  // There's an edge case where `StopCycling()` is already triggered via an alt
  // release event, but user doesn't release the tap on the
  // `window_cycle_list_`. If we reset `window_cycle_list_` first,
  // `WindowEventDispatcher::DispatchSyntheticTouchEvent` will be triggered
  // because of the availability changed for the `window_cycle_list_`. Thus
  // `event_filter_` will still receive the event and try to handle the event
  // even though it's in the process of stopping cycling. To avoid this, we
  // should remove our event filter first. Please check
  // https://crbug.com/1228381 for more details.
  event_filter_.reset();

  desks_observation_.Reset();
  const bool was_same_app_only = window_cycle_list_->same_app_only();
  window_cycle_list_.reset();

  // We can't use the MRU window list here to get the active window, since
  // cycling can activate a window on a different desk, leading to a desk-switch
  // animation launching. Getting the MRU window list for the active desk now
  // will always be for the current active desk, not the target active desk.
  aura::Window* active_window_after_window_cycle =
      window_util::GetActiveWindow();

  if (was_same_app_only) {
    base::UmaHistogramEnumeration(kSameAppWindowCycleDeskModeHistogramName,
                                  IsAltTabPerActiveDesk()
                                      ? AltTabMode::kCurrentDesk
                                      : AltTabMode::kAllDesks);
  } else if (active_window_after_window_cycle != nullptr &&
             active_window_before_window_cycle_ !=
                 active_window_after_window_cycle) {
    Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
        TaskSwitchSource::WINDOW_CYCLE_CONTROLLER);

    ReportPossibleDesksSwitchStats(active_desk_container_id_before_cycle_);
  }

  active_window_before_window_cycle_ = nullptr;
  active_desk_container_id_before_cycle_ = kShellWindowId_Invalid;
  Shell::Get()->event_rewriter_controller()->SetAltDownRemappingEnabled(true);
}

void WindowCycleController::InitFromUserPrefs() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kAltTabPerDesk,
      base::BindRepeating(&WindowCycleController::OnAltTabModePrefChanged,
                          base::Unretained(this)));

  OnAltTabModePrefChanged();
}

void WindowCycleController::OnAltTabModePrefChanged() {
  // Only update UI for alt-tab mode if the user is using alt-tab with the
  // interactive alt-tab mode supported.
  if (!IsInteractiveAltTabModeAllowed() || !IsCycling())
    return;

  is_switching_mode_ = true;

  // Update the window cycle list.
  MaybeResetCycleList();

  // After the cycle is reset, imitate the same forward cycling behavior as
  // starting alt-tab with `Step()`, which makes sure the correct window is
  // selected and focused.
  Step(WindowCyclingDirection::kForward,
       /*starting_alt_tab_or_switching_mode=*/true);

  // Update tab slider button UI.
  window_cycle_list_->OnModePrefsChanged();

  is_switching_mode_ = false;
}

bool WindowCycleController::IsValidKeyboardNavigation(
    KeyboardNavDirection direction) const {
  // Only allow Left and Right arrow keys if interactive alt-tab mode is not
  // in use.
  if (!IsInteractiveAltTabModeAllowed()) {
    return direction == KeyboardNavDirection::kLeft ||
           direction == KeyboardNavDirection::kRight;
  }

  // If the focus is on the window cycle list, the user can navigate up to
  // focus the mode buttons, or left and right to change the window selection.
  if (!IsTabSliderFocused())
    return direction != KeyboardNavDirection::kDown;

  // If the focus is on the tab slider button, the user can navigate down to
  // focus the non-empty list, determined by non-null target window. The user
  // can only navigate left while focusing the right button and vice versa.
  const bool per_desk = IsAltTabPerActiveDesk();
  return (direction == KeyboardNavDirection::kDown &&
          window_cycle_list_->GetTargetWindow()) ||
         (per_desk && direction == KeyboardNavDirection::kLeft) ||
         (!per_desk && direction == KeyboardNavDirection::kRight);
}

}  // namespace ash
