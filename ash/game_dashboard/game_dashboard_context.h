// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

class GameDashboardButton;
class GameDashboardMainMenuView;
class GameDashboardToolbarView;

// This class manages Game Dashboard related UI for a given `aura::Window`, and
// its instance is managed by the `GameDashboardController`.
class ASH_EXPORT GameDashboardContext : public views::ViewObserver,
                                        public views::WidgetObserver {
 public:
  // Indicator for the 4 quadrants that the toolbar is able to be placed.
  enum class ToolbarSnapLocation {
    kTopLeft,
    kTopRight,
    kBottomLeft,
    kBottomRight
  };

  explicit GameDashboardContext(aura::Window* game_window);
  GameDashboardContext(const GameDashboardContext&) = delete;
  GameDashboardContext& operator=(const GameDashboardContext&) = delete;
  ~GameDashboardContext() override;

  aura::Window* game_window() { return game_window_.get(); }

  GameDashboardMainMenuView* main_menu_view() { return main_menu_view_; }

  views::Widget* game_dashboard_button_widget() {
    return game_dashboard_button_widget_.get();
  }

  ToolbarSnapLocation toolbar_snap_location() const {
    return toolbar_snap_location_;
  }

  const std::u16string& GetRecordingDuration() const;

  // Reassigns the new `toolbar_snap_location_` and performs an animation as the
  // toolbar moves to its new location.
  void SetToolbarSnapLocation(ToolbarSnapLocation new_location);

  // Called by `GameDashboardController` when the game window bounds change.
  void OnWindowBoundsChanged();

  // Updates for Game Controls flags.
  void UpdateForGameControlsFlags();

  // Toggles the creation/deletion of the main menu within the game window.
  void ToggleMainMenu();

  // Closes the main menu. Clears `main_menu_widget_` and `main_menu_view_`.
  void CloseMainMenu();

  // Toggles the creation/deletion of the toolbar within the game window.
  // Returns the toolbar visibility state.
  bool ToggleToolbar();

  // Closes the toolbar. Clears `toolbar_widget_` and `toolbar_view_`.
  void CloseToolbar();

  // Conditionally, updates the toolbar widget's bounds and location, relative
  // to the `game_window_`.
  void MaybeUpdateToolbarWidgetBounds();

  bool IsToolbarVisible() const;

  // Called only when `CaptureModeController` has started a recording session.
  // If `is_recording_game_window` is true, then the recording session was
  // initiated by the Game Dashboard and the `game_window_` is being recorded.
  void OnRecordingStarted(bool is_recording_game_window);

  // Called only when `CaptureModeController` has ended a recording session or
  // if the recording session was aborted.
  void OnRecordingEnded();

  // Called when a recorded file has been finalized and fully saved, at which
  // point a new recording is allowed to be started.
  void OnVideoFileFinalized();

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  friend class GameDashboardContextTestApi;

  // Creates a Game Dashboard button widget and adds it as a sibling of the game
  // window.
  void CreateAndAddGameDashboardButtonWidget();

  // Updates the Game Dashboard button widget's bounds and location, relative to
  // the `game_window_`.
  void UpdateGameDashboardButtonWidgetBounds();

  // Called when `GameDashboardButton` is pressed, and toggles the main menu.
  void OnGameDashboardButtonPressed();

  // Shows the Game Dashboard welcome dialog, if it's enabled in the Game
  // Dashboard settings.
  void MaybeShowWelcomeDialog();

  // Updates the Game Dashboard welcome dialog's bounds and location, relative
  // to the `game_window_`.
  void MaybeUpdateWelcomeDialogBounds();

  // Determines the toolbar's physical location on screen based on the
  // `toolbar_snap_location_` value.
  const gfx::Rect CalculateToolbarWidgetBounds();

  // Calculates the height of the app's frame header.
  int GetFrameHeaderHeight() const;

  // Updates the toolbar widget's bounds and location utilizing an animation as
  // it transfers from the previous location.
  void AnimateToolbarWidgetBoundsChange(const gfx::Rect& target_screen_bounds);

  // Repeating timer callback that notifies `main_menu_view_` of the video
  // recording session duration.
  void OnUpdateRecordingTimer();

  // Closes and deletes the Game Dashboard welcome dialog once it's no longer
  // needed.
  void CloseWelcomeDialog();

  // Checks whether the welcome dialog should be displayed when the game window
  // opens.
  bool ShouldShowWelcomeDialog() const;

  const raw_ptr<aura::Window> game_window_;

  // Game Dashboard button widget for the Game Dashboard.
  std::unique_ptr<views::Widget> game_dashboard_button_widget_;

  // Expanded main menu for the Game Dashboard.
  views::UniqueWidgetPtr main_menu_widget_;

  // The toolbar for the Game Dashboard.
  std::unique_ptr<views::Widget> toolbar_widget_;

  // The dialog displayed when the game window first opens.
  std::unique_ptr<views::Widget> welcome_dialog_widget_;

  // The indicator of the current corner that the toolbar is placed.
  ToolbarSnapLocation toolbar_snap_location_;

  // The `GameDashboardButton` view in the `game_dashboard_button_widget_`.
  // Owned by the views hierarchy.
  raw_ptr<GameDashboardButton> game_dashboard_button_ = nullptr;

  // The `GameDashboardMainMenuView` when the user presses the Game Dashboard
  // button.
  // Owned by the views hierarchy.
  raw_ptr<GameDashboardMainMenuView, DanglingUntriaged> main_menu_view_ =
      nullptr;

  // The `GameDashboardToolbarView` when the user makes the toolbar visible.
  // Owned by the views hierarchy.
  raw_ptr<GameDashboardToolbarView> toolbar_view_ = nullptr;

  // A repeating timer to keep track of the recording session duration.
  base::RepeatingTimer recording_timer_;

  // Start time of when `recording_timer_` started.
  base::Time recording_start_time_;

  // Duration since `recording_timer_` started.
  std::u16string recording_duration_;

  // Indicates whether the Game Dashboard welcome dialog should be shown. This
  // param ensures the welcome dialog is only shown once per game window
  // startup.
  bool show_welcome_dialog_ = false;

  base::WeakPtrFactory<GameDashboardContext> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_
