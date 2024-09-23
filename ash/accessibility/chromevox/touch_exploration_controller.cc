// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox/touch_exploration_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/chromevox/touch_accessibility_enabler.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

#define SET_STATE(state) SetState(state, __func__)
#define VLOG_EVENT(event) \
  if (VLOG_IS_ON(1))      \
  VlogEvent(event, __func__)

namespace ash {

namespace {

// Delay between adjustment sounds.
const int kSoundDelayInMS = 150;

// How long the user must stay in the same anchor point in touch exploration
// before a right-click is triggered.
const base::TimeDelta kLongPressTimerDelay = base::Seconds(5);

void SetTouchAccessibilityFlag(ui::Event* event) {
  // This flag is used to identify mouse move events that were generated from
  // touch exploration in Chrome code.
  event->SetFlags(event->flags() | ui::EF_TOUCH_ACCESSIBILITY);
}

std::unique_ptr<ui::GestureProviderAura> BuildGestureProviderAura(
    TouchExplorationController* owner) {
  // Tune some aspects of gesture detection for ChromeVox.
  ui::GestureProvider::Config config =
      GetGestureProviderConfig(ui::GestureProviderConfigType::CURRENT_PLATFORM);
  config.gesture_detector_config.maximum_swipe_deviation_angle = 45;
  auto gesture_provider =
      std::make_unique<ui::GestureProviderAura>(owner, owner);
  gesture_provider->filtered_gesture_provider().UpdateConfig(config);
  return gesture_provider;
}

}  // namespace

TouchExplorationController::TouchExplorationController(
    aura::Window* root_window,
    TouchExplorationControllerDelegate* delegate,
    base::WeakPtr<TouchAccessibilityEnabler> touch_accessibility_enabler)
    : root_window_(root_window),
      delegate_(delegate),
      state_(NO_FINGERS_DOWN),
      anchor_point_state_(ANCHOR_POINT_NONE),
      gesture_provider_(BuildGestureProviderAura(this)),
      prev_state_(NO_FINGERS_DOWN),
      VLOG_on_(true),
      touch_accessibility_enabler_(touch_accessibility_enabler) {
  DCHECK(root_window);
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);
  if (touch_accessibility_enabler_)
    touch_accessibility_enabler_->RemoveEventHandler();
}

TouchExplorationController::~TouchExplorationController() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
  if (touch_accessibility_enabler_)
    touch_accessibility_enabler_->AddEventHandler();
}

void TouchExplorationController::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point_dip) {
  gfx::Point native_point = anchor_point_dip;
  SetAnchorPointInternal(gfx::PointF(native_point.x(), native_point.y()));
  anchor_point_state_ = ANCHOR_POINT_EXPLICITLY_SET;
}

void TouchExplorationController::SetExcludeBounds(const gfx::Rect& bounds) {
  exclude_bounds_ = bounds;
}

void TouchExplorationController::SetLiftActivationBounds(
    const gfx::Rect& bounds) {
  lift_activation_bounds_ = bounds;
}

