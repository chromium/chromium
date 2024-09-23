// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_controller.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/game_dashboard/game_dashboard_constants.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/game_dashboard/game_dashboard_metrics.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {
// The singleton instance owned by `Shell`.
GameDashboardController* g_instance = nullptr;

// List of known app IDs that are games.
static const std::array<std::string, 7> kGameAppIdAllowList{
    extension_misc::kGeForceNowAppId,   "iicceeckdelepgbcpojbgahbhnklpane",
    "ojjlibnpojmhhabohpkclejfdblglkpj", "hhkmajjdndhdnkbmomodobajdjngeejb",
    "gihmggjjlnjaldngedmnegjmhccccahg", "lbefcdhjbnilmnokeflglbaiaebadckd",
    "bifaabbnnccaenolhjngemgmegdjflkg"};

// List of pending game PWA app IDs that are being merged into
// `kGameAppIdAllowList`.
static const std::array<std::string, 12> kPWAGameAppIdAllowList{
    extension_misc::kAmazonLunaAppIdCA, extension_misc::kAmazonLunaAppIdDE,
    extension_misc::kAmazonLunaAppIdES, extension_misc::kAmazonLunaAppIdFR,
    extension_misc::kAmazonLunaAppIdIT, extension_misc::kAmazonLunaAppIdNL,
    extension_misc::kAmazonLunaAppIdPL, extension_misc::kAmazonLunaAppIdUK,
    extension_misc::kAmazonLunaAppIdUS, extension_misc::kBoosteroidAppId,
    extension_misc::kPokiAppId,         extension_misc::kXboxCloudGamingAppId};

// List of additional game app IDs that are being tested.
static const std::array<std::string, 3> kGamesInTestAppIdAllowList{
    extension_misc::kCoolMathGamesAppId, extension_misc::kNowGGAppIdUK,
    extension_misc::kNowGGAppIdUS};

// Checks whether the given `app_id` is allow listed to show the Game
// Dashboard button.
bool IsAppIdAllowListed(const std::string& app_id) {
  return base::Contains(kGameAppIdAllowList, app_id) ||
         (features::IsGameDashboardGamePWAsEnabled() &&
          base::Contains(kPWAGameAppIdAllowList, app_id)) ||
         (features::IsGameDashboardGamesInTestEnabled() &&
          base::Contains(kGamesInTestAppIdAllowList, app_id));
}
}  // namespace

// static
GameDashboardController* GameDashboardController::Get() {
  return g_instance;
}

// static
bool GameDashboardController::IsGameWindow(aura::Window* window) {
  DCHECK(window);
  return window->GetProperty(chromeos::kIsGameKey);
}

// static
bool GameDashboardController::ReadyForAccelerator(aura::Window* window) {
  return game_dashboard_utils::ShouldEnableFeatures() && IsGameWindow(window) &&
         game_dashboard_utils::ShouldEnableGameDashboardButton(window);
}

// static
void GameDashboardController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGameDashboardShowWelcomeDialog, true);
  registry->RegisterBooleanPref(prefs::kGameDashboardShowToolbar, false);
}

GameDashboardController::GameDashboardController(
    std::unique_ptr<GameDashboardDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
  CHECK(aura::Env::HasInstance());
  env_observation_.Observe(aura::Env::GetInstance());
  CaptureModeController::Get()->AddObserver(this);
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
}

GameDashboardController::~GameDashboardController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
  Shell::Get()->activation_client()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  CaptureModeController::Get()->RemoveObserver(this);
}

std::string GameDashboardController::GetArcAppName(
    const std::string& app_id) const {
  return delegate_->GetArcAppName(app_id);
}

GameDashboardContext* GameDashboardController::GetGameDashboardContext(
    aura::Window* window) const {
  auto it = game_window_contexts_.find(window);
  return it != game_window_contexts_.end() ? it->second.get() : nullptr;
}

void GameDashboardController::MaybeStackAboveWidget(aura::Window* window,
                                                    views::Widget* widget) {
  DCHECK(widget);
  DCHECK(window);

  if (auto* context = GetGameDashboardContext(window)) {
    context->MaybeStackAboveWidget(widget);
  }
}

void GameDashboardController::StartCaptureSession(
    GameDashboardContext* game_context) {
  CHECK(!active_recording_context_);
  auto* game_window = game_context->game_window();
  CHECK(game_window_contexts_.contains(game_window));
  auto* capture_mode_controller = CaptureModeController::Get();
  CHECK(capture_mode_controller->can_start_new_recording());

  active_recording_context_ = game_context;
  capture_mode_controller->StartForGameDashboard(game_window);
}

void GameDashboardController::ShowResizeToggleMenu(aura::Window* window) {
  delegate_->ShowResizeToggleMenu(window);
}

ukm::SourceId GameDashboardController::GetUkmSourceId(
    const std::string& app_id) const {
  return delegate_->GetUkmSourceId(app_id);
}

