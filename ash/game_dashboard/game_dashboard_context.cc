// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/game_dashboard/game_dashboard_button.h"
#include "ash/game_dashboard/game_dashboard_button_reveal_controller.h"
#include "ash/game_dashboard/game_dashboard_constants.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_main_menu_cursor_handler.h"
#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/game_dashboard/game_dashboard_metrics.h"
#include "ash/game_dashboard/game_dashboard_toolbar_view.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/game_dashboard_welcome_dialog.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/i18n/time_formatting.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/frame_header.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr base::TimeDelta kCountUpTimerRefreshInterval = base::Seconds(1);

const std::u16string& kDefaultRecordingDuration = u"00:00";

// Number of pixels to add to the top and bottom of the Game Dashboard button so
// that it's centered within the frame header.
constexpr int kGameDashboardButtonVerticalPaddingDp = 3;

// Maximum width of the game window that centers the welcome dialog in the
// window instead of right aligned.
constexpr int kMaxCenteredWelcomeDialogWidth =
    1.5 * game_dashboard::kWelcomeDialogFixedWidth;

// The animation duration for the bounds change operation on the toolbar widget.
constexpr base::TimeDelta kToolbarBoundsChangeAnimationDuration =
    base::Milliseconds(150);

std::unique_ptr<views::Widget> CreateTransientChildWidget(
    aura::Window* game_window,
    const std::string& widget_name,
    std::unique_ptr<views::View> view,
    views::Widget::InitParams::Activatable activatable =
        views::Widget::InitParams::Activatable::kDefault) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // Sets the widget as a transient child, which is actually a sibling
  // of the window. This ensures that this widget will not show up in
  // screenshots or screen recordings.
  params.parent = game_window;
  params.name = widget_name;
  params.activatable = activatable;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  wm::TransientWindowManager::GetOrCreate(widget->GetNativeWindow())
      ->set_parent_controls_visibility(true);
  widget->SetContentsView(std::move(view));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);

  return widget;
}

// Tells the camera preview to maybe update its position. This will ensure that
// the preview doesn't overlap with the toolbar.
void MaybeUpdateCameraPreview() {
  CaptureModeController::Get()->camera_controller()->MaybeUpdatePreviewWidget(
      /*animate=*/true);
}

// Determines whether a given `key_code` will interact with the toolbar.
bool WillToolbarViewProcessKeyCode(const ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_RIGHT:
    case ui::VKEY_LEFT:
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
    case ui::VKEY_RETURN:
    case ui::VKEY_SPACE:
      return true;
    default:
      return false;
  }
}

}  // namespace

GameDashboardContext::GameDashboardContext(aura::Window* game_window)
    : game_window_(game_window),
      app_id_(*game_window->GetProperty(kAppIDKey)),
      toolbar_snap_location_(GameDashboardToolbarSnapLocation::kTopRight) {
  DCHECK(game_window_);
  window_state_observation_.Observe(WindowState::Get(game_window_));
  show_welcome_dialog_ = game_dashboard_utils::ShouldShowWelcomeDialog();
}

GameDashboardContext::~GameDashboardContext() {
  MaybeRemovePreTargetHandler();
  window_state_observation_.Reset();
  game_dashboard_button_->RemoveObserver(this);
  if (main_menu_widget_) {
    main_menu_widget_->CloseNow();
  }
  CloseWelcomeDialogIfAny(/*show_toolbar=*/false);
}

const std::u16string& GameDashboardContext::GetRecordingDuration() const {
  return recording_duration_.empty() ? kDefaultRecordingDuration
                                     : recording_duration_;
}

void GameDashboardContext::EnableFeatures(
    bool enable,
    GameDashboardMainMenuToggleMethod main_menu_toggle_method) {
  DCHECK(game_dashboard_button_widget_)
      << "Game Dashboard button doesn't exist. Make sure to call Initialize() "
         "before trying to use the context.";
  if (enable) {
    SetGameDashboardButtonVisibility(/*visible=*/true);
    SetToolbarVisibility(/*visible=*/true);
  } else {
    CloseWelcomeDialogIfAny();
    SetToolbarVisibility(/*visible=*/false);
    if (main_menu_widget_) {
      CloseMainMenu(main_menu_toggle_method);
    }
    SetGameDashboardButtonVisibility(/*visible=*/false);
  }
}