ui::EventDispatchDetails TouchExplorationController::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!event.IsTouchEvent()) {
    if (event.IsKeyEvent()) {
      const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
      VLOG(1) << "\nKeyboard event: " << key_event.GetName()
              << "\n Key code: " << key_event.key_code()
              << ", Flags: " << key_event.flags()
              << ", Is char: " << key_event.is_char();
    }
    return SendEvent(continuation, &event);
  }
  const ui::TouchEvent& touch_event = static_cast<const ui::TouchEvent&>(event);

  // Let TouchAccessibilityEnabler process the unrewritten event.
  if (touch_accessibility_enabler_)
    touch_accessibility_enabler_->HandleTouchEvent(touch_event);

  if (event.type() == ui::EventType::kTouchPressed) {
    seen_press_ = true;
  }

  // Touch event comes in un-rotated, screen pixels. Convert it to rotated DIP
  // one.
  gfx::Point location = touch_event.location();
  gfx::Point root_location = touch_event.root_location();
  root_window_->GetHost()->ConvertPixelsToDIP(&location);
  root_window_->GetHost()->ConvertPixelsToDIP(&root_location);

  bool exclude =
      IsTargetedToArcVirtualKeyboard(location) ||
      (!exclude_bounds_.IsEmpty() && exclude_bounds_.Contains(location));
  if (exclude) {
    if (state_ == NO_FINGERS_DOWN)
      return SendEvent(continuation, &event);
    if (touch_event.type() == ui::EventType::kTouchMoved ||
        touch_event.type() == ui::EventType::kTouchPressed) {
      return DiscardEvent(continuation);
    }
    // Otherwise, continue handling events. Basically, we want to let
    // CANCELLED or RELEASE events through so this can get back to
    // the NO_FINGERS_DOWN state.
  }

  // If the tap timer should have fired by now but hasn't, run it now and
  // stop the timer. This is important so that behavior is consistent with
  // the timestamps of the events, and not dependent on the granularity of
  // the timer.
  if (tap_timer_.IsRunning() &&
      touch_event.time_stamp() - most_recent_press_timestamp_ >
          gesture_detector_config_.double_tap_timeout) {
    tap_timer_.Stop();
    OnTapTimerFired();
    // Note: this may change the state. We should now continue and process
    // this event under this new state.
  }

  const ui::EventType type = touch_event.type();
  const int touch_id = touch_event.pointer_details().id;

  // Always update touch ids and touch locations, so we can use those
  // no matter what state we're in.
  if (type == ui::EventType::kTouchPressed) {
    current_touch_ids_.push_back(touch_id);
    touch_locations_.insert(std::pair<int, gfx::PointF>(touch_id, location));
  } else if (type == ui::EventType::kTouchReleased ||
             type == ui::EventType::kTouchCancelled) {
    std::vector<int>::iterator it =
        base::ranges::find(current_touch_ids_, touch_id);

    // Can happen if touch exploration is enabled while fingers were down
    // or if an additional press occurred within the exclusion bounds.
    if (it == current_touch_ids_.end()) {
      // If we get a RELEASE event and we've never seen a PRESS event
      // since TouchExplorationController was instantiated, cancel the
      // event so that touch gestures that enable spoken feedback
      // don't accidentally trigger other behaviors on release.
      if (!seen_press_) {
        ui::TouchEvent new_event(ui::EventType::kTouchCancelled, gfx::Point(),
                                 touch_event.time_stamp(),
                                 touch_event.pointer_details());
        new_event.set_location(location);
        new_event.set_root_location(root_location);
        new_event.SetFlags(touch_event.flags());
        return SendEventFinally(continuation, &new_event);
      }

      // Otherwise just pass it through.
      return SendEvent(continuation, &event);
    }

    current_touch_ids_.erase(it);
    touch_locations_.erase(touch_id);
  } else if (type == ui::EventType::kTouchMoved) {
    std::vector<int>::iterator it =
        base::ranges::find(current_touch_ids_, touch_id);

    // Can happen if touch exploration is enabled while fingers were down.
    if (it == current_touch_ids_.end())
      return SendEvent(continuation, &event);

    touch_locations_[*it] = gfx::PointF(location);
  } else {
    NOTREACHED() << "Unexpected event type received: " << event.GetName();
  }
  VLOG_EVENT(touch_event);

  // In order to avoid accidentally double tapping when moving off the edge
  // of the screen, the state will be rewritten to NoFingersDown.
  if ((type == ui::EventType::kTouchReleased ||
       type == ui::EventType::kTouchCancelled) &&
      FindEdgesWithinInset(location, kLeavingScreenEdge) != NO_EDGE) {
    if (VLOG_on_)
      VLOG(1) << "Leaving screen";

    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
      if (VLOG_on_) {
        VLOG(1) << "Reset to no fingers in Rewrite event because the touch  "
                   "release or cancel was on the edge of the screen.";
      }
      return DiscardEvent(continuation);
    }
  }

  // We now need a TouchEvent that has its coordinates mapped into root window
  // DIP.
  ui::TouchEvent touch_event_dip = touch_event;
  touch_event_dip.set_location(location);

  // If the user is in a gesture state, or if there is a possiblity that the
  // user will enter it in the future, we send the event to the gesture
  // provider so it can keep track of the state of the fingers. When the user
  // leaves one of these states, SET_STATE will set the gesture provider to
  // NULL.
  if (gesture_provider_.get()) {
    if (gesture_provider_->OnTouchEvent(&touch_event_dip)) {
      gesture_provider_->OnTouchEventAck(
          touch_event_dip.unique_event_id(), false /* event_consumed */,
          false /* is_source_touch_event_set_blocking */);
    }
    ProcessGestureEvents();
  }

  // The rest of the processing depends on what state we're in.
  switch (state_) {
    case NO_FINGERS_DOWN:
      return InNoFingersDown(touch_event_dip, continuation);
    case SINGLE_TAP_PRESSED:
      return InSingleTapPressed(touch_event_dip, continuation);
    case SINGLE_TAP_RELEASED:
    case TOUCH_EXPLORE_RELEASED:
      return InSingleTapOrTouchExploreReleased(touch_event_dip, continuation);
    case DOUBLE_TAP_PENDING:
      return InDoubleTapPending(touch_event_dip, continuation);
    case TOUCH_RELEASE_PENDING:
      return InTouchReleasePending(touch_event_dip, continuation);
    case TOUCH_EXPLORATION:
      return InTouchExploration(touch_event_dip, continuation);
    case GESTURE_IN_PROGRESS:
      return InGestureInProgress(touch_event_dip, continuation);
    case TOUCH_EXPLORE_SECOND_PRESS:
      return InTouchExploreSecondPress(touch_event_dip, continuation);
    case TOUCH_EXPLORE_LONG_PRESS:
      return InTouchExploreLongPress(touch_event_dip, continuation);
    case SLIDE_GESTURE:
      return InSlideGesture(touch_event_dip, continuation);
    case ONE_FINGER_PASSTHROUGH:
      return InOneFingerPassthrough(touch_event_dip, continuation);
    case WAIT_FOR_NO_FINGERS:
      return InWaitForNoFingers(touch_event_dip, continuation);
    case TWO_FINGER_TAP:
      return InTwoFingerTap(touch_event_dip, continuation);
  }
  NOTREACHED();
}

ui::EventDispatchDetails TouchExplorationController::InNoFingersDown(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  const ui::EventType type = event.type();
  if (type != ui::EventType::kTouchPressed) {
    NOTREACHED() << "Unexpected event type received: " << event.GetName();
  }

  initial_press_ = std::make_unique<ui::TouchEvent>(event);
  initial_press_continuation_ = continuation;
  most_recent_press_timestamp_ = initial_press_->time_stamp();
  initial_presses_[event.pointer_details().id] = event.location();
  last_unused_finger_event_ = std::make_unique<ui::TouchEvent>(event);
  last_unused_finger_continuation_ = continuation;
  StartTapTimer();
  SET_STATE(SINGLE_TAP_PRESSED);
  return DiscardEvent(continuation);
}

