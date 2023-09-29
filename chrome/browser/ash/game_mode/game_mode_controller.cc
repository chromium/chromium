// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/game_mode/game_mode_controller.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "ui/views/widget/widget.h"

namespace game_mode {

using borealis::BorealisWindowManager;

namespace {

constexpr int kRefreshSec = 60;
constexpr int kTimeoutSec = kRefreshSec + 10;

// A GameModeCriteria for ARC windows. This potentially owns a GameModeEnabler
// which is initialized if the task of the window is determined to be a game.
class ArcGameModeCriteria : public GameModeController::GameModeCriteria {
 public:
  // Constructs an instance using the task ID of the window it is associated
  // with.
  ArcGameModeCriteria(
      aura::Window* window,
      const NotifySetGameModeCallback notify_set_game_mode_callback)
      : notify_set_game_mode_callback_(notify_set_game_mode_callback) {
    // For ARC container boards, Game Mode optimizations are not available
    // (b/248972198).
    if (!arc::IsArcVmEnabled())
      return;

    // ARC is only allowed for the primary user.
    auto* profile = ProfileManager::GetPrimaryUserProfile();
    DCHECK(arc::IsArcAllowedForProfile(profile));

    connection_ = ArcAppListPrefs::Get(profile)->app_connection_holder();
    auto* pkg_name = window->GetProperty(ash::kArcPackageNameKey);

    if (!pkg_name || pkg_name->empty()) {
      LOG(ERROR) << "Failed to find package name for the requested task";
      return;
    }

    if (IsKnownGame(*pkg_name)) {
      VLOG(2) << "ARC task package " << pkg_name << " is known game";
      Enable();
      return;
    }

    auto* app_instance =
        ARC_GET_INSTANCE_FOR_METHOD(connection_.get(), GetAppCategory);
    if (!app_instance)
      return;
    VLOG(2) << "Fetch app category of package: " << pkg_name;

    app_instance->GetAppCategory(
        *pkg_name, base::BindOnce(&ArcGameModeCriteria::OnReceiveAppCategory,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  // Checks if an ARC game package is in the known games list. These are apps
  // which should be treated like a game without checking their actual app
  // category. These games may not be classified as games in their manifest, but
  // we want to treat them as games..
  static bool IsKnownGame(const std::string& pkg_name) {
    // This does not have a category set as of v1.18.32 (circa late Sept, 2022).
    if (pkg_name == "com.mojang.minecraftedu")
      return true;

    // Not sure about whether the app already has the correct category, as I do
    // not have access to the APK. As we have no way to alter the list between
    // release milestones, add it just in case.
    if (pkg_name == "com.mojang.minecraftpe")
      return true;

    return false;
  }

  void OnReceiveAppCategory(arc::mojom::AppCategory category) {
    VLOG(2) << "ARC app category is: " << category;
    if (category == arc::mojom::AppCategory::kGame) {
      Enable();
    }
  }

  GameMode mode() const override { return GameMode::ARC; }

 private:
  void Enable() {
    bool signal_resourced = base::FeatureList::IsEnabled(arc::kGameModeFeature);
    enabler_ = std::make_unique<GameModeController::GameModeEnabler>(
        GameMode::ARC, signal_resourced, notify_set_game_mode_callback_);
  }

  raw_ptr<arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>,
          ExperimentalAsh>
      connection_;

  std::unique_ptr<GameModeController::GameModeEnabler> enabler_;
  const NotifySetGameModeCallback notify_set_game_mode_callback_;

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ArcGameModeCriteria> weak_ptr_factory_{this};
};

}  // namespace

GameModeController::GameModeController() {
  if (!ash::Shell::HasInstance())
    return;
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
  // In case a window is already focused when this is constructed.
  OnWindowFocused(focus_client->GetFocusedWindow(), nullptr);
}

GameModeController::~GameModeController() {
  if (ash::Shell::HasInstance())
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
}

GameMode GameModeController::ModeOfWindow(aura::Window* window) {
  if (BorealisWindowManager::IsBorealisWindow(window))
    return GameMode::BOREALIS;

  if (arc::GetWindowTaskId(window))
    return GameMode::ARC;

  return GameMode::OFF;
}

void GameModeController::OnWindowFocused(aura::Window* gained_focus,
                                         aura::Window* lost_focus) {
  auto maybe_keep_focused = std::move(focused_);

  if (!gained_focus)
    return;

  auto* widget = views::Widget::GetTopLevelWidgetForNativeView(gained_focus);
  // |widget| can be nullptr in tests.
  if (!widget)
    return;

  aura::Window* window = widget->GetNativeWindow();
  auto* window_state = ash::WindowState::Get(window);

  if (!window_state)
    return;

  auto mode = ModeOfWindow(window);
  VLOG(4) << "Focused window game mode type: " << static_cast<int>(mode);
  if (mode != GameMode::OFF) {
    focused_ = std::make_unique<WindowTracker>(
        window_state, std::move(maybe_keep_focused),
        base::BindRepeating(
            &GameModeController::NotifySetGameMode,
            // This is safe because the callback is only used by WindowTracker
            // and GameModeCriteria, which are owned by (or transitively owned
            // by) GameModeController. Therefore |this| cannot be stale.
            base::Unretained(this)));
  }
}

GameModeController::WindowTracker::WindowTracker(
    ash::WindowState* window_state,
    std::unique_ptr<WindowTracker> previous_focus,
    NotifySetGameModeCallback notify_set_game_mode_callback)
    : notify_set_game_mode_callback_(notify_set_game_mode_callback) {
  auto* window = window_state->window();
  auto mode = ModeOfWindow(window);

  // Only Borealis mode can retain GameMode state without leaving, since ARC
  // needs to fetch information after creating the GameModeCriteria instance.
  if (previous_focus && mode == GameMode::BOREALIS) {
    auto previous_criteria = std::move(previous_focus->game_mode_criteria_);
    if (previous_criteria && previous_criteria->mode() == mode)
      game_mode_criteria_ = std::move(previous_criteria);
  }

  UpdateGameModeStatus(window_state);
  window_state_observer_.Observe(window_state);
  window_observer_.Observe(window);
}

GameModeController::WindowTracker::~WindowTracker() {}

void GameModeController::WindowTracker::OnPostWindowStateTypeChange(
    ash::WindowState* window_state,
    chromeos::WindowStateType old_type) {
  UpdateGameModeStatus(window_state);
}

GameMode GameModeController::GameModeEnabler::mode() const {
  return mode_;
}

void GameModeController::WindowTracker::UpdateGameModeStatus(
    ash::WindowState* window_state) {
  auto* window = window_state->window();
  auto mode = ModeOfWindow(window);

  if (!window_state->IsFullscreen() || mode == GameMode::OFF) {
    game_mode_criteria_.reset();
    return;
  }

  if (game_mode_criteria_) {
    // No need to create a new criteria. The existing one is already valid for
    // this window.
    return;
  }

  VLOG(2) << "Initializing GameModeCriteria for mode: "
          << static_cast<int>(mode);

  if (mode == GameMode::BOREALIS) {
    // Borealis has no further criteria than the window being fullscreen and
    // focused, already guaranteed by WindowTracker existing.
    game_mode_criteria_ = std::make_unique<GameModeEnabler>(
        GameMode::BOREALIS, /*signal_resourced=*/true,
        notify_set_game_mode_callback_);
  } else if (mode == GameMode::ARC) {
    game_mode_criteria_ = std::make_unique<ArcGameModeCriteria>(
        window, notify_set_game_mode_callback_);
  } else {
    LOG(DFATAL) << "Unknown GameMode: " << static_cast<int>(mode);
  }
}

void GameModeController::WindowTracker::OnWindowDestroying(
    aura::Window* window) {
  window_state_observer_.Reset();
  window_observer_.Reset();
  game_mode_criteria_.reset();
}

bool GameModeController::GameModeEnabler::should_record_failure;

GameModeController::GameModeEnabler::GameModeEnabler(
    GameMode mode,
    bool signal_resourced,
    NotifySetGameModeCallback notify_set_game_mode_callback)
    : mode_(mode),
      signal_resourced_(signal_resourced),
      notify_set_game_mode_callback_(notify_set_game_mode_callback) {
  DCHECK(mode != GameMode::OFF);

  notify_set_game_mode_callback_.Run(mode_);

  if (!signal_resourced)
    return;

  GameModeEnabler::should_record_failure = true;
  base::UmaHistogramEnumeration(GameModeResultHistogramName(mode),
                                GameModeResult::kAttempted);
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        mode_, kTimeoutSec,
        base::BindOnce(&GameModeEnabler::OnSetGameMode,
                       /*refresh_of=*/absl::nullopt));
  }
  timer_.Start(FROM_HERE, base::Seconds(kRefreshSec), this,
               &GameModeEnabler::RefreshGameMode);
}

GameModeController::GameModeEnabler::~GameModeEnabler() {
  auto time_in_mode = began_.Elapsed();
  base::UmaHistogramLongTimes100(TimeInGameModeHistogramName(mode_),
                                 time_in_mode);

  notify_set_game_mode_callback_.Run(GameMode::OFF);

  if (!signal_resourced_)
    return;

  timer_.Stop();
  VLOG(1) << "Turning off game mode type: " << static_cast<int>(mode_);
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        GameMode::OFF, 0,
        base::BindOnce(&GameModeEnabler::OnSetGameMode, /*refresh_of=*/mode_));
  }
}

void GameModeController::GameModeEnabler::RefreshGameMode() {
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        mode_, kTimeoutSec,
        base::BindOnce(&GameModeEnabler::OnSetGameMode, /*refresh_of=*/mode_));
  }
}

// Previous is whether game mode was enabled previous to this call.
void GameModeController::GameModeEnabler::OnSetGameMode(
    absl::optional<GameMode> refresh_of,
    absl::optional<GameMode> previous) {
  if (!previous.has_value()) {
    LOG(ERROR) << "Failed to set Game Mode";
  } else if (GameModeEnabler::should_record_failure && refresh_of.has_value() &&
             previous.value() != refresh_of.value()) {
    // If game mode was not on and it was not the initial call,
    // it means the previous call failed/timed out.
    base::UmaHistogramEnumeration(GameModeResultHistogramName(*refresh_of),
                                  GameModeResult::kFailed);
    // Only record failures once per entry into gamemode.
    GameModeEnabler::should_record_failure = false;
  }
}

void GameModeController::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void GameModeController::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void GameModeController::NotifySetGameMode(GameMode game_mode) {
  for (auto& obs : observers_) {
    obs.OnSetGameMode(game_mode);
  }
}

}  // namespace game_mode
