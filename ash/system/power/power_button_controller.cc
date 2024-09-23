// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_controller.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_display_controller.h"
#include "ash/system/power/power_button_menu_item_view.h"
#include "ash/system/power/power_button_menu_metrics_type.h"
#include "ash/system/power/power_button_menu_screen_view.h"
#include "ash/system/power/power_button_menu_view.h"
#include "ash/system/power/power_button_screenshot_controller.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "ui/compositor/layer.h"
#include "ui/display/tablet_state.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace {

// Amount of time power button must be held to start the power menu animation
// for convertible/slate/detachable devices. This differs depending on whether
// the screen is on or off when the power button is initially pressed.
constexpr base::TimeDelta kShowMenuWhenScreenOnTimeout =
    base::Milliseconds(500);
constexpr base::TimeDelta kShowMenuWhenScreenOffTimeout =
    base::Milliseconds(2000);

// Time that power button should be pressed after power menu is shown before
// starting the cancellable pre-shutdown animation.
constexpr base::TimeDelta kRequestCancelableShutdownTimeout =
    base::Milliseconds(650);

enum PowerButtonUpState {
  UP_NONE = 0,
  UP_MENU_TIMER_WAS_RUNNING = 1 << 0,
  UP_PRE_SHUTDOWN_TIMER_WAS_RUNNING = 1 << 1,
  UP_SHOWING_ANIMATION_CANCELLED = 1 << 2,
  UP_CAN_CANCEL_SHUTDOWN_ANIMATION = 1 << 3,
  UP_MENU_WAS_OPENED = 1 << 4,
};

aura::Window* GetPowerMenuContainer() {
  return Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_PowerMenuContainer);
}

// Creates a fullscreen widget responsible for showing the power button menu.
std::unique_ptr<views::Widget> CreateMenuWidget() {
  auto menu_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.accept_events = true;
  params.name = "PowerButtonMenuWindow";
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.parent = GetPowerMenuContainer();
  menu_widget->Init(std::move(params));

  gfx::Rect widget_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  menu_widget->SetBounds(widget_bounds);

  // Enable arrow key - arrow right/left and down/up triggers the same focus
  // movement as tab/shift+tab.
  menu_widget->widget_delegate()->SetEnableArrowKeyTraversal(true);
  return menu_widget;
}

}  // namespace

constexpr base::TimeDelta PowerButtonController::kScreenStateChangeDelay;

constexpr base::TimeDelta PowerButtonController::kIgnoreRepeatedButtonUpDelay;

constexpr base::TimeDelta
    PowerButtonController::kIgnorePowerButtonAfterResumeDelay;

constexpr const char* PowerButtonController::kEdgeField;
constexpr const char* PowerButtonController::kLeftEdge;
constexpr const char* PowerButtonController::kRightEdge;
constexpr const char* PowerButtonController::kTopEdge;
constexpr const char* PowerButtonController::kBottomEdge;

PowerButtonController::PowerButtonController(
    BacklightsForcedOffSetter* backlights_forced_off_setter)
    : backlights_forced_off_setter_(backlights_forced_off_setter),
      lock_state_controller_(Shell::Get()->lock_state_controller()),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  ProcessCommandLine();
  display_controller_ = std::make_unique<PowerButtonDisplayController>(
      backlights_forced_off_setter_, tick_clock_);
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_client->AddObserver(this);
  power_manager_client->GetSwitchStates(base::BindOnce(
      &PowerButtonController::OnGetSwitchStates, weak_factory_.GetWeakPtr()));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTabletMode)) {
    AccelerometerReader::GetInstance()->AddObserver(this);
  }

  auto* shell = Shell::Get();
  shell->display_configurator()->AddObserver(this);
  backlights_forced_off_observation_.Observe(backlights_forced_off_setter);
  shell->lock_state_controller()->AddObserver(this);
  shell->session_controller()->AddObserver(this);
}

