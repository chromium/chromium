// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/touch_exploration_controller.h"

#include <utility>

#include "ash/accessibility/touch_accessibility_enabler.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

#define SET_STATE(state) SetState(state, __func__)
#define VLOG_EVENT(event) \
  if (VLOG_IS_ON(1))      \
  VlogEvent(event, __func__)

namespace ash {

namespace {

// Delay between adjustment sounds.
const int kSoundDelayInMS = 150;

void SetTouchAccessibilityFlag(ui::Event* event) {
  // This flag is used to identify mouse move events that were generated from
  // touch exploration in Chrome code.
  event->set_flags(event->flags() | ui::EF_TOUCH_ACCESSIBILITY);
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
      gesture_provider_(new ui::GestureProviderAura(this, this)),
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
  anchor_point_dip_ = gfx::PointF(native_point.x(), native_point.y());
  anchor_point_state_ = ANCHOR_POINT_EXPLICITLY_SET;
}

void TouchExplorationController::SetExcludeBounds(const gfx::Rect& bounds) {
  exclude_bounds_ = bounds;
}

void TouchExplorationController::SetLiftActivationBounds(
    const gfx::Rect& bounds) {
  lift_activation_bounds_ = bounds;
}

ui::EventRewriteStatus TouchExplorationController::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (!event.IsTouchEvent()) {
    if (event.IsKeyEvent()) {
      const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
      VLOG(1) << "\nKeyboard event: " << key_event.GetName()
              << "\n Key code: " << key_event.key_code()
              << ", Flags: " << key_event.flags()
              << ", Is char: " << key_event.is_char();
    }
    return ui::EVENT_REWRITE_CONTINUE;
  }
  const ui::TouchEvent& touch_event = static_cast<const ui::TouchEvent&>(event);

  // Let TouchAccessibilityEnabler process the unrewritten event.
  if (touch_accessibility_enabler_)
    touch_accessibility_enabler_->HandleTouchEvent(touch_event);

  if (event.type() == ui::ET_TOUCH_PRESSED)
    seen_press_ = true;

  // Touch events come through in screen pixels.
  gfx::Point location = touch_event.location();
  gfx::Point root_location = touch_event.root_location();
  root_window_->GetHost()->ConvertPixelsToDIP(&location);
  root_window_->GetHost()->ConvertPixelsToDIP(&root_location);

  if (!exclude_bounds_.IsEmpty()) {
    bool in_exclude_area = exclude_bounds_.Contains(location);
    if (in_exclude_area) {
      if (state_ == NO_FINGERS_DOWN)
        return ui::EVENT_REWRITE_CONTINUE;
      if (touch_event.type() == ui::ET_TOUCH_MOVED ||
          touch_event.type() == ui::ET_TOUCH_PRESSED) {
        return ui::EVENT_REWRITE_DISCARD;
      }
      // Otherwise, continue handling events. Basically, we want to let
      // CANCELLED or RELEASE events through so this can get back to
      // the NO_FINGERS_DOWN state.
    }
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
  if (type == ui::ET_TOUCH_PRESSED) {
    current_touch_ids_.push_back(touch_id);
    touch_locations_.insert(std::pair<int, gfx::PointF>(touch_id, location));
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    std::vector<int>::iterator it = std::find(
        current_touch_ids_.begin(), current_touch_ids_.end(), touch_id);

    // Can happen if touch exploration is enabled while fingers were down
    // or if an additional press occurred within the exclusion bounds.
    if (it == current_touch_ids_.end()) {
      // If we get a RELEASE event and we've never seen a PRESS event
      // since TouchExplorationController was instantiated, cancel the
      // event so that touch gestures that enable spoken feedback
      // don't accidentally trigger other behaviors on release.
      if (!seen_press_) {
        std::unique_ptr<ui::TouchEvent> new_event(new ui::TouchEvent(
            ui::ET_TOUCH_CANCELLED, gfx::Point(), touch_event.time_stamp(),
            touch_event.pointer_details()));
        new_event->set_location(location);
        new_event->set_root_location(root_location);
        new_event->set_flags(touch_event.flags());
        *rewritten_event = std::move(new_event);
        return ui::EVENT_REWRITE_REWRITTEN;
      }

      // Otherwise just pass it through.
      return ui::EVENT_REWRITE_CONTINUE;
    }

    current_touch_ids_.erase(it);
    touch_locations_.erase(touch_id);
  } else if (type == ui::ET_TOUCH_MOVED) {
    std::vector<int>::iterator it = std::find(
        current_touch_ids_.begin(), current_touch_ids_.end(), touch_id);

    // Can happen if touch exploration is enabled while fingers were down.
    if (it == current_touch_ids_.end())
      return ui::EVENT_REWRITE_CONTINUE;

    touch_locations_[*it] = gfx::PointF(location);
  } else {
    NOTREACHED() << "Unexpected event type received: " << event.GetName();
    return ui::EVENT_REWRITE_CONTINUE;
  }
  VLOG_EVENT(touch_event);

  // In order to avoid accidentally double tapping when moving off the edge
  // of the screen, the state will be rewritten to NoFingersDown.
  if ((type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) &&
      FindEdgesWithinInset(location, kLeavingScreenEdge) != NO_EDGE) {
    if (VLOG_on_)
      VLOG(1) << "Leaving screen";

    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
      if (VLOG_on_) {
        VLOG(1) << "Reset to no fingers in Rewrite event because the touch  "
                   "release or cancel was on the edge of the screen.";
      }
      return ui::EVENT_REWRITE_DISCARD;
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
          false /* is_source_touch_event_set_non_blocking */);
    }
    ProcessGestureEvents();
  }