void GameDashboardContext::Initialize() {
  CHECK(!game_dashboard_button_widget_)
      << "The context can only be initialized once.";
  CreateAndAddGameDashboardButtonWidget();
  // ARC windows handle displaying the welcome dialog once the
  // `game_dashboard_button_` becomes available.
  if (!IsArcWindow(game_window_)) {
    MaybeShowWelcomeDialog();
  }
  // The pretarget handler must be added when the context is initialized, since
  // `OnWindowActivated()` is called before the context was created and
  // initialized.
  MaybeAddPreTargetHandler();
}

void GameDashboardContext::MaybeStackAboveWidget(views::Widget* widget) {
  DCHECK(widget);
  DCHECK(game_dashboard_button_widget_);

  game_dashboard_button_widget_->StackAboveWidget(widget);

  if (welcome_dialog_widget_) {
    welcome_dialog_widget_->StackAboveWidget(widget);
  }

  if (main_menu_widget_) {
    main_menu_widget_->StackAboveWidget(widget);
  }

  if (toolbar_widget_) {
    toolbar_widget_->StackAboveWidget(widget);
  }

  EnsureMainMenuAboveToolbar();
}

void GameDashboardContext::SetGameDashboardToolbarSnapLocation(
    GameDashboardToolbarSnapLocation new_location) {
  toolbar_snap_location_ = new_location;
  AnimateToolbarWidgetBoundsChange(CalculateToolbarWidgetBounds());
  MaybeUpdateCameraPreview();
  RecordGameDashboardToolbarNewLocation(app_id_, new_location);
}

void GameDashboardContext::OnWindowBoundsChanged(bool from_animation) {
  UpdateGameDashboardButtonWidgetBounds();
  MaybeUpdateToolbarWidgetBounds();
  MaybeUpdateWelcomeDialogBounds();

  if (from_animation) {
    EnableFeatures(
        /*enable=*/!game_window_->layer()->GetAnimator()->is_animating(),
        GameDashboardMainMenuToggleMethod::kAnimation);
  }
}

void GameDashboardContext::UpdateForGameControlsFlags() {
  CHECK(IsArcWindow(game_window_));

  const bool should_enable_button =
      game_dashboard_utils::ShouldEnableGameDashboardButton(game_window_);
  game_dashboard_button_->SetEnabled(should_enable_button);
  if (should_enable_button) {
    // ARC windows handle displaying the welcome dialog once the
    // `game_dashboard_button_` becomes available.
    MaybeShowWelcomeDialog();
  }

  if (toolbar_view_) {
    toolbar_view_->UpdateViewForGameControls(
        game_window_->GetProperty(kArcGameControlsFlagsKey));
  }

  // Ensure that the main menu is above the toolbar because updating toolbar
  // changes its zorder.
  EnsureMainMenuAboveToolbar();
}

void GameDashboardContext::ToggleMainMenuByAccelerator() {
  if (game_dashboard_button_reveal_controller_) {
    // Window is in fullscreen, and `game_dashboard_button_widget_` may not be
    // visible. Reset its position and make it visible. Don't animate the button
    // so it and the main menu show up at the same time.
    game_dashboard_button_reveal_controller_->UpdateVisibility(
        /*target_visibility=*/true, /*animate=*/false);
  }

  ToggleMainMenu(GameDashboardMainMenuToggleMethod::kSearchPlusG);
}

void GameDashboardContext::ToggleMainMenu(
    GameDashboardMainMenuToggleMethod toggle_method) {
  if (!main_menu_widget_) {
    // If opened, close the welcome dialog, before opening the main menu.
    CloseWelcomeDialogIfAny();
    auto widget_delegate = std::make_unique<GameDashboardMainMenuView>(this);
    DCHECK(!main_menu_view_);
    main_menu_view_ = widget_delegate.get();
    main_menu_widget_ =
        base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
            std::move(widget_delegate)));
    main_menu_widget_->AddObserver(this);
    main_menu_widget_->Show();
    game_dashboard_utils::UpdateAccessibilityTree(GetTraversableWidgets());
    game_dashboard_button_->SetToggled(true);
    AddCursorHandler();
    RecordGameDashboardToggleMainMenu(app_id_, toggle_method,
                                      /*toggled_on=*/true);
  } else {
    DCHECK(main_menu_view_);
    DCHECK(main_menu_widget_);
    CloseMainMenu(toggle_method);
  }
}