ui::EventDispatchDetails TouchExplorationController::InSingleTapPressed(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  const ui::EventType type = event.type();
  if (type == ui::EventType::kTouchPressed) {
    initial_presses_[event.pointer_details().id] = event.location();
    SET_STATE(TWO_FINGER_TAP);
    return DiscardEvent(continuation);
  }
  if (type == ui::EventType::kTouchReleased ||
      type == ui::EventType::kTouchCancelled) {
    if (current_touch_ids_.size() == 0 &&
        event.pointer_details().id == initial_press_->pointer_details().id) {
      MaybeSendSimulatedTapInLiftActivationBounds(event, continuation);
      SET_STATE(SINGLE_TAP_RELEASED);
    } else if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return DiscardEvent(continuation);
  }
  if (type == ui::EventType::kTouchMoved) {
    float distance = (event.location() - initial_press_->location()).Length();
    // If the user does not move far enough from the original position, then the
    // resulting movement should not be considered to be a deliberate gesture or
    // touch exploration.
    if (distance <= gesture_detector_config_.touch_slop)
      return DiscardEvent(continuation);

    float delta_time =
        (event.time_stamp() - most_recent_press_timestamp_).InSecondsF();
    float velocity = distance / delta_time;
    if (VLOG_on_) {
      VLOG(1) << "\n Delta time: " << delta_time << "\n Distance: " << distance
              << "\n Velocity of click: " << velocity
              << "\n Minimum swipe velocity: "
              << gesture_detector_config_.minimum_swipe_velocity;
    }
    // Change to slide gesture if the slide occurred at the right edge.
    if (ShouldEnableVolumeSlideGesture(event)) {
      SET_STATE(SLIDE_GESTURE);
      return InSlideGesture(event, continuation);
    }

    // If the user moves fast enough from the initial touch location, start
    // gesture detection. Otherwise, jump to the touch exploration mode early.
    if (velocity > gesture_detector_config_.minimum_swipe_velocity) {
      SET_STATE(GESTURE_IN_PROGRESS);
      return InGestureInProgress(event, continuation);
    }
    anchor_point_state_ = ANCHOR_POINT_FROM_TOUCH_EXPLORATION;
    EnterTouchToMouseMode();
    SET_STATE(TOUCH_EXPLORATION);
    return InTouchExploration(event, continuation);
  }
  NOTREACHED();
}

ui::EventDispatchDetails
TouchExplorationController::InSingleTapOrTouchExploreReleased(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  const ui::EventType type = event.type();
  // If there is more than one finger down, then discard to wait until no
  // fingers are down.
  if (current_touch_ids_.size() > 1) {
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return DiscardEvent(continuation);
  }
  if (type == ui::EventType::kTouchPressed) {
    // If there is no anchor point for synthesized events because the
    // user hasn't touch-explored or focused anything yet, we can't
    // send a click, so discard.
    if (anchor_point_state_ == ANCHOR_POINT_NONE) {
      tap_timer_.Stop();
      return DiscardEvent(continuation);
    }
    // This is the second tap in a double-tap (or double tap-hold).
    // We set the tap timer. If it fires before the user lifts their finger,
    // one-finger passthrough begins. Otherwise, there is a touch press and
    // release at the location of the last touch exploration.
    SET_STATE(DOUBLE_TAP_PENDING);
    // The old tap timer (from the initial click) is stopped if it is still
    // going, and the new one is set.
    tap_timer_.Stop();
    StartTapTimer();
    most_recent_press_timestamp_ = event.time_stamp();
    // This will update as the finger moves before a possible passthrough, and
    // will determine the offset.
    last_unused_finger_event_ = std::make_unique<ui::TouchEvent>(event);
    last_unused_finger_continuation_ = continuation;
    return DiscardEvent(continuation);
  }
  if (type == ui::EventType::kTouchReleased &&
      anchor_point_state_ == ANCHOR_POINT_NONE) {
    // If the previous press was discarded, we need to also handle its
    // release.
    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return DiscardEvent(continuation);
  }
  if (type == ui::EventType::kTouchMoved) {
    return DiscardEvent(continuation);
  }
  DUMP_WILL_BE_NOTREACHED();
  return SendEvent(continuation, &event);
}

ui::EventDispatchDetails TouchExplorationController::InDoubleTapPending(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  const ui::EventType type = event.type();
  if (type == ui::EventType::kTouchPressed) {
    return DiscardEvent(continuation);
  }
  if (type == ui::EventType::kTouchMoved) {
    // If the user moves far enough from the initial touch location (outside
    // the "slop" region, jump to passthrough mode early.
    float delta = (event.location() - initial_press_->location()).Length();
    if (delta > gesture_detector_config_.double_tap_slop) {
      tap_timer_.Stop();
      OnTapTimerFired();
    }
    return DiscardEvent(continuation);
  } else if (type == ui::EventType::kTouchReleased ||
             type == ui::EventType::kTouchCancelled) {
    if (current_touch_ids_.size() != 0)
      return DiscardEvent(continuation);

    SendSimulatedClick();

    SET_STATE(NO_FINGERS_DOWN);
    return DiscardEvent(continuation);
  }
  NOTREACHED();
}

ui::EventDispatchDetails TouchExplorationController::InTouchReleasePending(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  const ui::EventType type = event.type();
  if (type == ui::EventType::kTouchPressed ||
      type == ui::EventType::kTouchMoved) {
    return DiscardEvent(continuation);
  } else if (type == ui::EventType::kTouchReleased ||
             type == ui::EventType::kTouchCancelled) {
    if (current_touch_ids_.size() != 0)
      return DiscardEvent(continuation);

    SendSimulatedClick();
    SET_STATE(NO_FINGERS_DOWN);
    return DiscardEvent(continuation);
  }
  NOTREACHED();
}

