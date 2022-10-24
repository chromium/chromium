// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_view.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// A boolean pref indicates whether the bar is set to show or hide through the
// context menu. Showing the bar if this pref is true, hiding otherwise.
constexpr char kBentoBarEnabled[] = "ash.bento_bar.enabled";

// Creates and returns the widget that contains the PersistentDesksBarView. The
// returned widget has no content view yet, and hasn't been shown yet.
std::unique_ptr<views::Widget> CreatePersistentDesksBarWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // Create and show the bar only in the primary display for now. Since it is
  // enough to collect the metrics for the experiment, it can also avoid the bar
  // to consume space from all the displays.
  auto* root_window = Shell::GetPrimaryRootWindow();
  params.parent = root_window->GetChildById(kShellWindowId_ShelfContainer);
  gfx::Rect bounds = display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(root_window)
                         .bounds();
  bounds.set_height(PersistentDesksBarController::kBarHeight);
  params.bounds = bounds;
  params.name = "PersistentDesksBarWidget";

  widget->Init(std::move(params));
  return widget;
}

}  // namespace

PersistentDesksBarController::PersistentDesksBarController() {
  auto* shell = Shell::Get();
  shell->session_controller()->AddObserver(this);
  shell->overview_controller()->AddObserver(this);
  shell->desks_controller()->AddObserver(this);
  shell->tablet_mode_controller()->AddObserver(this);
  shell->AddShellObserver(this);
  shell->app_list_controller()->AddObserver(this);
  shell->accessibility_controller()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

PersistentDesksBarController::~PersistentDesksBarController() {
  display::Screen::GetScreen()->RemoveObserver(this);
  auto* shell = Shell::Get();
  shell->accessibility_controller()->RemoveObserver(this);
  shell->app_list_controller()->RemoveObserver(this);
  shell->RemoveShellObserver(this);
  shell->tablet_mode_controller()->RemoveObserver(this);
  shell->desks_controller()->RemoveObserver(this);
  shell->overview_controller()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
}

// static
void PersistentDesksBarController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kBentoBarEnabled, /*default_value=*/true);
}

// static
bool PersistentDesksBarController::ShouldPersistentDesksBarBeVisible() {
  // Only check whether the feature is overridden from command line if the
  // FeatureList is initialized.
  const base::FeatureList* feature_list = base::FeatureList::GetInstance();
  return (feature_list && feature_list->IsFeatureOverriddenFromCommandLine(
                              features::kBentoBar.name,
                              base::FeatureList::OVERRIDE_ENABLE_FEATURE)) ||
         (features::IsBentoBarEnabled() &&
          desks_restore_util::HasPrimaryUserUsedDesksRecently());
}

void PersistentDesksBarController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state == session_manager::SessionState::ACTIVE)
    MaybeInitBarWidget();
  else
    DestroyBarWidget();
}

void PersistentDesksBarController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  active_user_pref_service_ = prefs;
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  pref_change_registrar_->Add(
      kBentoBarEnabled,
      base::BindRepeating(
          &PersistentDesksBarController::UpdateBarStateOnPrefChanges,
          base::Unretained(this)));
  UpdateBarStateOnPrefChanges();
}

void PersistentDesksBarController::OnOverviewModeWillStart() {
  overview_mode_in_progress_ = true;
  DestroyBarWidget();
}

void PersistentDesksBarController::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  overview_mode_in_progress_ = false;
  if (!canceled)
    MaybeInitBarWidget();
}

void PersistentDesksBarController::OnDeskAdded(const Desk* desk) {
  MaybeInitBarWidget();
}

void PersistentDesksBarController::OnDeskRemoved(const Desk* desk) {
  if (!persistent_desks_bar_widget_)
    return;

  if (DesksController::Get()->desks().size() == 1)
    DestroyBarWidget();
  else
    persistent_desks_bar_view_->RefreshDeskButtons();
}

void PersistentDesksBarController::OnDeskReordered(int old_index,
                                                   int new_index) {
  // Desk reordering is supported in overview mode only. The bar should have
  // been destroyed while entering overview mode.
  DCHECK(!persistent_desks_bar_widget_);
}

void PersistentDesksBarController::OnDeskActivationChanged(
    const Desk* activated,
    const Desk* deactivated) {
  if (!persistent_desks_bar_widget_)
    return;

  persistent_desks_bar_view_->RefreshDeskButtons();
}

void PersistentDesksBarController::OnDeskNameChanged(
    const Desk* desk,
    const std::u16string& new_name) {
  if (persistent_desks_bar_view_)
    persistent_desks_bar_view_->RefreshDeskButtons();
}

void PersistentDesksBarController::OnTabletModeStarted() {
  DestroyBarWidget();
}

void PersistentDesksBarController::OnTabletModeEnded() {
  MaybeInitBarWidget();
}

void PersistentDesksBarController::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  const Shelf* shelf = Shelf::ForWindow(root_window);
  if (shelf->IsHorizontalAlignment())
    MaybeInitBarWidget();
  else
    DestroyBarWidget();
}

void PersistentDesksBarController::OnViewStateChanged(AppListViewState state) {
  if (state == AppListViewState::kFullscreenAllApps ||
      state == AppListViewState::kFullscreenSearch) {
    DestroyBarWidget();
  } else {
    MaybeInitBarWidget();
  }
}