void GameDashboardContext::CloseMainMenu(
    GameDashboardMainMenuToggleMethod toggle_method) {
  DCHECK(main_menu_widget_);
  main_menu_widget_->RemoveObserver(this);
  // Reset the `main_menu_widget_` before calling `UpdateOnMainMenuClosed()` to
  // ensure all Game Dashboard widgets are up-to-date.
  main_menu_widget_.reset();
  // Since the `WidgetObserver` has been removed, `OnWidgetDestroyed` will not
  // be called. Explicitly call `UpdateOnMainMenuClosed()` to update the
  // `main_menu_view_`, remove the cursor handler, and update the
  // `game_dashboard_button_` UI.
  UpdateOnMainMenuClosed();
  RecordGameDashboardToggleMainMenu(app_id_, toggle_method,
                                    /*toggled_on=*/false);
}

bool GameDashboardContext::ToggleToolbar() {
  if (!toolbar_widget_) {
    auto view = std::make_unique<GameDashboardToolbarView>(this);
    DCHECK(!toolbar_view_);
    toolbar_view_ = view.get();
    toolbar_widget_ = CreateTransientChildWidget(
        game_window_, "GameDashboardToolbar", std::move(view));
    DCHECK_EQ(game_window_,
              wm::GetTransientParent(toolbar_widget_->GetNativeWindow()));
    toolbar_widget_->widget_delegate()->SetAccessibleTitle(
        l10n_util::GetStringUTF16(
            IDS_ASH_GAME_DASHBOARD_TOOLBAR_TILE_BUTTON_TITLE));
    MaybeUpdateToolbarWidgetBounds();

    SetToolbarVisibility(/*visible=*/true);
    game_dashboard_utils::UpdateAccessibilityTree(GetTraversableWidgets());
    // Display the toolbar behind the main menu view.
    EnsureMainMenuAboveToolbar();
    RecordGameDashboardToolbarToggleState(app_id_, /*toggled_on=*/true);
    return true;
  }

  CloseToolbar();
  return false;
}

void GameDashboardContext::CloseToolbar() {
  DCHECK(toolbar_view_);
  DCHECK(toolbar_widget_);
  toolbar_view_ = nullptr;
  toolbar_widget_.reset();
  game_dashboard_utils::UpdateAccessibilityTree(GetTraversableWidgets());
  RecordGameDashboardToolbarToggleState(app_id_, /*toggled_on=*/false);
}

void GameDashboardContext::MaybeUpdateToolbarWidgetBounds() {
  if (toolbar_widget_) {
    toolbar_widget_->SetBounds(CalculateToolbarWidgetBounds());
    MaybeUpdateCameraPreview();
  }
}

bool GameDashboardContext::IsToolbarVisible() const {
  return toolbar_widget_ && toolbar_widget_->IsVisible();
}

gfx::Rect GameDashboardContext::GetToolbarBoundsInScreen() const {
  return IsToolbarVisible() ? toolbar_widget_->GetWindowBoundsInScreen()
                            : gfx::Rect{};
}

void GameDashboardContext::OnRecordingStarted(bool is_recording_game_window) {
  if (is_recording_game_window) {
    CHECK(!recording_timer_.IsRunning());
    DCHECK(recording_start_time_.is_null());
    DCHECK(recording_duration_.empty());
    game_dashboard_button_->OnRecordingStarted();
    recording_start_time_ = base::Time::Now();
    OnUpdateRecordingTimer();
    recording_timer_.Start(FROM_HERE, kCountUpTimerRefreshInterval, this,
                           &GameDashboardContext::OnUpdateRecordingTimer);
    CHECK(recording_from_main_menu_);
    RecordGameDashboardRecordingStartSource(
        app_id_, *recording_from_main_menu_ ? GameDashboardMenu::kMainMenu
                                            : GameDashboardMenu::kToolbar);
    // `recording_from_main_menu_` is used to record the histogram for starting
    // recording only. Reset it after the histogram is recorded.
    recording_from_main_menu_ = std::nullopt;
  }
  if (main_menu_view_) {
    main_menu_view_->OnRecordingStarted(is_recording_game_window);
  }
  if (toolbar_view_) {
    toolbar_view_->OnRecordingStarted(is_recording_game_window);
  }
}