ui::EventDispatchDetails TouchExplorationController::InTouchExploration(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  const ui::EventType type = event.type();
  if (type == ui::EventType::kTouchPressed) {
    // Enter split-tap mode.
    initial_press_ = std::make_unique<ui::TouchEvent>(event);
    initial_press_continuation_ = continuation;
    tap_timer_.Stop();
    SET_STATE(TOUCH_EXPLORE_SECOND_PRESS);
    return DiscardEvent(continuation);
  } else if (type == ui::EventType::kTouchReleased ||
             type == ui::EventType::kTouchCancelled) {
    initial_press_ = std::make_unique<ui::TouchEvent>(event);
    initial_press_continuation_ = continuation;
    StartTapTimer();
    most_recent_press_timestamp_ = event.time_stamp();
    MaybeSendSimulatedTapInLiftActivationBounds(event, continuation);
    SET_STATE(TOUCH_EXPLORE_RELEASED);
  } else if (type != ui::EventType::kTouchMoved) {
    NOTREACHED();
  }

  // |location| is in window DIP coordinates.
  gfx::PointF location(event.location());

  // APIs taking this point e.g.
  // chrome.accessibilityPrivate.sendSyntheticMouseEvent,
  // chrome.automation.AutomationNode.prototype.hitTest, all take screen
  // coordinates.
  ::wm::ConvertPointToScreen(root_window_, &location);
  delegate_->HandleAccessibilityGesture(ax::mojom::Gesture::kTouchExplore,
                                        location);

  last_touch_exploration_ = std::make_unique<ui::TouchEvent>(event);
  if (anchor_point_state_ != ANCHOR_POINT_EXPLICITLY_SET)
    SetAnchorPointInternal(last_touch_exploration_->location_f());

  return DiscardEvent(continuation);
}

ui::EventDispatchDetails TouchExplorationController::InGestureInProgress(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  // The events were sent to the gesture provider in RewriteEvent already.
  // If no gesture is registered before the tap timer times out, the state
  // will change to "wait for no fingers down" or "touch exploration" depending
  // on the number of fingers down, and this function will stop being called.
  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return DiscardEvent(continuation);
}

ui::EventDispatchDetails TouchExplorationController::InOneFingerPassthrough(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  if (event.pointer_details().id != initial_press_->pointer_details().id) {
    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return DiscardEvent(continuation);
  }
  // |event| locations are in DIP; see |RewriteEvent|. We need to dispatch
  // screen coordinates.
  gfx::PointF location_f(
      ConvertDIPToPixels(event.location_f() - passthrough_offset_));
  ui::TouchEvent new_event(event.type(), gfx::Point(), event.time_stamp(),
                           event.pointer_details(), event.flags());
  new_event.set_location_f(location_f);
  new_event.set_root_location_f(location_f);
  SetTouchAccessibilityFlag(&new_event);
  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return SendEventFinally(continuation, &new_event);
}

ui::EventDispatchDetails TouchExplorationController::InTouchExploreSecondPress(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  ui::EventType type = event.type();
  if (type == ui::EventType::kTouchPressed) {
    // A third finger being pressed means that a split tap can no longer go
    // through. The user enters the wait state, Since there has already been
    // a press dispatched when split tap began, the touch needs to be
    // cancelled.
    ui::TouchEvent new_event(ui::EventType::kTouchCancelled, gfx::Point(),
                             event.time_stamp(),
                             initial_press_->pointer_details(), event.flags());
    // |event| locations are in DIP; see |RewriteEvent|. We need to dispatch
    // screen coordinates.
    gfx::PointF location_f(ConvertDIPToPixels(anchor_point_dip_));
    new_event.set_location_f(location_f);
    new_event.set_root_location_f(location_f);
    SetTouchAccessibilityFlag(&new_event);
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return SendEventFinally(continuation, &new_event);
  }
  if (type == ui::EventType::kTouchMoved) {
    // If the fingers have moved too far from their original locations,
    // the user can no longer split tap.
    ui::TouchEvent* original_touch;
    if (event.pointer_details().id ==
        last_touch_exploration_->pointer_details().id) {
      original_touch = last_touch_exploration_.get();
    } else if (event.pointer_details().id ==
               initial_press_->pointer_details().id) {
      original_touch = initial_press_.get();
    } else {
      NOTREACHED();
    }
    // Check the distance between the current finger location and the original
    // location. The slop for this is a bit more generous since keeping two
    // fingers in place is a bit harder. If the user has left the slop, the
    // user enters the wait state.
    if ((event.location_f() - original_touch->location_f()).Length() >
        GetSplitTapTouchSlop()) {
      SET_STATE(WAIT_FOR_NO_FINGERS);
    }
    return DiscardEvent(continuation);
  }
  if (type == ui::EventType::kTouchReleased ||
      type == ui::EventType::kTouchCancelled) {
    // If the touch exploration finger is lifted, there is no option to return
    // to touch explore anymore. The remaining finger acts as a pending
    // tap or long tap for the last touch explore location.
    if (event.pointer_details().id ==
        last_touch_exploration_->pointer_details().id) {
      SET_STATE(TOUCH_RELEASE_PENDING);
      return DiscardEvent(continuation);
    }

    // Continue to release the touch only if the touch explore finger is the
    // only finger remaining.
    if (current_touch_ids_.size() != 1) {
      return DiscardEvent(continuation);
    }

    SendSimulatedClick();

    SET_STATE(TOUCH_EXPLORATION);
    EnterTouchToMouseMode();
    return DiscardEvent(continuation);
  }
  NOTREACHED();
}

ui::EventDispatchDetails TouchExplorationController::InTouchExploreLongPress(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  // Simulate a right mouse click.
  auto mouse_pressed = std::make_unique<ui::MouseEvent>(
      ui::EventType::kMousePressed, anchor_point_dip_, anchor_point_dip_, Now(),
      ui::EF_IS_SYNTHESIZED | ui::EF_TOUCH_ACCESSIBILITY |
          ui::EF_RIGHT_MOUSE_BUTTON,
      ui::EF_RIGHT_MOUSE_BUTTON);
  auto mouse_released = std::make_unique<ui::MouseEvent>(
      ui::EventType::kMouseReleased, anchor_point_dip_, anchor_point_dip_,
      Now(), ui::EF_IS_SYNTHESIZED | ui::EF_TOUCH_ACCESSIBILITY,
      ui::EF_LEFT_MOUSE_BUTTON);

  DispatchEvent(mouse_pressed.get(), continuation);
  DispatchEvent(mouse_released.get(), continuation);

  SET_STATE(TOUCH_EXPLORATION);
  EnterTouchToMouseMode();
  return InTouchExploration(event, continuation);
}