  ui::EventRewriteStatus status = ui::EVENT_REWRITE_CONTINUE;
  // The rest of the processing depends on what state we're in.
  switch (state_) {
    case NO_FINGERS_DOWN:
      status = InNoFingersDown(touch_event_dip, rewritten_event);
      break;
    case SINGLE_TAP_PRESSED:
      status = InSingleTapPressed(touch_event_dip, rewritten_event);
      break;
    case SINGLE_TAP_RELEASED:
    case TOUCH_EXPLORE_RELEASED:
      status =
          InSingleTapOrTouchExploreReleased(touch_event_dip, rewritten_event);
      break;
    case DOUBLE_TAP_PENDING:
      status = InDoubleTapPending(touch_event_dip, rewritten_event);
      break;
    case TOUCH_RELEASE_PENDING:
      status = InTouchReleasePending(touch_event_dip, rewritten_event);
      break;
    case TOUCH_EXPLORATION:
      status = InTouchExploration(touch_event_dip, rewritten_event);
      break;
    case GESTURE_IN_PROGRESS:
      status = InGestureInProgress(touch_event_dip, rewritten_event);
      break;
    case TOUCH_EXPLORE_SECOND_PRESS:
      status = InTouchExploreSecondPress(touch_event_dip, rewritten_event);
      break;
    case SLIDE_GESTURE:
      status = InSlideGesture(touch_event_dip, rewritten_event);
      break;
    case ONE_FINGER_PASSTHROUGH:
      status = InOneFingerPassthrough(touch_event_dip, rewritten_event);
      break;
    case WAIT_FOR_NO_FINGERS:
      status = InWaitForNoFingers(touch_event_dip, rewritten_event);
      break;
    case TWO_FINGER_TAP:
      status = InTwoFingerTap(touch_event_dip, rewritten_event);
      break;
  }
  if (status == ui::EVENT_REWRITE_REWRITTEN) {
    DCHECK(rewritten_event->get());
    SetTouchAccessibilityFlag(rewritten_event->get());
  }
  return status;
}

ui::EventRewriteStatus TouchExplorationController::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InNoFingersDown(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type != ui::ET_TOUCH_PRESSED) {
    NOTREACHED() << "Unexpected event type received: " << event.GetName();
    return ui::EVENT_REWRITE_CONTINUE;
  }

  initial_press_ = std::make_unique<ui::TouchEvent>(event);
  most_recent_press_timestamp_ = initial_press_->time_stamp();
  initial_presses_[event.pointer_details().id] = event.location();
  last_unused_finger_event_ = std::make_unique<ui::TouchEvent>(event);
  StartTapTimer();
  SET_STATE(SINGLE_TAP_PRESSED);
  return ui::EVENT_REWRITE_DISCARD;
}