void GameDashboardContext::OnRecordingEnded() {
  // Resetting the timer will stop the timer.
  recording_timer_.Stop();
  recording_start_time_ = base::Time();
  recording_duration_.clear();
  game_dashboard_button_->OnRecordingEnded();
  if (main_menu_view_) {
    main_menu_view_->OnRecordingEnded();
  }
  if (toolbar_view_) {
    toolbar_view_->OnRecordingEnded();
  }
}

void GameDashboardContext::OnVideoFileFinalized() {
  // For now it's ok to just call `OnRecordingEnded()` to update the UI.
  OnRecordingEnded();
}

void GameDashboardContext::SetGameDashboardButtonVisibility(bool visible) {
  if (visible && !game_dashboard_button_widget_->IsVisible() &&
      !display::Screen::GetScreen()->InTabletMode()) {
    // Show the Game Dashboard button if it's not visible.
    // When the top edge timer fires, it's going to try to show the Game
    // Dashboard button. Because this is already showing the button, stop
    // the top edge timer.
    if (game_dashboard_button_reveal_controller_) {
      game_dashboard_button_reveal_controller_->StopTopEdgeTimer();
    }
    game_dashboard_button_widget_->ShowInactive();
  } else if (!visible && game_dashboard_button_widget_->IsVisible() &&
             !IsMainMenuOpen()) {
    // Hide the Game Dashboard button if its visible and the main menu is
    // closed.
    game_dashboard_button_widget_->Hide();
  }
}

void GameDashboardContext::SetToolbarVisibility(bool visible) {
  if (!toolbar_widget_) {
    return;
  }
  // If the toolbar should be visible and currently is not, show it. If the
  // toolbar should not be visible and currently is, hide it. Otherwise, leave
  // the toolbar in whatever state it is in.
  const bool is_toolbar_visible = toolbar_widget_->IsVisible();
  if (visible && !is_toolbar_visible) {
    // Calling `Show()` on a widget activates that given widget, which causes a
    // crash when Game Dashboard widgets are added to multiple windows while
    // exiting overview mode. To avoid changing the activated window after a
    // user has already selected a window in overview mode, show all widgets as
    // inactive.
    toolbar_widget_->ShowInactive();
  } else if (!visible && is_toolbar_visible) {
    toolbar_widget_->Hide();
  }
}

void GameDashboardContext::MaybeAddPreTargetHandler() {
  if (!game_window_->Contains(
          wm::GetTransientRoot(window_util::GetActiveWindow()))) {
    // Don't add a pretarget handler to any window whose transient root is not
    // active.
    return;
  }

  if (!added_to_pre_target_handler_) {
    added_to_pre_target_handler_ = true;
    // The pretarget handler must be added to the Shell in order to be
    // properly notified of all events interacting with different widgets
    // within the game window.
    Shell::Get()->AddPreTargetHandler(this);
  }
}

void GameDashboardContext::MaybeRemovePreTargetHandler() {
  if (added_to_pre_target_handler_) {
    added_to_pre_target_handler_ = false;
    Shell::Get()->RemovePreTargetHandler(this);
  }
}