ui::EventDispatchDetails TouchExplorationController::InWaitForNoFingers(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  if (current_touch_ids_.size() == 0)
    SET_STATE(NO_FINGERS_DOWN);
  return DiscardEvent(continuation);
}

void TouchExplorationController::PlaySoundForTimer() {
  delegate_->PlayVolumeAdjustEarcon();
}

void TouchExplorationController::SendSimulatedClick() {
  // Always send a double tap gesture to ChromeVox.
  delegate_->HandleAccessibilityGesture(ax::mojom::Gesture::kClick);
}

void TouchExplorationController::SendSimulatedTap(
    const Continuation continuation) {
  auto touch_press = std::make_unique<ui::TouchEvent>(
      ui::EventType::kTouchPressed, gfx::Point(), Now(),
      initial_press_->pointer_details());
  touch_press->set_location_f(anchor_point_dip_);
  touch_press->set_root_location_f(anchor_point_dip_);
  DispatchEvent(touch_press.get(), continuation);

  auto touch_release = std::make_unique<ui::TouchEvent>(
      ui::EventType::kTouchReleased, gfx::Point(), Now(),
      initial_press_->pointer_details());
  touch_release->set_location_f(anchor_point_dip_);
  touch_release->set_root_location_f(anchor_point_dip_);
  DispatchEvent(touch_release.get(), continuation);
}

void TouchExplorationController::MaybeSendSimulatedTapInLiftActivationBounds(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  gfx::Point location = event.location();
  gfx::Point anchor_location(anchor_point_dip_.x(), anchor_point_dip_.y());
  if (lift_activation_bounds_.Contains(anchor_location.x(),
                                       anchor_location.y()) &&
      lift_activation_bounds_.Contains(location)) {
    delegate_->PlayTouchTypeEarcon();
    SendSimulatedTap(continuation);
  }
}

ui::EventDispatchDetails TouchExplorationController::InSlideGesture(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  // The timer should not fire when sliding.
  tap_timer_.Stop();

  ui::EventType type = event.type();
  // If additional fingers are added before a swipe gesture has been registered,
  // then wait until all fingers have been lifted.
  if (type == ui::EventType::kTouchPressed ||
      event.pointer_details().id != initial_press_->pointer_details().id) {
    if (sound_timer_.IsRunning())
      sound_timer_.Stop();
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return DiscardEvent(continuation);
  }

  // There should not be more than one finger down.
  DCHECK_LE(current_touch_ids_.size(), 1U);

  // Allows user to return to the edge to adjust the sound if they have left the
  // boundaries.
  int edge = FindEdgesWithinInset(event.location(), kSlopDistanceFromEdge);
  if (!(edge & RIGHT_EDGE) && (type != ui::EventType::kTouchReleased)) {
    if (sound_timer_.IsRunning()) {
      sound_timer_.Stop();
    }
    return DiscardEvent(continuation);
  }

  // This can occur if the user leaves the screen edge and then returns to it to
  // continue adjusting the sound.
  if (!sound_timer_.IsRunning()) {
    sound_timer_.Start(FROM_HERE, base::Milliseconds(kSoundDelayInMS), this,
                       &TouchExplorationController::PlaySoundForTimer);
    delegate_->PlayVolumeAdjustEarcon();
  }

  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return DiscardEvent(continuation);
}

ui::EventDispatchDetails TouchExplorationController::InTwoFingerTap(
    const ui::TouchEvent& event,
    const Continuation continuation) {
  ui::EventType type = event.type();
  if (type == ui::EventType::kTouchPressed) {
    // This is now a three finger gesture.
    SET_STATE(GESTURE_IN_PROGRESS);
    return DiscardEvent(continuation);
  }

  if (type == ui::EventType::kTouchMoved) {
    // Determine if it was a swipe.
    gfx::Point original_location = initial_presses_[event.pointer_details().id];
    float distance = (event.location() - original_location).Length();
    // If the user moves too far from the original position, consider the
    // movement a swipe.
    if (distance > gesture_detector_config_.touch_slop) {
      SET_STATE(GESTURE_IN_PROGRESS);
    }
    return DiscardEvent(continuation);
  }

  if (current_touch_ids_.size() != 0) {
    return DiscardEvent(continuation);
  }

  if (type == ui::EventType::kTouchReleased) {
    delegate_->HandleAccessibilityGesture(ax::mojom::Gesture::kTap2);
    SET_STATE(NO_FINGERS_DOWN);
    return DiscardEvent(continuation);
  }
  return DiscardEvent(continuation);
}

base::TimeTicks TouchExplorationController::Now() {
  return ui::EventTimeForNow();
}

void TouchExplorationController::StartTapTimer() {
  tap_timer_.Start(FROM_HERE, gesture_detector_config_.double_tap_timeout, this,
                   &TouchExplorationController::OnTapTimerFired);
}