void PersistentDesksBarController::OnAccessibilityStatusChanged() {
  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();
  if (accessibility_controller->spoken_feedback().enabled() ||
      accessibility_controller->docked_magnifier().enabled()) {
    DestroyBarWidget();
  } else {
    MaybeInitBarWidget();
  }
}

void PersistentDesksBarController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Ignore all metrics except for those listed in `filter`.
  const uint32_t filter =
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
      display::DisplayObserver::DISPLAY_METRIC_PRIMARY |
      display::DisplayObserver::DISPLAY_METRIC_ROTATION |
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  if ((filter & changed_metrics) == 0)
    return;

  DestroyBarWidget();
  MaybeInitBarWidget();
}

bool PersistentDesksBarController::IsEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(kBentoBarEnabled);
}

void PersistentDesksBarController::ToggleEnabledState() {
  DCHECK(active_user_pref_service_);
  const bool new_state = !IsEnabled();
  active_user_pref_service_->SetBoolean(kBentoBarEnabled, new_state);
  if (!new_state)
    DestroyBarWidget();

  UMA_HISTOGRAM_BOOLEAN("Ash.Desks.BentoBarEnabled", new_state);
}

void PersistentDesksBarController::MaybeInitBarWidget() {
  if (!ShouldPersistentDesksBarBeCreated())
    return;

  if (!persistent_desks_bar_widget_) {
    DCHECK(!persistent_desks_bar_view_);
    persistent_desks_bar_widget_ = CreatePersistentDesksBarWidget();
    persistent_desks_bar_view_ = persistent_desks_bar_widget_->SetContentsView(
        std::make_unique<PersistentDesksBarView>());
  }
  persistent_desks_bar_view_->RefreshDeskButtons();
  persistent_desks_bar_widget_->Show();

  // Update work area on the persistent desks bar's state. Note, the bar is only
  // created in the primary display.
  WorkAreaInsets::ForWindow(Shell::GetPrimaryRootWindow())
      ->SetPersistentDeskBarHeight(kBarHeight);

  UMA_HISTOGRAM_BOOLEAN("Ash.Desks.BentoBarIsVisible", true);
}

void PersistentDesksBarController::DestroyBarWidget() {
  persistent_desks_bar_widget_.reset();
  persistent_desks_bar_view_ = nullptr;

  // Update work area on the persistent desks bar's state. Note, the bar is only
  // created in the primary display.
  WorkAreaInsets::ForWindow(Shell::GetPrimaryRootWindow())
      ->SetPersistentDeskBarHeight(0);
}

void PersistentDesksBarController::UpdateBarOnWindowStateChanges(
    aura::Window* window) {
  if (window->GetRootWindow() != Shell::GetPrimaryRootWindow())
    return;

  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  if (!base::Contains(windows, window))
    return;

  if (WindowState::Get(window)->IsFullscreen())
    DestroyBarWidget();
  else
    MaybeInitBarWidget();
}

void PersistentDesksBarController::UpdateBarOnWindowDestroying(
    aura::Window* window) {
  if (!WindowState::Get(window)->IsFullscreen() ||
      window->GetRootWindow() != Shell::GetPrimaryRootWindow()) {
    return;
  }
  MaybeInitBarWidget();
}

bool PersistentDesksBarController::ShouldPersistentDesksBarBeCreated() const {
  if (!ShouldPersistentDesksBarBeVisible())
    return false;

  if (!IsEnabled())
    return false;

  Shell* shell = Shell::Get();

  // Do not create the bar in tablet mode, overview mode or if there is
  // only one desk.
  if (TabletMode::Get()->InTabletMode() || overview_mode_in_progress_ ||
      DesksController::Get()->desks().size() == 1) {
    return false;
  }

  // Do not create the bar in non-active user session.
  if (shell->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return false;
  }

  // Do not create the bar if the shelf is not horizontal-aligned.
  if (!Shelf::ForWindow(Shell::GetPrimaryRootWindow())->IsHorizontalAlignment())
    return false;

  // Do not create the bar if the app list is fullscreened.
  AppListControllerImpl* app_list_controller = shell->app_list_controller();
  if (app_list_controller) {
    const AppListViewState state = app_list_controller->GetAppListViewState();
    if (state == AppListViewState::kFullscreenAllApps ||
        state == AppListViewState::kFullscreenSearch) {
      return false;
    }
  }

  // Do not create the bar if ChromeVox or Docked Magnifier is on.
  AccessibilityControllerImpl* accessibility_controller =
      shell->accessibility_controller();
  if (accessibility_controller->spoken_feedback().enabled() ||
      accessibility_controller->docked_magnifier().enabled()) {
    return false;
  }

  // Do not create the bar if any window within the primary display is
  // fullscreened.
  MruWindowTracker::WindowList windows =
      shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (aura::Window* window : windows) {
    if (window->GetRootWindow() != Shell::GetPrimaryRootWindow())
      continue;
    if (WindowState::Get(window)->IsFullscreen())
      return false;
  }

  return true;
}

void PersistentDesksBarController::UpdateBarStateOnPrefChanges() {
  if (IsEnabled())
    MaybeInitBarWidget();
  else
    DestroyBarWidget();
}

}  // namespace ash