PowerButtonController::~PowerButtonController() {
  auto* shell = Shell::Get();
  shell->session_controller()->RemoveObserver(this);
  shell->lock_state_controller()->RemoveObserver(this);
  shell->display_configurator()->RemoveObserver(this);
  AccelerometerReader::GetInstance()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerButtonController::OnPreShutdownTimeout() {
  lock_state_controller_->RequestCancelableShutdown(
      ShutdownReason::POWER_BUTTON);
  // |menu_widget_| might be reset on login status change while shutting down.
  if (!menu_widget_) {
    return;
  }

  static_cast<PowerButtonMenuScreenView*>(menu_widget_->GetContentsView())
      ->power_button_menu_view()
      ->FocusPowerOffButton();
}

void PowerButtonController::OnLegacyPowerButtonEvent(bool down) {
  // Avoid starting the lock/shutdown sequence if the power button is pressed
  // while the screen is off (http://crbug.com/128451), unless an external
  // display is still on (http://crosbug.com/p/24912).
  if (brightness_is_zero_ && !internal_display_off_and_external_display_on_) {
    return;
  }

  if (!down) {
    return;
  }

  // Ignore the power button down event if the menu is partially opened.
  if (IsMenuOpened() && !show_menu_animation_done_) {
    return;
  }

  // If power button releases won't get reported correctly because we're not
  // running on official hardware, show menu animation on the first power
  // button press. On a further press while the menu is open, simply shut down
  // (http://crbug.com/945005).
  if (!show_menu_animation_done_) {
    StartPowerMenuAnimation(ShutdownReason::POWER_BUTTON);
  } else {
    lock_state_controller_->RequestShutdown(ShutdownReason::POWER_BUTTON);
  }
}

void PowerButtonController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  if (down) {
    force_off_on_button_up_ = false;
    if (UseTabletBehavior()) {
      force_off_on_button_up_ = true;

      // When the system resumes in response to the power button being pressed,
      // Chrome receives powerd's SuspendDone signal and notification that the
      // backlight has been turned back on before seeing the power button events
      // that woke the system. Avoid forcing off display just after resuming to
      // ensure that we don't turn the display off in response to the events.
      if (timestamp - last_resume_time_ <= kIgnorePowerButtonAfterResumeDelay) {
        force_off_on_button_up_ = false;
      }

      // The actual display may remain off for a short period after powerd asks
      // Chrome to turn it on. If the user presses the power button again during
      // this time, they probably intend to turn the display on. Avoid forcing
      // off in this case.
      if (timestamp - display_controller_->screen_state_last_changed() <=
          kScreenStateChangeDelay) {
        force_off_on_button_up_ = false;
      }
    }

    screen_off_when_power_button_down_ = !display_controller_->IsScreenOn();
    menu_shown_when_power_button_down_ = show_menu_animation_done_;
    display_controller_->SetBacklightsForcedOff(false);

    if (menu_shown_when_power_button_down_) {
      pre_shutdown_timer_.Start(FROM_HERE, kRequestCancelableShutdownTimeout,
                                this,
                                &PowerButtonController::OnPreShutdownTimeout);
      return;
    }

    if (!UseTabletBehavior()) {
      StartPowerMenuAnimation(ShutdownReason::POWER_BUTTON);
    } else {
      base::TimeDelta timeout = screen_off_when_power_button_down_
                                    ? kShowMenuWhenScreenOffTimeout
                                    : kShowMenuWhenScreenOnTimeout;

      power_button_menu_timer_.Start(
          FROM_HERE, timeout,
          base::BindOnce(&PowerButtonController::StartPowerMenuAnimation,
                         base::Unretained(this), ShutdownReason::POWER_BUTTON));
    }
  } else {
    uint32_t up_state = UP_NONE;
    if (lock_state_controller_->MaybeCancelShutdownAnimation()) {
      up_state |= UP_CAN_CANCEL_SHUTDOWN_ANIMATION;
    }
    const base::TimeTicks previous_up_time = last_button_up_time_;
    last_button_up_time_ = timestamp;

    const bool menu_timer_was_running = power_button_menu_timer_.IsRunning();
    const bool pre_shutdown_timer_was_running = pre_shutdown_timer_.IsRunning();
    power_button_menu_timer_.Stop();
    pre_shutdown_timer_.Stop();

    const bool menu_was_partially_opened =
        IsMenuOpened() && !show_menu_animation_done_;
    // Cancel the menu animation if it's still ongoing when the button is
    // released.
    if (menu_was_partially_opened) {
      static_cast<PowerButtonMenuScreenView*>(menu_widget_->GetContentsView())
          ->ScheduleShowHideAnimation(false);
      up_state |= UP_SHOWING_ANIMATION_CANCELLED;
    }

    // If the button is tapped (i.e. not held long enough to start the
    // cancellable shutdown animation) while the menu is open, dismiss the menu.
    if (menu_shown_when_power_button_down_ && pre_shutdown_timer_was_running) {
      DismissMenu();
    }

    // Ignore the event if it comes too soon after the last one.
    if (timestamp - previous_up_time <= kIgnoreRepeatedButtonUpDelay) {
      return;
    }

    if (!screen_off_when_power_button_down_) {
      if (menu_timer_was_running) {
        up_state |= UP_MENU_TIMER_WAS_RUNNING;
      }
      if (pre_shutdown_timer_was_running) {
        up_state |= UP_PRE_SHUTDOWN_TIMER_WAS_RUNNING;
      }
      if (show_menu_animation_done_) {
        up_state |= UP_MENU_WAS_OPENED;
      }
      UpdatePowerButtonEventUMAHistogram(up_state);
    }

    if (screen_off_when_power_button_down_ || !force_off_on_button_up_) {
      return;
    }

    if (menu_timer_was_running || (menu_shown_when_power_button_down_ &&
                                   pre_shutdown_timer_was_running)) {
      display_controller_->SetBacklightsForcedOff(true);
      LockScreenIfRequired();
    }
  }
}