void TouchExplorationController::OnTapTimerFired() {
  switch (state_) {
    case SINGLE_TAP_RELEASED:
      SET_STATE(NO_FINGERS_DOWN);
      break;
    case TOUCH_EXPLORE_RELEASED:
      SET_STATE(NO_FINGERS_DOWN);
      last_touch_exploration_ =
          std::make_unique<ui::TouchEvent>(*initial_press_);
      SetAnchorPointInternal(last_touch_exploration_->location_f());
      anchor_point_state_ = ANCHOR_POINT_FROM_TOUCH_EXPLORATION;
      return;
    case DOUBLE_TAP_PENDING: {
      SET_STATE(ONE_FINGER_PASSTHROUGH);
      delegate_->PlayPassthroughEarcon();
      passthrough_offset_ =
          last_unused_finger_event_->location_f() - anchor_point_dip_;
      std::unique_ptr<ui::TouchEvent> passthrough_press(
          new ui::TouchEvent(ui::EventType::kTouchPressed, gfx::Point(), Now(),
                             last_unused_finger_event_->pointer_details()));
      passthrough_press->set_location_f(anchor_point_dip_);
      passthrough_press->set_root_location_f(anchor_point_dip_);
      DispatchEvent(passthrough_press.get(), last_unused_finger_continuation_);
      return;
    }
    case SINGLE_TAP_PRESSED:
      [[fallthrough]];
    case GESTURE_IN_PROGRESS:
      // If only one finger is down, go into touch exploration.
      if (current_touch_ids_.size() == 1) {
        anchor_point_state_ = ANCHOR_POINT_FROM_TOUCH_EXPLORATION;
        EnterTouchToMouseMode();
        SET_STATE(TOUCH_EXPLORATION);
        break;
      }
      // Otherwise wait for all fingers to be lifted.
      SET_STATE(WAIT_FOR_NO_FINGERS);
      return;
    case TWO_FINGER_TAP:
      SET_STATE(WAIT_FOR_NO_FINGERS);
      break;
    default:
      return;
  }
  EnterTouchToMouseMode();
  delegate_->HandleAccessibilityGesture(ax::mojom::Gesture::kTouchExplore,
                                        initial_press_->location_f());
  last_touch_exploration_ = std::make_unique<ui::TouchEvent>(*initial_press_);
  SetAnchorPointInternal(last_touch_exploration_->location_f());
  anchor_point_state_ = ANCHOR_POINT_FROM_TOUCH_EXPLORATION;
}

void TouchExplorationController::ResetLiftActivationLongPressTimer() {
  long_press_timer_.Stop();
  if (state_ == TOUCH_EXPLORATION &&
      lift_activation_bounds_.Contains(
          gfx::ToFlooredPoint(anchor_point_dip_))) {
    long_press_timer_.Start(
        FROM_HERE, kLongPressTimerDelay, this,
        &TouchExplorationController::OnLiftActivationLongPressTimerFired);
  }
}

void TouchExplorationController::OnLiftActivationLongPressTimerFired() {
  if (state_ == TOUCH_EXPLORATION &&
      lift_activation_bounds_.Contains(
          gfx::ToFlooredPoint(anchor_point_dip_))) {
    delegate_->PlayLongPressRightClickEarcon();
    SET_STATE(TOUCH_EXPLORE_LONG_PRESS);
  }
}

void TouchExplorationController::DispatchEvent(
    ui::Event* event,
    const Continuation continuation) {
  SetTouchAccessibilityFlag(event);
  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF screen_point(ConvertDIPToPixels(located_event->location_f()));
    located_event->set_location_f(screen_point);
    located_event->set_root_location_f(screen_point);
  }
  if (SendEventFinally(continuation, event).dispatcher_destroyed)
    VLOG(0) << "Undispatched event due to destroyed dispatcher.";
}

// This is an override for a function that is only called for timer-based events
// like long press. Events that are created synchronously as a result of
// certain touch events are added to the vector accessible via
// GetAndResetPendingGestures(). We only care about swipes (which are created
// synchronously), so we ignore this callback.
void TouchExplorationController::OnGestureEvent(ui::GestureConsumer* consumer,
                                                ui::GestureEvent* gesture) {}

const std::string& TouchExplorationController::GetName() const {
  static const std::string name("TouchExplorationController");
  return name;
}

void TouchExplorationController::ProcessGestureEvents() {
  std::vector<std::unique_ptr<ui::GestureEvent>> gestures =
      gesture_provider_->GetAndResetPendingGestures();
  bool resolved_gesture = false;
  max_gesture_touch_points_ =
      std::max(max_gesture_touch_points_, current_touch_ids_.size());
  for (const auto& gesture : gestures) {
    if (gesture->type() == ui::EventType::kGestureSwipe &&
        state_ == GESTURE_IN_PROGRESS) {
      OnSwipeEvent(gesture.get());
      // The tap timer to leave gesture state is ended, and we now wait for
      // all fingers to be released.
      tap_timer_.Stop();
      SET_STATE(WAIT_FOR_NO_FINGERS);
      return;
    }
    if (state_ == SLIDE_GESTURE && gesture->IsScrollGestureEvent()) {
      SideSlideControl(gesture.get());
      resolved_gesture = true;
    }
  }

  if (resolved_gesture)
    return;

  if (current_touch_ids_.size() == 0) {
    switch (max_gesture_touch_points_) {
      case 3:
        delegate_->HandleAccessibilityGesture(ax::mojom::Gesture::kTap3);
        break;
      case 4:
        delegate_->HandleAccessibilityGesture(ax::mojom::Gesture::kTap4);
        break;
      default:
        break;
    }
    max_gesture_touch_points_ = 0;
  }
}

void TouchExplorationController::SideSlideControl(ui::GestureEvent* gesture) {
  ui::EventType type = gesture->type();

  if (type == ui::EventType::kGestureScrollBegin) {
    delegate_->PlayVolumeAdjustEarcon();
  }

  if (type == ui::EventType::kGestureScrollEnd) {
    if (sound_timer_.IsRunning())
      sound_timer_.Stop();
    delegate_->PlayVolumeAdjustEarcon();
  }

  // If the user is in the corner of the right side of the screen, the volume
  // will be automatically set to 100% or muted depending on which corner they
  // are in. Otherwise, the user will be able to adjust the volume by sliding
  // their finger along the right side of the screen. Volume is relative to
  // where they are on the right side of the screen.
  gfx::Point location = gesture->location();
  int edge = FindEdgesWithinInset(location, kSlopDistanceFromEdge);
  if (!(edge & RIGHT_EDGE))
    return;

  if (edge & TOP_EDGE) {
    delegate_->SetOutputLevel(100);
    return;
  }
  if (edge & BOTTOM_EDGE) {
    delegate_->SetOutputLevel(0);
    return;
  }

  location = gesture->location();
  gfx::RectF bounds(root_window_->bounds());
  float volume_adjust_height = bounds.height() - 2 * kMaxDistanceFromEdge;
  float ratio = (location.y() - kMaxDistanceFromEdge) / volume_adjust_height;
  float volume = 100 - 100 * ratio;
  if (VLOG_on_) {
    VLOG(1) << "\n Volume = " << volume
            << "\n Location = " << location.ToString()
            << "\n Bounds = " << bounds.right();
  }
  delegate_->SetOutputLevel(static_cast<int>(volume));
}