ui::EventRewriteStatus TouchExplorationController::InSingleTapPressed(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    initial_presses_[event.pointer_details().id] = event.location();
    SET_STATE(TWO_FINGER_TAP);
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    if (current_touch_ids_.size() == 0 &&
        event.pointer_details().id == initial_press_->pointer_details().id) {
      MaybeSendSimulatedTapInLiftActivationBounds(event);
      SET_STATE(SINGLE_TAP_RELEASED);
    } else if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_MOVED) {
    float distance = (event.location() - initial_press_->location()).Length();
    // If the user does not move far enough from the original position, then the
    // resulting movement should not be considered to be a deliberate gesture or
    // touch exploration.
    if (distance <= gesture_detector_config_.touch_slop)
      return ui::EVENT_REWRITE_DISCARD;

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
    int edge = FindEdgesWithinInset(event.location(), kMaxDistanceFromEdge);
    if (edge & RIGHT_EDGE && edge != BOTTOM_RIGHT_CORNER) {
      SET_STATE(SLIDE_GESTURE);
      return InSlideGesture(event, rewritten_event);
    }

    // If the user moves fast enough from the initial touch location, start
    // gesture detection. Otherwise, jump to the touch exploration mode early.
    if (velocity > gesture_detector_config_.minimum_swipe_velocity) {
      SET_STATE(GESTURE_IN_PROGRESS);
      return InGestureInProgress(event, rewritten_event);
    }
    anchor_point_state_ = ANCHOR_POINT_FROM_TOUCH_EXPLORATION;
    EnterTouchToMouseMode();
    SET_STATE(TOUCH_EXPLORATION);
    return InTouchExploration(event, rewritten_event);
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus
TouchExplorationController::InSingleTapOrTouchExploreReleased(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  // If there is more than one finger down, then discard to wait until no
  // fingers are down.
  if (current_touch_ids_.size() > 1) {
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return ui::EVENT_REWRITE_DISCARD;
  }
  if (type == ui::ET_TOUCH_PRESSED) {
    // If there is no anchor point for synthesized events because the
    // user hasn't touch-explored or focused anything yet, we can't
    // send a click, so discard.
    if (anchor_point_state_ == ANCHOR_POINT_NONE) {
      tap_timer_.Stop();
      return ui::EVENT_REWRITE_DISCARD;
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
    last_unused_finger_event_.reset(new ui::TouchEvent(event));
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED &&
             anchor_point_state_ == ANCHOR_POINT_NONE) {
    // If the previous press was discarded, we need to also handle its
    // release.
    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_MOVED) {
    return ui::EVENT_REWRITE_DISCARD;
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InDoubleTapPending(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_MOVED) {
    // If the user moves far enough from the initial touch location (outside
    // the "slop" region, jump to passthrough mode early.
    float delta = (event.location() - initial_press_->location()).Length();
    if (delta > gesture_detector_config_.double_tap_slop) {
      tap_timer_.Stop();
      OnTapTimerFired();
    }
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    if (current_touch_ids_.size() != 0)
      return ui::EVENT_REWRITE_DISCARD;

    SendSimulatedClickOrTap();

    SET_STATE(NO_FINGERS_DOWN);
    return ui::EVENT_REWRITE_DISCARD;
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InTouchReleasePending(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED || type == ui::ET_TOUCH_MOVED) {
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    if (current_touch_ids_.size() != 0)
      return ui::EVENT_REWRITE_DISCARD;

    SendSimulatedClickOrTap();
    SET_STATE(NO_FINGERS_DOWN);
    return ui::EVENT_REWRITE_DISCARD;
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InTouchExploration(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    // Enter split-tap mode.
    initial_press_ = std::make_unique<ui::TouchEvent>(event);
    tap_timer_.Stop();
    SET_STATE(TOUCH_EXPLORE_SECOND_PRESS);
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    initial_press_ = std::make_unique<ui::TouchEvent>(event);
    StartTapTimer();
    most_recent_press_timestamp_ = event.time_stamp();
    MaybeSendSimulatedTapInLiftActivationBounds(event);
    SET_STATE(TOUCH_EXPLORE_RELEASED);
  } else if (type != ui::ET_TOUCH_MOVED) {
    NOTREACHED();
    return ui::EVENT_REWRITE_CONTINUE;
  }

  // Rewrite as a mouse-move event.
  // |event| locations are in DIP; see |RewriteEvent|. We need to dispatch
  // |screen coords.
  gfx::PointF location_f(ConvertDIPToPixels(event.location_f()));
  *rewritten_event = CreateMouseMoveEvent(location_f, event.flags());
  last_touch_exploration_ = std::make_unique<ui::TouchEvent>(event);
  if (anchor_point_state_ != ANCHOR_POINT_EXPLICITLY_SET)
    anchor_point_dip_ = last_touch_exploration_->location_f();

  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus TouchExplorationController::InGestureInProgress(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  // The events were sent to the gesture provider in RewriteEvent already.
  // If no gesture is registered before the tap timer times out, the state
  // will change to "wait for no fingers down" or "touch exploration" depending
  // on the number of fingers down, and this function will stop being called.
  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return ui::EVENT_REWRITE_DISCARD;
}

ui::EventRewriteStatus TouchExplorationController::InOneFingerPassthrough(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (event.pointer_details().id != initial_press_->pointer_details().id) {
    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return ui::EVENT_REWRITE_DISCARD;
  }
  // |event| locations are in DIP; see |RewriteEvent|. We need to dispatch
  // screen coordinates.
  gfx::PointF location_f(
      ConvertDIPToPixels(event.location_f() - passthrough_offset_));
  std::unique_ptr<ui::TouchEvent> new_event(new ui::TouchEvent(
      event.type(), gfx::Point(), event.time_stamp(), event.pointer_details()));
  new_event->set_location_f(location_f);
  new_event->set_root_location_f(location_f);
  new_event->set_flags(event.flags());
  *rewritten_event = std::move(new_event);
  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus TouchExplorationController::InTouchExploreSecondPress(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    // A third finger being pressed means that a split tap can no longer go
    // through. The user enters the wait state, Since there has already been
    // a press dispatched when split tap began, the touch needs to be
    // cancelled.
    std::unique_ptr<ui::TouchEvent> new_event(new ui::TouchEvent(
        ui::ET_TOUCH_CANCELLED, gfx::Point(), event.time_stamp(),
        initial_press_->pointer_details()));
    // TODO(dmazzoni): fix for multiple displays. http://crbug.com/616793
    // |event| locations are in DIP; see |RewriteEvent|. We need to dispatch
    // screen coordinates.
    gfx::PointF location_f(ConvertDIPToPixels(anchor_point_dip_));
    new_event->set_location_f(location_f);
    new_event->set_root_location_f(location_f);
    new_event->set_flags(event.flags());
    *rewritten_event = std::move(new_event);
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return ui::EVENT_REWRITE_REWRITTEN;
  } else if (type == ui::ET_TOUCH_MOVED) {
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
      SET_STATE(WAIT_FOR_NO_FINGERS);
      return ui::EVENT_REWRITE_DISCARD;
    }
    // Check the distance between the current finger location and the original
    // location. The slop for this is a bit more generous since keeping two
    // fingers in place is a bit harder. If the user has left the slop, the
    // user enters the wait state.
    if ((event.location_f() - original_touch->location_f()).Length() >
        GetSplitTapTouchSlop()) {
      SET_STATE(WAIT_FOR_NO_FINGERS);
    }
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    // If the touch exploration finger is lifted, there is no option to return
    // to touch explore anymore. The remaining finger acts as a pending
    // tap or long tap for the last touch explore location.
    if (event.pointer_details().id ==
        last_touch_exploration_->pointer_details().id) {
      SET_STATE(TOUCH_RELEASE_PENDING);
      return ui::EVENT_REWRITE_DISCARD;
    }

    // Continue to release the touch only if the touch explore finger is the
    // only finger remaining.
    if (current_touch_ids_.size() != 1)
      return ui::EVENT_REWRITE_DISCARD;

    SendSimulatedClickOrTap();

    SET_STATE(TOUCH_EXPLORATION);
    EnterTouchToMouseMode();
    return ui::EVENT_REWRITE_DISCARD;
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InWaitForNoFingers(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (current_touch_ids_.size() == 0)
    SET_STATE(NO_FINGERS_DOWN);
  return ui::EVENT_REWRITE_DISCARD;
}

void TouchExplorationController::PlaySoundForTimer() {
  delegate_->PlayVolumeAdjustEarcon();
}

void TouchExplorationController::SendSimulatedClickOrTap() {
  // If we got an anchor point from ChromeVox, send a double-tap gesture
  // and let ChromeVox handle the click.
  if (anchor_point_state_ == ANCHOR_POINT_EXPLICITLY_SET) {
    delegate_->HandleAccessibilityGesture(ax::mojom::Gesture::kClick);
    return;
  }
  SendSimulatedTap();
}

void TouchExplorationController::SendSimulatedTap() {
  std::unique_ptr<ui::TouchEvent> touch_press;
  touch_press.reset(new ui::TouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(),
                                       Now(),
                                       initial_press_->pointer_details()));
  touch_press->set_location_f(anchor_point_dip_);
  touch_press->set_root_location_f(anchor_point_dip_);
  DispatchEvent(touch_press.get());

  std::unique_ptr<ui::TouchEvent> touch_release;
  touch_release.reset(new ui::TouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(),
                                         Now(),
                                         initial_press_->pointer_details()));
  touch_release->set_location_f(anchor_point_dip_);
  touch_release->set_root_location_f(anchor_point_dip_);
  DispatchEvent(touch_release.get());
}

void TouchExplorationController::MaybeSendSimulatedTapInLiftActivationBounds(
    const ui::TouchEvent& event) {
  gfx::Point location = event.location();
  gfx::Point anchor_location(anchor_point_dip_.x(), anchor_point_dip_.y());
  if (lift_activation_bounds_.Contains(anchor_location.x(),
                                       anchor_location.y()) &&
      lift_activation_bounds_.Contains(location)) {
    delegate_->PlayTouchTypeEarcon();
    SendSimulatedTap();
  }
}

ui::EventRewriteStatus TouchExplorationController::InSlideGesture(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  // The timer should not fire when sliding.
  tap_timer_.Stop();

  ui::EventType type = event.type();
  // If additional fingers are added before a swipe gesture has been registered,
  // then wait until all fingers have been lifted.
  if (type == ui::ET_TOUCH_PRESSED ||
      event.pointer_details().id != initial_press_->pointer_details().id) {
    if (sound_timer_.IsRunning())
      sound_timer_.Stop();
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return ui::EVENT_REWRITE_DISCARD;
  }

  // There should not be more than one finger down.
  DCHECK_LE(current_touch_ids_.size(), 1U);

  // Allows user to return to the edge to adjust the sound if they have left the
  // boundaries.
  int edge = FindEdgesWithinInset(event.location(), kSlopDistanceFromEdge);
  if (!(edge & RIGHT_EDGE) && (type != ui::ET_TOUCH_RELEASED)) {
    if (sound_timer_.IsRunning()) {
      sound_timer_.Stop();
    }
    return ui::EVENT_REWRITE_DISCARD;
  }

  // This can occur if the user leaves the screen edge and then returns to it to
  // continue adjusting the sound.
  if (!sound_timer_.IsRunning()) {
    sound_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kSoundDelayInMS), this,
                       &TouchExplorationController::PlaySoundForTimer);
    delegate_->PlayVolumeAdjustEarcon();
  }

  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return ui::EVENT_REWRITE_DISCARD;
}

ui::EventRewriteStatus TouchExplorationController::InTwoFingerTap(
    const ui::TouchEvent& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    // This is now a three finger gesture.
    SET_STATE(GESTURE_IN_PROGRESS);
    return ui::EVENT_REWRITE_DISCARD;
  }

  if (type == ui::ET_TOUCH_MOVED) {
    // Determine if it was a swipe.
    gfx::Point original_location = initial_presses_[event.pointer_details().id];
    float distance = (event.location() - original_location).Length();
    // If the user moves too far from the original position, consider the
    // movement a swipe.
    if (distance > gesture_detector_config_.touch_slop) {
      SET_STATE(GESTURE_IN_PROGRESS);
    }
    return ui::EVENT_REWRITE_DISCARD;
  }

  if (current_touch_ids_.size() != 0)
    return ui::EVENT_REWRITE_DISCARD;

  if (type == ui::ET_TOUCH_RELEASED) {
    delegate_->HandleAccessibilityGesture(ax::mojom::Gesture::kTap2);
    SET_STATE(NO_FINGERS_DOWN);
    return ui::EVENT_REWRITE_DISCARD;
  }
  return ui::EVENT_REWRITE_DISCARD;
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
      anchor_point_dip_ = last_touch_exploration_->location_f();
      anchor_point_state_ = ANCHOR_POINT_FROM_TOUCH_EXPLORATION;
      return;
    case DOUBLE_TAP_PENDING: {
      SET_STATE(ONE_FINGER_PASSTHROUGH);
      passthrough_offset_ =
          last_unused_finger_event_->location_f() - anchor_point_dip_;
      std::unique_ptr<ui::TouchEvent> passthrough_press(
          new ui::TouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(), Now(),
                             last_unused_finger_event_->pointer_details()));
      passthrough_press->set_location_f(anchor_point_dip_);
      passthrough_press->set_root_location_f(anchor_point_dip_);
      DispatchEvent(passthrough_press.get());
      return;
    }
    case SINGLE_TAP_PRESSED:
      FALLTHROUGH;
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
  std::unique_ptr<ui::Event> mouse_move = CreateMouseMoveEvent(
      initial_press_->location_f(), initial_press_->flags());
  DispatchEvent(mouse_move.get());
  last_touch_exploration_ = std::make_unique<ui::TouchEvent>(*initial_press_);
  anchor_point_dip_ = last_touch_exploration_->location_f();
  anchor_point_state_ = ANCHOR_POINT_FROM_TOUCH_EXPLORATION;
}

void TouchExplorationController::DispatchEvent(ui::Event* event) {
  SetTouchAccessibilityFlag(event);
  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF screen_point(ConvertDIPToPixels(located_event->location_f()));
    located_event->set_location_f(screen_point);
    located_event->set_root_location_f(screen_point);
  }
  ignore_result(
      root_window_->GetHost()->dispatcher()->OnEventFromSource(event));
}