void GameDashboardContext::OnEvent(ui::Event* event) {
  // Close the main menu if the user clicks outside of both the main menu
  // widget and the Game Dashboard button.
  auto event_type = event->type();
  if (event_type == ui::EventType::kKeyPressed) {
    const ui::KeyEvent* key_event = event->AsKeyEvent();
    if (toolbar_widget_ && toolbar_widget_->IsActive() && main_menu_widget_ &&
        WillToolbarViewProcessKeyCode(key_event->key_code())) {
      // Close the main menu if the toolbar processes the given key.
      CloseMainMenu(GameDashboardMainMenuToggleMethod::kOthers);
    } else if (ShouldNavigateToNewWidget(key_event)) {
      const auto* currently_focused = views::Widget::GetWidgetForNativeWindow(
          static_cast<aura::Window*>(event->target()));
      const bool reverse = event->IsShiftDown();

      // Manually move focus from the currently focused widget to the next in
      // the widget list. It is possible that `currently_focused` is not in the
      // `GetTraversableWidgets()`. For example, `currently_focused` is the app
      // window itself.
      if (auto* next_focus = game_dashboard_utils::GetNextWidgetToFocus(
              GetTraversableWidgets(), currently_focused, reverse)) {
        MoveFocus(next_focus, event, reverse);
      }
    }
  } else if (main_menu_widget_) {
    switch (event_type) {
      case ui::EventType::kTouchPressed:
      case ui::EventType::kMousePressed: {
        // TODO(b/328852471): Update logic to compare event target with native
        // window.
        const ui::LocatedEvent* located_event = event->AsLocatedEvent();
        const auto event_location =
            located_event->target()->GetScreenLocation(*located_event);
        if (!game_dashboard_button_->GetBoundsInScreen().Contains(
                event_location) &&
            !main_menu_widget_->GetWindowBoundsInScreen().Contains(
                event_location)) {
          // Touch/Mouse event occurred outside both the main menu widget and
          // the Game Dashboard button bounds. Ignore the bounds of the Game
          // Dashboard button since it will already toggle the main menu when
          // pressed.
          CloseMainMenu(GameDashboardMainMenuToggleMethod::kOthers);
        }
      } break;
      default:
        break;
    }
  }
  ui::EventHandler::OnEvent(event);
}

void GameDashboardContext::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  CHECK_EQ(game_dashboard_button_, observed_view);
  UpdateGameDashboardButtonWidgetBounds();
  MaybeUpdateWelcomeDialogBounds();
}

void GameDashboardContext::OnWidgetDestroyed(views::Widget* widget) {
  DCHECK(main_menu_view_);
  DCHECK_EQ(widget, main_menu_view_->GetWidget());
  UpdateOnMainMenuClosed();

  // Record main menu toggle off metrics.
  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kLostFocus:
      // The main menu is closed explicitly in the overview mode and the
      // observer is removed before this event.
      DCHECK(!OverviewController::Get()->InOverviewSession());
      // Close reason for clicking outside or closing game window by clicking
      // close button on the caption.
      RecordGameDashboardToggleMainMenu(
          app_id_, GameDashboardMainMenuToggleMethod::kOthers,
          /*toggled_on=*/false);
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      // Close reason for key Esc pressed.
      RecordGameDashboardToggleMainMenu(app_id_,
                                        GameDashboardMainMenuToggleMethod::kEsc,
                                        /*toggled_on=*/false);
      break;
    case views::Widget::ClosedReason::kUnspecified:
      // Close reason when the game window is closed unspecified.
      RecordGameDashboardToggleMainMenu(
          app_id_, GameDashboardMainMenuToggleMethod::kOthers,
          /*toggled_on=*/false);
      break;
    default:
      break;
  }
}

void GameDashboardContext::OnPreWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  // Hide the Game Dashboard button before the window switches to fullscreen.
  if (window_state->IsFullscreen()) {
    DCHECK(!game_dashboard_button_reveal_controller_);
    game_dashboard_button_reveal_controller_ =
        std::make_unique<GameDashboardButtonRevealController>(this);

    if (!chromeos::IsMinimizedWindowStateType(old_type)) {
      // When the window goes from minimized to fullscreen, hide the Game
      // Dashboard widget.
      game_dashboard_button_reveal_controller_->UpdateVisibility(
          /*target_visibility=*/false, /*animate=*/false);
    }
  }
}

void GameDashboardContext::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  if (!window_state->IsFullscreen() &&
      game_dashboard_button_reveal_controller_) {
    if (!window_state->IsMinimized()) {
      // When the window exits fullscreen and goes to any state that is not
      // minimize, show the Game Dashboard button widget. Otherwise do nothing.
      // Changing the visibility of the widget when the window is minimized
      // causes Chrome to crash.
      game_dashboard_button_reveal_controller_->UpdateVisibility(
          /*target_visibility=*/true, /*animate=*/false);
    }
    game_dashboard_button_reveal_controller_.reset();
  }
}

void GameDashboardContext::AddCursorHandler() {
  DCHECK(!main_menu_cursor_handler_);
  main_menu_cursor_handler_ =
      std::make_unique<GameDashboardMainMenuCursorHandler>(this);
  game_window_->AddPreTargetHandler(main_menu_cursor_handler_.get());
}