void TouchExplorationController::OnSwipeEvent(ui::GestureEvent* swipe_gesture) {
  // A swipe gesture contains details for the direction in which the swipe
  // occurred. TODO(evy) : Research which swipe results users most want and
  // remap these swipes to the best events. Hopefully in the near future
  // there will also be a menu for users to pick custom mappings.
  ui::GestureEventDetails event_details = swipe_gesture->details();
  int num_fingers = event_details.touch_points();
  if (VLOG_on_)
    VLOG(1) << "\nSwipe with " << num_fingers << " fingers.";

  ax::mojom::Gesture gesture = ax::mojom::Gesture::kNone;
  if (event_details.swipe_left()) {
    switch (num_fingers) {
      case 1:
        gesture = ax::mojom::Gesture::kSwipeLeft1;
        break;
      case 2:
        gesture = ax::mojom::Gesture::kSwipeLeft2;
        break;
      case 3:
        gesture = ax::mojom::Gesture::kSwipeLeft3;
        break;
      case 4:
        gesture = ax::mojom::Gesture::kSwipeLeft4;
        break;
      default:
        break;
    }
  } else if (event_details.swipe_up()) {
    switch (num_fingers) {
      case 1:
        gesture = ax::mojom::Gesture::kSwipeUp1;
        break;
      case 2:
        gesture = ax::mojom::Gesture::kSwipeUp2;
        break;
      case 3:
        gesture = ax::mojom::Gesture::kSwipeUp3;
        break;
      case 4:
        gesture = ax::mojom::Gesture::kSwipeUp4;
        break;
      default:
        break;
    }
  } else if (event_details.swipe_right()) {
    switch (num_fingers) {
      case 1:
        gesture = ax::mojom::Gesture::kSwipeRight1;
        break;
      case 2:
        gesture = ax::mojom::Gesture::kSwipeRight2;
        break;
      case 3:
        gesture = ax::mojom::Gesture::kSwipeRight3;
        break;
      case 4:
        gesture = ax::mojom::Gesture::kSwipeRight4;
        break;
      default:
        break;
    }
  } else if (event_details.swipe_down()) {
    switch (num_fingers) {
      case 1:
        gesture = ax::mojom::Gesture::kSwipeDown1;
        break;
      case 2:
        gesture = ax::mojom::Gesture::kSwipeDown2;
        break;
      case 3:
        gesture = ax::mojom::Gesture::kSwipeDown3;
        break;
      case 4:
        gesture = ax::mojom::Gesture::kSwipeDown4;
        break;
      default:
        break;
    }
  }

  if (gesture != ax::mojom::Gesture::kNone)
    delegate_->HandleAccessibilityGesture(gesture);
}

int TouchExplorationController::FindEdgesWithinInset(gfx::Point point_dip,
                                                     float inset) {
  gfx::RectF inner_bounds_dip(root_window_->bounds());
  inner_bounds_dip.Inset(inset);

  // Bitwise manipulation in order to determine where on the screen the point
  // lies. If more than one bit is turned on, then it is a corner where the two
  // bit/edges intersect. Otherwise, if no bits are turned on, the point must be
  // in the center of the screen.
  int result = NO_EDGE;
  if (point_dip.x() < inner_bounds_dip.x())
    result |= LEFT_EDGE;
  if (point_dip.x() > inner_bounds_dip.right())
    result |= RIGHT_EDGE;
  if (point_dip.y() < inner_bounds_dip.y())
    result |= TOP_EDGE;
  if (point_dip.y() > inner_bounds_dip.bottom())
    result |= BOTTOM_EDGE;
  return result;
}

void TouchExplorationController::DispatchKeyWithFlags(
    const ui::KeyboardCode key,
    int flags,
    const Continuation continuation) {
  ui::KeyEvent key_down(ui::EventType::kKeyPressed, key, flags);
  ui::KeyEvent key_up(ui::EventType::kKeyReleased, key, flags);
  DispatchEvent(&key_down, continuation);
  DispatchEvent(&key_up, continuation);
  if (VLOG_on_) {
    VLOG(1) << "\nKey down: key code : " << key_down.key_code()
            << ", flags: " << key_down.flags()
            << "\nKey up: key code : " << key_up.key_code()
            << ", flags: " << key_up.flags();
  }
}

void TouchExplorationController::EnterTouchToMouseMode() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  if (cursor_client && !cursor_client->IsMouseEventsEnabled())
    cursor_client->EnableMouseEvents();
  if (cursor_client && cursor_client->IsCursorVisible())
    cursor_client->HideCursor();
}