// This is an override for a function that is only called for timer-based events
// like long press. Events that are created synchronously as a result of
// certain touch events are added to the vector accessible via
// GetAndResetPendingGestures(). We only care about swipes (which are created
// synchronously), so we ignore this callback.
void TouchExplorationController::OnGestureEvent(ui::GestureConsumer* consumer,
                                                ui::GestureEvent* gesture) {}

void TouchExplorationController::ProcessGestureEvents() {
  std::vector<std::unique_ptr<ui::GestureEvent>> gestures =
      gesture_provider_->GetAndResetPendingGestures();
  bool resolved_gesture = false;
  max_gesture_touch_points_ =
      std::max(max_gesture_touch_points_, current_touch_ids_.size());
  for (const auto& gesture : gestures) {
    if (gesture->type() == ui::ET_GESTURE_SWIPE &&
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

  if (type == ui::ET_GESTURE_SCROLL_BEGIN) {
    delegate_->PlayVolumeAdjustEarcon();
  }

  if (type == ui::ET_GESTURE_SCROLL_END) {
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
  inner_bounds_dip.Inset(inset, inset);

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
    int flags) {
  ui::KeyEvent key_down(ui::ET_KEY_PRESSED, key, flags);
  ui::KeyEvent key_up(ui::ET_KEY_RELEASED, key, flags);
  DispatchEvent(&key_down);
  DispatchEvent(&key_up);
  if (VLOG_on_) {
    VLOG(1) << "\nKey down: key code : " << key_down.key_code()
            << ", flags: " << key_down.flags()
            << "\nKey up: key code : " << key_up.key_code()
            << ", flags: " << key_up.flags();
  }
}

base::Closure TouchExplorationController::BindKeyEventWithFlags(
    const ui::KeyboardCode key,
    int flags) {
  return base::Bind(&TouchExplorationController::DispatchKeyWithFlags,
                    base::Unretained(this), key, flags);
}

std::unique_ptr<ui::MouseEvent>
TouchExplorationController::CreateMouseMoveEvent(const gfx::PointF& location,
                                                 int flags) {
  // The "synthesized" flag should be set on all events that don't have a
  // backing native event.
  flags |= ui::EF_IS_SYNTHESIZED;

  std::unique_ptr<ui::MouseEvent> event(new ui::MouseEvent(
      ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(), Now(), flags, 0));
  event->set_location_f(location);
  event->set_root_location_f(location);
  return event;
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
    case ONE_FINGER_PASSTHROUGH:
    case WAIT_FOR_NO_FINGERS:
      if (gesture_provider_.get())
        gesture_provider_.reset(NULL);
      max_gesture_touch_points_ = 0;
      break;
    case NO_FINGERS_DOWN:
      gesture_provider_ = std::make_unique<ui::GestureProviderAura>(this, this);
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
  if (prev_event_ && prev_event_->type() == ui::ET_TOUCH_MOVED &&
      touch_event.type() == ui::ET_TOUCH_MOVED) {
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

float TouchExplorationController::GetSplitTapTouchSlop() {
  return gesture_detector_config_.touch_slop * 3;
}

gfx::PointF TouchExplorationController::ConvertDIPToPixels(
    const gfx::PointF& location_f) {
  gfx::Point location(gfx::ToFlooredPoint(location_f));
  root_window_->GetHost()->ConvertDIPToPixels(&location);
  return gfx::PointF(location);
}

}  // namespace ash