void PowerButtonController::OnLockButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  lock_button_down_ = down;

  // Ignore the lock button behavior if power button is being pressed.
  if (power_button_down_) {
    return;
  }

  const SessionControllerImpl* const session_controller =
      Shell::Get()->session_controller();
  if (!session_controller->CanLockScreen() ||
      session_controller->IsScreenLocked() ||
      lock_state_controller_->LockRequested() ||
      lock_state_controller_->ShutdownRequested()) {
    return;
  }

  if (down) {
    lock_state_controller_->StartLockAnimation();
  } else {
    lock_state_controller_->CancelLockAnimation();
  }
}

bool PowerButtonController::IsMenuOpened() const {
  return menu_widget_ && menu_widget_->GetLayer()->GetTargetVisibility();
}

void PowerButtonController::ShowMenuOnDebugAccelerator() {
  StartPowerMenuAnimation(ShutdownReason::DEBUG_ACCELERATOR);
}

void PowerButtonController::DismissMenu() {
  if (IsMenuOpened()) {
    static_cast<PowerButtonMenuScreenView*>(menu_widget_->GetContentsView())
        ->ResetOpacity();
    menu_widget_->Hide();
  }

  show_menu_animation_done_ = false;
  active_window_paint_as_active_lock_.reset();
}

void PowerButtonController::StopForcingBacklightsOff() {
  display_controller_->SetBacklightsForcedOff(false);
}

void PowerButtonController::OnArcPowerButtonMenuEvent() {
  StartPowerMenuAnimation(ShutdownReason::ARC_POWER_BUTTON);
}

void PowerButtonController::CancelPowerButtonEvent() {
  force_off_on_button_up_ = false;
  StopTimersAndDismissMenu();
}

void PowerButtonController::OnDisplayConfigurationChanged(
    const display::DisplayConfigurator::DisplayStateList& display_states) {
  bool internal_display_off = false;
  bool external_display_on = false;
  for (const display::DisplaySnapshot* display : display_states) {
    if (display->type() == display::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (!display->current_mode()) {
        internal_display_off = true;
      }
    } else if (display->current_mode()) {
      external_display_on = true;
    }
  }
  internal_display_off_and_external_display_on_ =
      internal_display_off && external_display_on;
}

void PowerButtonController::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  brightness_is_zero_ =
      change.percent() <= std::numeric_limits<double>::epsilon();
}

void PowerButtonController::PowerButtonEventReceived(
    bool down,
    base::TimeTicks timestamp) {
  if (lock_state_controller_->ShutdownRequested()) {
    return;
  }

  // Handle tablet mode power button screenshot accelerator.
  if (screenshot_controller_ &&
      screenshot_controller_->OnPowerButtonEvent(down, timestamp)) {
    return;
  }

  power_button_down_ = down;
  // Ignore power button if lock button is being pressed.
  if (lock_button_down_) {
    return;
  }

  button_type_ == ButtonType::LEGACY ? OnLegacyPowerButtonEvent(down)
                                     : OnPowerButtonEvent(down, timestamp);
}

void PowerButtonController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  DismissMenu();
}

void PowerButtonController::SuspendDone(base::TimeDelta sleep_duration) {
  last_resume_time_ = tick_clock_->NowTicks();
}

void PowerButtonController::OnLoginStatusChanged(LoginStatus status) {
  // Destroy |menu_widget_| on login status change to reset the content of the
  // menu since the menu items change if login status changed.
  menu_widget_.reset();
}

void PowerButtonController::OnGetSwitchStates(
    std::optional<chromeos::PowerManagerClient::SwitchStates> result) {
  if (!result.has_value()) {
    return;
  }

  if (result->tablet_mode !=
      chromeos::PowerManagerClient::TabletMode::UNSUPPORTED) {
    AccelerometerReader::GetInstance()->RemoveObserver(this);
    InitTabletPowerButtonMembers();
  }
}