void TouchExplorationController::SetState(State new_state,
                                          const char* function_name) {
  state_ = new_state;
  VlogState(function_name);
  // These are the states the user can be in that will never result in a
  // gesture before the user returns to NO_FINGERS_DOWN. Therefore, if the
  // gesture provider still exists, it's reset to NULL until the user returns
  // to NO_FINGERS_DOWN.
  switch (new_state) {
    case SINGLE_TAP_RELEASED:
    case TOUCH_EXPLORE_RELEASED:
    case DOUBLE_TAP_PENDING:
    case TOUCH_RELEASE_PENDING:
    case TOUCH_EXPLORATION:
    case TOUCH_EXPLORE_SECOND_PRESS:
    case TOUCH_EXPLORE_LONG_PRESS:
    case ONE_FINGER_PASSTHROUGH:
    case WAIT_FOR_NO_FINGERS:
      if (gesture_provider_.get())
        gesture_provider_.reset(NULL);
      max_gesture_touch_points_ = 0;
      break;
    case NO_FINGERS_DOWN:
      gesture_provider_ = BuildGestureProviderAura(this);
      if (sound_timer_.IsRunning())
        sound_timer_.Stop();
      tap_timer_.Stop();
      break;
    case SINGLE_TAP_PRESSED:
    case GESTURE_IN_PROGRESS:
    case SLIDE_GESTURE:
    case TWO_FINGER_TAP:
      break;
  }
}

void TouchExplorationController::VlogState(const char* function_name) {
  if (!VLOG_on_)
    return;
  if (prev_state_ == state_)
    return;
  prev_state_ = state_;
  const char* state_string = EnumStateToString(state_);
  VLOG(1) << "\n Function name: " << function_name
          << "\n State: " << state_string;
}

void TouchExplorationController::VlogEvent(const ui::TouchEvent& touch_event,
                                           const char* function_name) {
  if (!VLOG_on_)
    return;

  if (prev_event_ && prev_event_->type() == touch_event.type() &&
      prev_event_->pointer_details().id == touch_event.pointer_details().id) {
    return;
  }
  // The above statement prevents events of the same type and id from being
  // printed in a row. However, if two fingers are down, they would both be
  // moving and alternating printing move events unless we check for this.
  if (prev_event_ && prev_event_->type() == ui::EventType::kTouchMoved &&
      touch_event.type() == ui::EventType::kTouchMoved) {
    return;
  }

  const std::string& type = touch_event.GetName();
  const gfx::PointF& location = touch_event.location_f();
  const int touch_id = touch_event.pointer_details().id;

  VLOG(1) << "\n Function name: " << function_name << "\n Event Type: " << type
          << "\n Location: " << location.ToString()
          << "\n Touch ID: " << touch_id;
  prev_event_ = std::make_unique<ui::TouchEvent>(touch_event);
}

const char* TouchExplorationController::EnumStateToString(State state) {
  switch (state) {
    case NO_FINGERS_DOWN:
      return "NO_FINGERS_DOWN";
    case SINGLE_TAP_PRESSED:
      return "SINGLE_TAP_PRESSED";
    case SINGLE_TAP_RELEASED:
      return "SINGLE_TAP_RELEASED";
    case TOUCH_EXPLORE_RELEASED:
      return "TOUCH_EXPLORE_RELEASED";
    case DOUBLE_TAP_PENDING:
      return "DOUBLE_TAP_PENDING";
    case TOUCH_RELEASE_PENDING:
      return "TOUCH_RELEASE_PENDING";
    case TOUCH_EXPLORATION:
      return "TOUCH_EXPLORATION";
    case GESTURE_IN_PROGRESS:
      return "GESTURE_IN_PROGRESS";
    case TOUCH_EXPLORE_SECOND_PRESS:
      return "TOUCH_EXPLORE_SECOND_PRESS";
    case TOUCH_EXPLORE_LONG_PRESS:
      return "TOUCH_EXPLORE_LONG_PRESS";
    case SLIDE_GESTURE:
      return "SLIDE_GESTURE";
    case ONE_FINGER_PASSTHROUGH:
      return "ONE_FINGER_PASSTHROUGH";
    case WAIT_FOR_NO_FINGERS:
      return "WAIT_FOR_NO_FINGERS";
    case TWO_FINGER_TAP:
      return "TWO_FINGER_TAP";
  }
  return "Not a state";
}

void TouchExplorationController::SetAnchorPointInternal(
    const gfx::PointF& anchor_point) {
  anchor_point_dip_ = anchor_point;
  ResetLiftActivationLongPressTimer();
}

float TouchExplorationController::GetSplitTapTouchSlop() {
  return gesture_detector_config_.touch_slop * 3;
}

gfx::PointF TouchExplorationController::ConvertDIPToPixels(
    const gfx::PointF& location_f) {
  gfx::Point location(gfx::ToFlooredPoint(location_f));
  root_window_->GetHost()->ConvertDIPToPixels(&location);
  return gfx::PointF(location);
}

bool TouchExplorationController::IsTargetedToArcVirtualKeyboard(
    const gfx::Point& location) {
  // Copy event here as WindowTargeter::FindTargetForEvent modify touch event.
  ui::TouchEvent event(ui::EventType::kTouchMoved, gfx::Point(), Now(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  event.set_location(location);

  // It's safe to static cast to aura::Window here. As current implementation of
  // WindowTargeter::FindTargetForEvent only returns aura::Window.
  aura::Window* target = static_cast<aura::Window*>(
      root_window_->targeter()->FindTargetForEvent(root_window_, &event));

  aura::Window* container = GetContainerForWindow(target);
  if (!container)
    return false;

  return container->GetId() == kShellWindowId_ArcVirtualKeyboardContainer;
}

bool TouchExplorationController::ShouldEnableVolumeSlideGesture(
    const ui::TouchEvent& event) {
  // Can be nullptr in unit tests.
  int edge = FindEdgesWithinInset(event.location(), kMaxDistanceFromEdge);
  return edge & RIGHT_EDGE && edge != BOTTOM_RIGHT_CORNER &&
         (!Shell::HasInstance() ||
          display::Screen::GetScreen()->InTabletMode() ||
          Shell::Get()
              ->accessibility_controller()
              ->enable_chromevox_volume_slide_gesture());
}

}  // namespace ash