void GameDashboardController::OnWindowInitialized(aura::Window* new_window) {
  if (const auto* top_level_window = new_window->GetToplevelWindow();
      !top_level_window ||
      top_level_window->GetType() != aura::client::WINDOW_TYPE_NORMAL) {
    // Ignore non-NORMAL window types.
    return;
  }
  GetWindowGameState(new_window);
}

void GameDashboardController::OnWindowPropertyChanged(aura::Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  if (key == kAppIDKey) {
    GetWindowGameState(window);
  } else if (key == kArcGameControlsFlagsKey) {
    RefreshForGameControlsFlags(window);
  } else if (key == kWindowStateKey) {
    MaybeCreateGameDashboardContext(window);
  }
}

void GameDashboardController::OnWindowParentChanged(aura::Window* window,
                                                    aura::Window* parent) {
  if (parent) {
    // When this controller determines that the given `window` is a game, the
    // `window` may not be parented. The controller will not create a
    // `GameDashboardContext`. When the window is reparented to a
    // valid parent, `OnWindowParentChanged` will be called and create a
    // `GameDashboardContext` for it.
    MaybeCreateGameDashboardContext(window);
  }
}

void GameDashboardController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (auto* context = GetGameDashboardContext(window)) {
    context->OnWindowBoundsChanged(reason ==
                                   ui::PropertyChangeReason::FROM_ANIMATION);
  }
}

void GameDashboardController::OnWindowDestroying(aura::Window* window) {
  window_observations_.RemoveObservation(window);
  game_window_contexts_.erase(window);
}

void GameDashboardController::OnWindowTransformed(
    aura::Window* window,
    ui::PropertyChangeReason reason) {
  if (auto* context = GetGameDashboardContext(window);
      context && game_dashboard_utils::ShouldEnableFeatures()) {
    // Enable the features if the window is not minimized or undergoing an
    // animation. Otherwise, disable them.
    const bool enable = (reason == ui::PropertyChangeReason::FROM_ANIMATION) &&
                        !(WindowState::Get(window)->IsMinimized());
    context->EnableFeatures(enable,
                            GameDashboardMainMenuToggleMethod::kAnimation);
  }
}

void GameDashboardController::OnRecordingStarted(aura::Window* current_root) {
  // Update any needed game dashboard UIs if and only if this recording started
  // from a request by a game dashboard entry point.
  for (auto const& [game_window, context] : game_window_contexts_) {
    context->OnRecordingStarted(context.get() == active_recording_context_);
  }
}

void GameDashboardController::OnRecordingEnded() {
  active_recording_context_ = nullptr;
  for (auto const& [game_window, context] : game_window_contexts_) {
    context->OnRecordingEnded();
  }
}

void GameDashboardController::OnVideoFileFinalized(
    bool user_deleted_video_file,
    const gfx::ImageSkia& thumbnail) {
  for (auto const& [game_window, context] : game_window_contexts_) {
    context->OnVideoFileFinalized();
  }
}

void GameDashboardController::OnRecordedWindowChangingRoot(
    aura::Window* new_root) {
  // TODO(phshah): Update any game dashboard UIs that need to change as a result
  // of the recorded window moving to a different display if and only if this
  // recording started from a request by a game dashboard entry point. If
  // nothing needs to change, leave empty.
}

void GameDashboardController::OnRecordingStartAborted() {
  OnRecordingEnded();
}

void GameDashboardController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kInClamshellMode:
      // Cancel the tablet toast if it is still shown.
      Shell::Get()->toast_manager()->Cancel(game_dashboard::kTabletToastId);
      // Enable the Game Dashboard features if the display is not in Overview
      // Mode.
      OverviewController::Get()->InOverviewSession()
          ? MaybeEnableFeatures(/*enable=*/false,
                                GameDashboardMainMenuToggleMethod::kOverview)
          : MaybeEnableFeatures(/*enable=*/true,
                                GameDashboardMainMenuToggleMethod::kTabletMode);
      break;
    case display::TabletState::kEnteringTabletMode: {
      const int toast_text_id =
          active_recording_context_
              ? IDS_ASH_GAME_DASHBOARD_TABLET_STOP_RECORDING_TOAST
              : IDS_ASH_GAME_DASHBOARD_TABLET_TOAST;
      if (active_recording_context_) {
        auto* capture_mode_controller = CaptureModeController::Get();
        CHECK(capture_mode_controller->is_recording_in_progress());
        capture_mode_controller->EndVideoRecording(
            EndRecordingReason::kGameDashboardTabletMode);
      }
      MaybeEnableFeatures(/*enable=*/false,
                          GameDashboardMainMenuToggleMethod::kTabletMode);
      // Show the toast to notify users when there is any game window open.
      if (!game_window_contexts_.empty()) {
        Shell::Get()->toast_manager()->Show(
            ToastData(game_dashboard::kTabletToastId,
                      ToastCatalogName::kGameDashboardEnterTablet,
                      l10n_util::GetStringUTF16(toast_text_id)));
      }
      break;
    }
    case display::TabletState::kInTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
  }
}