void GameDashboardContext::RemoveCursorHandler() {
  if (main_menu_cursor_handler_) {
    game_window_->RemovePreTargetHandler(main_menu_cursor_handler_.get());
    main_menu_cursor_handler_.reset();
  }
}

void GameDashboardContext::CreateAndAddGameDashboardButtonWidget() {
  auto game_dashboard_button = std::make_unique<GameDashboardButton>(
      base::BindRepeating(&GameDashboardContext::OnGameDashboardButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      game_dashboard_utils::GetFrameHeaderHeight(game_window_) / 2.0f);
  DCHECK(!game_dashboard_button_);
  game_dashboard_button_ = game_dashboard_button.get();
  // Allow the Game Dashboard button to be activatable so that it can be
  // focusable during tab navigation.
  game_dashboard_button_widget_ = CreateTransientChildWidget(
      game_window_, "GameDashboardButton", std::move(game_dashboard_button));
  // Add observer after `game_dashboard_button_widget_` is created because the
  // observation is to update `game_dashboard_button_widget_` bounds.
  game_dashboard_button_->AddObserver(this);
  DCHECK_EQ(
      game_window_,
      wm::GetTransientParent(game_dashboard_button_widget_->GetNativeWindow()));
  UpdateGameDashboardButtonWidgetBounds();
  if (game_dashboard_utils::ShouldEnableFeatures()) {
    SetGameDashboardButtonVisibility(/*visible=*/true);
  }
}

void GameDashboardContext::UpdateGameDashboardButtonWidgetBounds() {
  DCHECK(game_dashboard_button_widget_)
      << "Game Dashboard button doesn't exist. Make sure to call Initialize() "
         "before trying to use the context.";
  auto preferred_size =
      game_dashboard_button_widget_->GetContentsView()->GetPreferredSize();
  gfx::Point origin = game_window_->GetBoundsInScreen().top_center();

  const int frame_header_height =
      game_dashboard_utils::GetFrameHeaderHeight(game_window_);
  if (frame_header_height == 0) {
    VLOG(1) << "No frame header height. Not updating main menu widget bounds.";
    return;
  }
  // Position the button in the top center of the `FrameHeader`.
  origin.set_x(origin.x() - preferred_size.width() / 2);
  origin.set_y(origin.y() + kGameDashboardButtonVerticalPaddingDp);
  preferred_size.set_height(frame_header_height -
                            2 * kGameDashboardButtonVerticalPaddingDp);
  game_dashboard_button_widget_->SetBounds(gfx::Rect(origin, preferred_size));
}

void GameDashboardContext::OnGameDashboardButtonPressed() {
  // Close the welcome dialog if it's open when a user opens the main menu view.
  CloseWelcomeDialogIfAny();
  ToggleMainMenu(GameDashboardMainMenuToggleMethod::kGameDashboardButton);
}

void GameDashboardContext::MaybeShowWelcomeDialog() {
  // If the welcome dialog should not be shown, or the Game Dashboard feature is
  // disabled, do not show the welcome dialog.
  if (!show_welcome_dialog_ || !game_dashboard_utils::ShouldEnableFeatures()) {
    MaybeShowToolbar();
    return;
  }

  DCHECK(!welcome_dialog_widget_);
  show_welcome_dialog_ = false;
  auto view = std::make_unique<GameDashboardWelcomeDialog>();
  GameDashboardWelcomeDialog* welcome_dialog_view = view.get();
  // Activatable for accessibility screen reader.
  welcome_dialog_widget_ = CreateTransientChildWidget(
      game_window_, "GameDashboardWelcomeDialog", std::move(view),
      /*activatable=*/views::Widget::InitParams::Activatable::kDefault);
  welcome_dialog_widget_->AddObserver(this);
  MaybeUpdateWelcomeDialogBounds();
  welcome_dialog_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_BOTH);
  welcome_dialog_widget_->SetVisibilityAnimationDuration(
      base::Milliseconds(700));
  welcome_dialog_widget_->ShowInactive();
  welcome_dialog_view->StartTimer(
      base::BindOnce(&GameDashboardContext::OnWelcomeDialogTimerCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  // Once the dialog is shown, add an announcement for screen readers since the
  // dialog only shows for a short amount of time.
  welcome_dialog_view->AnnounceForAccessibility();
}

void GameDashboardContext::MaybeUpdateWelcomeDialogBounds() {
  if (!welcome_dialog_widget_) {
    return;
  }

  const gfx::Rect game_bounds = game_window_->GetBoundsInScreen();
  const gfx::Size preferred_size =
      welcome_dialog_widget_->GetContentsView()->GetPreferredSize();
  const int frame_header_height =
      game_dashboard_utils::GetFrameHeaderHeight(game_window_);
  int origin_x;

  if (game_bounds.width() > kMaxCenteredWelcomeDialogWidth) {
    // Place welcome dialog right aligned in the game window.
    origin_x = game_bounds.right() - game_dashboard::kWelcomeDialogEdgePadding -
               preferred_size.width();
  } else {
    // Place welcome dialog centered in the game window.
    origin_x =
        game_bounds.x() + (game_bounds.width() - preferred_size.width()) / 2;
  }

  welcome_dialog_widget_->SetBounds(gfx::Rect(
      gfx::Point(origin_x, game_bounds.y() +
                               game_dashboard::kWelcomeDialogEdgePadding +
                               frame_header_height),
      preferred_size));
}

const gfx::Rect GameDashboardContext::CalculateToolbarWidgetBounds() {
  const gfx::Rect game_bounds = game_window_->GetBoundsInScreen();
  const gfx::Size preferred_size =
      toolbar_widget_->GetContentsView()->GetPreferredSize();
  const int frame_header_height =
      game_dashboard_utils::GetFrameHeaderHeight(game_window_);
  gfx::Point origin;

  switch (toolbar_snap_location_) {
    case GameDashboardToolbarSnapLocation::kTopRight:
      origin =
          gfx::Point(game_bounds.right() - game_dashboard::kToolbarEdgePadding -
                         preferred_size.width(),
                     game_bounds.y() + game_dashboard::kToolbarEdgePadding +
                         frame_header_height);
      break;
    case GameDashboardToolbarSnapLocation::kTopLeft:
      origin =
          gfx::Point(game_bounds.x() + game_dashboard::kToolbarEdgePadding,
                     game_bounds.y() + game_dashboard::kToolbarEdgePadding +
                         frame_header_height);
      break;
    case GameDashboardToolbarSnapLocation::kBottomRight:
      origin = gfx::Point(
          game_bounds.right() - game_dashboard::kToolbarEdgePadding -
              preferred_size.width(),
          game_bounds.bottom() - game_dashboard::kToolbarEdgePadding -
              preferred_size.height());
      break;
    case GameDashboardToolbarSnapLocation::kBottomLeft:
      origin = gfx::Point(game_bounds.x() + game_dashboard::kToolbarEdgePadding,
                          game_bounds.bottom() -
                              game_dashboard::kToolbarEdgePadding -
                              preferred_size.height());
      break;
  }

  return gfx::Rect(origin, preferred_size);
}

void GameDashboardContext::AnimateToolbarWidgetBoundsChange(
    const gfx::Rect& target_screen_bounds) {
  DCHECK(toolbar_widget_);
  auto* toolbar_window = toolbar_widget_->GetNativeWindow();
  const auto current_bounds = toolbar_window->GetBoundsInScreen();
  if (target_screen_bounds == current_bounds) {
    return;
  }

  toolbar_widget_->SetBounds(target_screen_bounds);
  const auto transform = gfx::Transform::MakeTranslation(
      current_bounds.CenterPoint() - target_screen_bounds.CenterPoint());
  ui::Layer* layer = toolbar_window->layer();
  layer->SetTransform(transform);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kToolbarBoundsChangeAnimationDuration)
      .SetTransform(layer, gfx::Transform(), gfx::Tween::ACCEL_0_80_DECEL_80);
}

void GameDashboardContext::MaybeShowToolbar() {
  if (game_dashboard_utils::ShouldShowToolbar() && !toolbar_widget_ &&
      !OverviewController::Get()->InOverviewSession() &&
      !display::Screen::GetScreen()->InTabletMode()) {
    // Show the toolbar, if it's not already showing.
    ToggleToolbar();
    DCHECK(toolbar_widget_);
  }
}

void GameDashboardContext::OnUpdateRecordingTimer() {
  DCHECK(!recording_start_time_.is_null());
  const base::TimeDelta delta = base::Time::Now() - recording_start_time_;
  std::u16string duration;
  if (!base::TimeDurationFormatWithSeconds(
          delta, base::DurationFormatWidth::DURATION_WIDTH_NUMERIC,
          &duration)) {
    VLOG(1) << "Error converting the duration to a string: " << duration;
    return;
  }
  // Remove the leading `0:` for durations less than an hour.
  if (delta < base::Hours(1)) {
    base::ReplaceFirstSubstringAfterOffset(&duration, 0, u"0:", u"");
  }
  recording_duration_ = duration;
  game_dashboard_button_->UpdateRecordingDuration(duration);
  if (main_menu_view_) {
    main_menu_view_->UpdateRecordingDuration(duration);
  }
}

void GameDashboardContext::CloseWelcomeDialogIfAny(bool show_toolbar) {
  if (welcome_dialog_widget_) {
    welcome_dialog_widget_->SetVisibilityAnimationDuration(
        base::Milliseconds(300));
    welcome_dialog_widget_->Hide();
    welcome_dialog_widget_->RemoveObserver(this);
    welcome_dialog_widget_.reset();
    if (show_toolbar) {
      MaybeShowToolbar();
    }
  }
}

void GameDashboardContext::OnWelcomeDialogTimerCompleted() {
  CloseWelcomeDialogIfAny();
}

void GameDashboardContext::UpdateOnMainMenuClosed() {
  DCHECK(main_menu_view_);
  DCHECK(!main_menu_widget_.get());
  RemoveCursorHandler();
  main_menu_view_ = nullptr;
  // Update the accessibility tree since `main_menu_widget_` has been destroyed.
  game_dashboard_utils::UpdateAccessibilityTree(GetTraversableWidgets());
  game_dashboard_button_->SetToggled(false);
}

void GameDashboardContext::EnsureMainMenuAboveToolbar() {
  if (main_menu_widget_ && toolbar_widget_) {
    main_menu_widget_->StackAboveWidget(toolbar_widget_.get());
  }
}

bool GameDashboardContext::ShouldNavigateToNewWidget(
    const ui::KeyEvent* event) const {
  // Tab navigation between Game Dashboard sibling widgets is only supported
  // when the GD button is enabled.
  if (!game_dashboard_button_->GetEnabled() ||
      event->type() != ui::EventType::kKeyPressed ||
      event->key_code() != ui::VKEY_TAB) {
    return false;
  }

  if (auto* target_widget = views::Widget::GetWidgetForNativeWindow(
          static_cast<aura::Window*>(event->target()))) {
    if (auto* focus_manager = target_widget->GetFocusManager()) {
      // If `GetNextFocusableView` returns null, navigation has reached the last
      // focusable view in the given direction.
      return !(focus_manager->GetNextFocusableView(
          /*starting_view=*/focus_manager->GetFocusedView(),
          /*starting_widget=*/target_widget,
          /*reverse=*/event->IsShiftDown(),
          /*dont_loop=*/true));
    }
  }

  return false;
}

std::vector<views::Widget*> GameDashboardContext::GetTraversableWidgets()
    const {
  std::vector<views::Widget*> widget_list;
  widget_list.emplace_back(game_dashboard_button_widget_.get());
  if (main_menu_widget_) {
    widget_list.emplace_back(main_menu_widget_.get());
  }
  if (toolbar_widget_) {
    widget_list.emplace_back(toolbar_widget_.get());
  }
  if (widget_list.size() == 1) {
    // If the toolbar and main menu widgets don't exist but focus is placed on
    // the Game Dashboard button, manually move focus to the game window to
    // avoid tab support looping just the Game Dashboard button.
    widget_list.emplace_back(
        views::Widget::GetWidgetForNativeWindow(game_window_.get()));
  }

  return widget_list;
}

void GameDashboardContext::MoveFocus(views::Widget* new_widget,
                                     ui::Event* event,
                                     bool reverse) {
  CHECK(new_widget) << "Cannot move focus to a non-existent widget.";
  auto* focus_manager = new_widget->GetFocusManager();
  DCHECK(focus_manager) << "Cannot move focus without a focus manager";
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same view when the parent view
  // is refocused.
  focus_manager->SetStoredFocusView(nullptr);
  focus_manager->AdvanceFocus(reverse);
  event->StopPropagation();
  event->SetHandled();
}

}  // namespace ash
