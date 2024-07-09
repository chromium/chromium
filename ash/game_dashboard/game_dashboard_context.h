// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/game_dashboard/game_dashboard_metrics.h"
#include "ash/wm/window_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"
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
class GameDashboardButtonRevealController;
class GameDashboardMainMenuCursorHandler;
class GameDashboardMainMenuView;
class GameDashboardToolbarView;

// This class manages Game Dashboard related UI for a given `aura::Window`, and
// its instance is managed by the `GameDashboardController`.
class ASH_EXPORT GameDashboardContext : public ui::EventHandler,
                                        public views::ViewObserver,
                                        public views::WidgetObserver,
                                        public WindowStateObserver {
 public:
  explicit GameDashboardContext(aura::Window* game_window);
  GameDashboardContext(const GameDashboardContext&) = delete;
  GameDashboardContext& operator=(const GameDashboardContext&) = delete;
  ~GameDashboardContext() override;

  aura::Window* game_window() { return game_window_.get(); }

  const std::string& app_id() const { return app_id_; }

  GameDashboardMainMenuView* main_menu_view() { return main_menu_view_; }

  base::WeakPtr<GameDashboardContext> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  views::Widget* game_dashboard_button_widget() {
    return game_dashboard_button_widget_.get();
  }

  GameDashboardToolbarSnapLocation toolbar_snap_location() const {
    return toolbar_snap_location_;
  }

  void set_recording_from_main_menu(bool from_main_menu) {
    recording_from_main_menu_ = from_main_menu;
  }

  // Returns true if the main menu is opened. `main_menu_widget_` is created
  // only when the main menu is opened, otherwise it's null.
  bool IsMainMenuOpen() const { return main_menu_widget_.get(); }

  const std::u16string& GetRecordingDuration() const;

  // Initializes the context. Creates and starts showing the Game Dashboard
  // button. Also shows the welcome dialog, if
  // `prefs::kGameDashboardShowWelcomeDialog` is true. Separating this logic
  // ensures the constructor never references the context created before the
  // instance is assigned. Note that this logic should be called once the
  // context is created.
  void Initialize();

  // Stacks Game Dashboard UI widgets above `widget` if it is needed.
  void MaybeStackAboveWidget(views::Widget* widget);

  // Enables the Game Dashboard widgets' UI visibility if `enable` is true.
  // Otherwise, hides the widgets, and uses `main_menu_toggle_method` to
  // conditionally hide the toolbar.
  void EnableFeatures(
      bool enable,
      GameDashboardMainMenuToggleMethod main_menu_toggle_method);

  // Reassigns the new `toolbar_snap_location_` and performs an animation as the
  // toolbar moves to its new location.
  void SetGameDashboardToolbarSnapLocation(
      GameDashboardToolbarSnapLocation new_location);

  // Called by `GameDashboardController` when the game window bounds change.
  // `from_animation` is true when window bounds are changing due to an
  // animation.
  void OnWindowBoundsChanged(bool from_animation);

  // Updates for Game Controls flags.
  void UpdateForGameControlsFlags();

  // Toggles the creation/deletion of the main menu within the game window.
  void ToggleMainMenuByAccelerator();
  void ToggleMainMenu(GameDashboardMainMenuToggleMethod toggle_method);

  // Closes the main menu. Clears `main_menu_widget_` and `main_menu_view_`.
  void CloseMainMenu(GameDashboardMainMenuToggleMethod toggle_method);

  // Toggles the creation/deletion of the toolbar within the game window.
  // Returns the toolbar visibility state.
  bool ToggleToolbar();

  // Closes the toolbar. Clears `toolbar_widget_` and `toolbar_view_`.
  void CloseToolbar();

  // Conditionally, updates the toolbar widget's bounds and location, relative
  // to the `game_window_`.
  void MaybeUpdateToolbarWidgetBounds();

  bool IsToolbarVisible() const;

  // Returns the toolbar bounds if `IsToolbarVisible`, otherwise an empty rect.
  gfx::Rect GetToolbarBoundsInScreen() const;

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

  // Controls the Game Dashboard Button visibility.
  void SetGameDashboardButtonVisibility(bool visible);

  // Controls the Toolbar widget visibility.
  void SetToolbarVisibility(bool visible);

  // Conditionally, adds this context to the pre-target handler if it hasn't
  // already been added.
  void MaybeAddPreTargetHandler();

  // Conditionally, removes this context from the pre-target handler if it
  // hasn't already been removed.
  void MaybeRemovePreTargetHandler();

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

 private:
  friend class GameDashboardContextTestApi;

  // Registers a pretarget handler to always show the mouse cursor. Called when
  // the user opens the main menu.
  void AddCursorHandler();

  // Unregisters the pretarget handler that always shows the mouse cursor.
  // Called when the user closes the main menu.
  void RemoveCursorHandler();

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

  // Updates the toolbar widget's bounds and location utilizing an animation as
  // it transfers from the previous location.
  void AnimateToolbarWidgetBoundsChange(const gfx::Rect& target_screen_bounds);

  // Shows the Game Dashboard toolbar if `prefs::kGameDashboardShowToolbar` is
  // true.
  void MaybeShowToolbar();

  // Repeating timer callback that notifies `main_menu_view_` of the video
  // recording session duration.
  void OnUpdateRecordingTimer();

  // Closes and deletes the Game Dashboard welcome dialog. After closing the
  // dialog, if `show_toolbar` is true, call `MaybeShowToolbar`.
  void CloseWelcomeDialogIfAny(bool show_toolbar = true);

  // Callback when the `GameDashboardWelcomeDialog`'s timer has completed.
  void OnWelcomeDialogTimerCompleted();

  // Resets the `main_menu_view_`, removes the cursor handler, and updates the
  // `game_dashboard_button_` UI.
  void UpdateOnMainMenuClosed();

  // Ensures that the main menu stacks above the toolbar.
  void EnsureMainMenuAboveToolbar();

  // Determines whether it's required to tab navigate from one Game Dashboard
  // widget to another widget. Returns false if tab-navigating within the same
  // widget.
  bool ShouldNavigateToNewWidget(const ui::KeyEvent* event) const;

  // Returns a list of visible Game Dashboard widgets that are available to be
  // traversed.
  std::vector<views::Widget*> GetTraversableWidgets() const;

  // Manually moves focus to the `new_widget`. If `reverse` is true, focus will
  // move backwards.
  void MoveFocus(views::Widget* new_widget, ui::Event* event, bool reverse);

  const raw_ptr<aura::Window> game_window_;

  const std::string app_id_;

  // Game Dashboard button widget for the Game Dashboard.
  std::unique_ptr<views::Widget> game_dashboard_button_widget_;

  // Delegate responsible for determining when to show/hide the Game Dashboard
  // button when `game_window_` is in fullscreen. This a temporary scoped
  // object that is around while `game_window_` is fullscreen.
  std::unique_ptr<GameDashboardButtonRevealController>
      game_dashboard_button_reveal_controller_;

  // Expanded main menu for the Game Dashboard, which displays the main menu and
  // the settings view.
  views::UniqueWidgetPtr main_menu_widget_;

  // The toolbar for the Game Dashboard.
  std::unique_ptr<views::Widget> toolbar_widget_;

  // The dialog displayed when the game window first opens.
  std::unique_ptr<views::Widget> welcome_dialog_widget_;

  // The indicator of the current corner that the toolbar is placed.
  GameDashboardToolbarSnapLocation toolbar_snap_location_;

  // The `GameDashboardButton` view in the `game_dashboard_button_widget_`.
  // Owned by the views hierarchy.
  raw_ptr<GameDashboardButton> game_dashboard_button_ = nullptr;

  // The `GameDashboardMainMenuView` when the user presses the Game Dashboard
  // button to display all Game Dashboard views. This displays the main menu and
  // settings views. Owned by the views hierarchy.
  raw_ptr<GameDashboardMainMenuView, DanglingUntriaged> main_menu_view_ =
      nullptr;

  // The `GameDashboardToolbarView` when the user makes the toolbar visible.
  // Owned by the views hierarchy.
  raw_ptr<GameDashboardToolbarView> toolbar_view_ = nullptr;

  // Handles cursor management when the main menu is open.
  std::unique_ptr<GameDashboardMainMenuCursorHandler> main_menu_cursor_handler_;

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

  // Indicates where the recording feature starts from the main menu. It is
  // false if the recording starts from the toolbar. It is null if the recording
  // is started from somewhere else.
  std::optional<bool> recording_from_main_menu_;

  // Indicates whether this context has been added as a Shell's pre-target
  // handler. This param ensures this context isn't added as a pre-target
  // handler multiple times.
  bool added_to_pre_target_handler_ = false;

  base::ScopedObservation<WindowState, WindowStateObserver>
      window_state_observation_{this};

  base::WeakPtrFactory<GameDashboardContext> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTEXT_H_