void GameDashboardController::OnOverviewModeWillStart() {
  // In overview mode, hide the Game Dashboard button, and if open, close the
  // main menu.
  MaybeEnableFeatures(/*enable=*/false,
                      GameDashboardMainMenuToggleMethod::kOverview);
}

void GameDashboardController::OnOverviewModeEnded() {
  // Make the Game Dashboard button visible.
  MaybeEnableFeatures(/*enable=*/true,
                      GameDashboardMainMenuToggleMethod::kOverview);
}

void GameDashboardController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  GameDashboardContext* lost_active_context =
      GetGameDashboardContext(wm::GetTransientRoot(lost_active));
  GameDashboardContext* gained_active_context =
      GetGameDashboardContext(wm::GetTransientRoot(gained_active));
  if (lost_active_context == gained_active_context) {
    // Ignore if the activation is moving within the same game window.
    return;
  }

  // If `lost_active_context` and `gained_active_context` both exist, the
  // activated widget is moving between Game Dashboard windows. If only
  // `gained_active_context` exists, activation is moving from a non-game window
  // to a Game Dashboard window. If only `lost_active_context` exists,
  // activation is moving from a Game Dashboard window into a non-game window.
  if (gained_active_context) {
    gained_active_context->MaybeAddPreTargetHandler();
  }
  if (lost_active_context) {
    lost_active_context->MaybeRemovePreTargetHandler();
  }
}

void GameDashboardController::MaybeCreateGameDashboardContext(
    aura::Window* window) {
  DCHECK(window);
  // Do not create a GameDashboardContext if the window is not a game, is not
  // parented, doesn't have a WindowState, or is being destroyed.
  if (!IsGameWindow(window) || !window->parent() || !WindowState::Get(window) ||
      window->is_destroying()) {
    return;
  }
  auto& context = game_window_contexts_[window];
  if (!context) {
    context = std::make_unique<GameDashboardContext>(window);
    context->Initialize();
    RefreshForGameControlsFlags(window);
    delegate_->RecordGameWindowOpenedEvent(window);
  }
}

void GameDashboardController::GetWindowGameState(aura::Window* window) {
  if (const auto* app_id = window->GetProperty(kAppIDKey); !app_id) {
    RefreshWindowTracking(window, WindowGameState::kNotYetKnown);
  } else if (IsAppIdAllowListed(*app_id)) {
    RefreshWindowTracking(window, WindowGameState::kGame);
  } else if (IsArcWindow(window)) {
    // For ARC apps, the "app_id" is equivalent to its package name.
    delegate_->GetIsGame(
        *app_id, base::BindOnce(
                     &GameDashboardController::OnArcWindowIsGame,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::make_unique<aura::WindowTracker>(
                         std::vector<raw_ptr<aura::Window, VectorExperimental>>(
                             {window}))));
  } else {
    RefreshWindowTracking(window, WindowGameState::kNotGame);
  }
}

void GameDashboardController::OnArcWindowIsGame(
    std::unique_ptr<aura::WindowTracker> window_tracker,
    bool is_game) {
  if (const auto windows = window_tracker->windows(); !windows.empty()) {
    RefreshWindowTracking(windows[0], is_game ? WindowGameState::kGame
                                              : WindowGameState::kNotGame);
  }
}

void GameDashboardController::RefreshWindowTracking(aura::Window* window,
                                                    WindowGameState state) {
  DCHECK(window);
  const bool is_observing = window_observations_.IsObservingSource(window);
  const bool should_observe = state != WindowGameState::kNotGame;

  if (state != WindowGameState::kNotYetKnown) {
    const bool is_game = state == WindowGameState::kGame;
    const bool prev_is_game_property =
        window->GetProperty(chromeos::kIsGameKey);
    window->SetProperty(chromeos::kIsGameKey, is_game);
    if (is_game) {
      MaybeCreateGameDashboardContext(window);
    } else if (prev_is_game_property) {
      // The window was a game, but NOT anymore. This can happen if the user
      // disables ARC during the existing session.
      game_window_contexts_.erase(window);
    }
  }

  if (is_observing == should_observe) {
    return;
  }

  if (should_observe) {
    window_observations_.AddObservation(window);
  } else {
    window_observations_.RemoveObservation(window);
  }
}

void GameDashboardController::RefreshForGameControlsFlags(
    aura::Window* window) {
  if (!IsArcWindow(window)) {
    return;
  }

  if (auto* context = GetGameDashboardContext(window)) {
    context->UpdateForGameControlsFlags();
  }
}

void GameDashboardController::MaybeEnableFeatures(
    bool enable,
    GameDashboardMainMenuToggleMethod main_menu_toggle_method) {
  const bool should_enable =
      enable && game_dashboard_utils::ShouldEnableFeatures();
  for (auto const& [_, context] : game_window_contexts_) {
    context->OnWindowBoundsChanged(
        main_menu_toggle_method ==
        GameDashboardMainMenuToggleMethod::kAnimation);
    context->EnableFeatures(should_enable, main_menu_toggle_method);
  }
}

}  // namespace ash