void PowerButtonController::OnAccelerometerUpdated(
    const AccelerometerUpdate& update) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableTabletMode));

  // This device has at least an accelerometer, therefore it is a tablet or
  // convertible.
  AccelerometerReader::GetInstance()->RemoveObserver(this);
  InitTabletPowerButtonMembers();
}

void PowerButtonController::OnBacklightsForcedOffChanged(bool forced_off) {
  DismissMenu();
}

void PowerButtonController::OnScreenBacklightStateChanged(
    ScreenBacklightState screen_backlight_state) {
  if (screen_backlight_state != ScreenBacklightState::ON) {
    DismissMenu();
  }
}

void PowerButtonController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      in_tablet_mode_ = true;
      StopTimersAndDismissMenu();
      break;
    case display::TabletState::kInClamshellMode:
      in_tablet_mode_ = false;
      StopTimersAndDismissMenu();
      break;
  }
}

void PowerButtonController::OnSecurityCurtainEnabled() {
  DismissMenu();
  Shell::GetPrimaryRootWindow()->AddChild(GetPowerMenuContainer());
}

void PowerButtonController::OnSecurityCurtainDisabled() {
  DismissMenu();
  GetPowerMenuContainerParent(Shell::GetPrimaryRootWindow())
      ->AddChild(GetPowerMenuContainer());
}

void PowerButtonController::OnLockStateEvent(
    LockStateObserver::EventType event) {
  // Reset |lock_button_down_| when lock animation finished. LOCK_RELEASED is
  // not allowed when screen is locked, which means OnLockButtonEvent will not
  // be called in lock screen. This will lead |lock_button_down_| to stay in a
  // dirty state if press lock button after login but release in lock screen.
  if (event == EVENT_LOCK_ANIMATION_FINISHED) {
    lock_button_down_ = false;
  }
}

bool PowerButtonController::UseTabletBehavior() const {
  return in_tablet_mode_ || force_tablet_power_button_;
}

void PowerButtonController::StopTimersAndDismissMenu() {
  pre_shutdown_timer_.Stop();
  power_button_menu_timer_.Stop();
  DismissMenu();
}

void PowerButtonController::StartPowerMenuAnimation(ShutdownReason reason) {
  shutdown_reason_ = reason;

  // Avoid a distracting deactivation animation on the formerly-active
  // window when the menu is activated.
  views::Widget* active_toplevel_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          window_util::GetActiveWindow());
  active_window_paint_as_active_lock_ =
      active_toplevel_widget ? active_toplevel_widget->LockPaintAsActive()
                             : nullptr;

  if (!menu_widget_) {
    menu_widget_ = CreateMenuWidget();
    menu_widget_->SetContentsView(std::make_unique<PowerButtonMenuScreenView>(
        shutdown_reason_, power_button_position_,
        power_button_offset_percentage_,
        base::BindRepeating(&PowerButtonController::SetShowMenuAnimationDone,
                            base::Unretained(this))));
  }
  auto* contents_view =
      static_cast<PowerButtonMenuScreenView*>(menu_widget_->GetContentsView());
  contents_view->OnWidgetShown(power_button_position_,
                               power_button_offset_percentage_);
  menu_widget_->Show();

  // Hide cursor, but let it reappear if the mouse moves.
  Shell* shell = Shell::Get();
  if (shell->cursor_manager()) {
    shell->cursor_manager()->HideCursor();
  }

  contents_view->ScheduleShowHideAnimation(true);
}

void PowerButtonController::ProcessCommandLine() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  button_type_ = cl->HasSwitch(switches::kAuraLegacyPowerButton)
                     ? ButtonType::LEGACY
                     : ButtonType::NORMAL;
  force_tablet_power_button_ = cl->HasSwitch(switches::kForceTabletPowerButton);

  ParsePowerButtonPositionSwitch();
}

void PowerButtonController::InitTabletPowerButtonMembers() {
  if (!screenshot_controller_) {
    screenshot_controller_ =
        std::make_unique<PowerButtonScreenshotController>(tick_clock_);
  }
}

void PowerButtonController::LockScreenIfRequired() {
  const SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  if (session_controller->ShouldLockScreenAutomatically() &&
      session_controller->CanLockScreen() &&
      !session_controller->IsUserSessionBlocked() &&
      !lock_state_controller_->LockRequested()) {
    lock_state_controller_->LockWithoutAnimation();
  }
}

void PowerButtonController::SetShowMenuAnimationDone() {
  show_menu_animation_done_ = true;
  if (button_type_ != ButtonType::LEGACY &&
      shutdown_reason_ == ShutdownReason::POWER_BUTTON) {
    pre_shutdown_timer_.Start(FROM_HERE, kRequestCancelableShutdownTimeout,
                              this,
                              &PowerButtonController::OnPreShutdownTimeout);
  }
}

void PowerButtonController::ParsePowerButtonPositionSwitch() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(switches::kAshPowerButtonPosition)) {
    return;
  }

  std::optional<base::Value> parsed_json = base::JSONReader::Read(
      cl->GetSwitchValueASCII(switches::kAshPowerButtonPosition));
  if (!parsed_json || !parsed_json->is_dict()) {
    LOG(ERROR) << switches::kAshPowerButtonPosition << " flag has no value";
    return;
  }

  const base::Value::Dict& position_info = parsed_json->GetDict();
  const std::string* edge = position_info.FindString(kEdgeField);
  std::optional<double> position = position_info.FindDouble(kPositionField);

  if (!edge || !position) {
    LOG(ERROR) << "Both " << kEdgeField << " field and " << kPositionField
               << " are always needed if " << switches::kAshPowerButtonPosition
               << " is set";
    return;
  }

  power_button_offset_percentage_ = *position;

  if (*edge == kLeftEdge) {
    power_button_position_ = PowerButtonPosition::LEFT;
  } else if (*edge == kRightEdge) {
    power_button_position_ = PowerButtonPosition::RIGHT;
  } else if (*edge == kTopEdge) {
    power_button_position_ = PowerButtonPosition::TOP;
  } else if (*edge == kBottomEdge) {
    power_button_position_ = PowerButtonPosition::BOTTOM;
  } else {
    LOG(ERROR) << "Invalid " << kEdgeField << " field in "
               << switches::kAshPowerButtonPosition;
    return;
  }

  if (power_button_offset_percentage_ < 0 ||
      power_button_offset_percentage_ > 1.0f) {
    LOG(ERROR) << "Invalid " << kPositionField << " field in "
               << switches::kAshPowerButtonPosition;
    power_button_position_ = PowerButtonPosition::NONE;
  }
}

void PowerButtonController::UpdatePowerButtonEventUMAHistogram(
    uint32_t up_state) {
  if (up_state & UP_SHOWING_ANIMATION_CANCELLED) {
    RecordPressInLaptopModeHistogram(PowerButtonPressType::kTapWithoutMenu);
  }

  if (up_state & UP_MENU_TIMER_WAS_RUNNING) {
    RecordPressInTabletModeHistogram(PowerButtonPressType::kTapWithoutMenu);
  }

  if (menu_shown_when_power_button_down_) {
    if (up_state & UP_PRE_SHUTDOWN_TIMER_WAS_RUNNING) {
      force_off_on_button_up_
          ? RecordPressInTabletModeHistogram(PowerButtonPressType::kTapWithMenu)
          : RecordPressInLaptopModeHistogram(
                PowerButtonPressType::kTapWithMenu);
    } else if (!(up_state & UP_CAN_CANCEL_SHUTDOWN_ANIMATION)) {
      force_off_on_button_up_
          ? RecordPressInTabletModeHistogram(
                PowerButtonPressType::kLongPressWithMenuToShutdown)
          : RecordPressInLaptopModeHistogram(
                PowerButtonPressType::kLongPressWithMenuToShutdown);
    }
  } else if (up_state & UP_MENU_WAS_OPENED) {
    if (up_state & UP_PRE_SHUTDOWN_TIMER_WAS_RUNNING ||
        up_state & UP_CAN_CANCEL_SHUTDOWN_ANIMATION) {
      force_off_on_button_up_ ? RecordPressInTabletModeHistogram(
                                    PowerButtonPressType::kLongPressToShowMenu)
                              : RecordPressInLaptopModeHistogram(
                                    PowerButtonPressType::kLongPressToShowMenu);
    } else if (!(up_state & UP_SHOWING_ANIMATION_CANCELLED)) {
      force_off_on_button_up_
          ? RecordPressInTabletModeHistogram(
                PowerButtonPressType::kLongPressWithoutMenuToShutdown)
          : RecordPressInLaptopModeHistogram(
                PowerButtonPressType::kLongPressWithoutMenuToShutdown);
    }
  }
}

}  // namespace ash
