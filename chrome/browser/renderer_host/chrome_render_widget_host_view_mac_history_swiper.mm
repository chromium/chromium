// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <optional>

#import "chrome/browser/ui/cocoa/history_overlay_controller.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "ui/events/blink/did_overscroll_params.h"

namespace {

// The horizontal distance in points required for touchless (Magic Mouse/
// touchscreen) gestures to complete history navigation. Unlike the thresholds
// below, which use normalized [0, 1] touch surface coordinates, scroll wheel
// events report delta in points (device-independent units). 200 points
// corresponds to roughly 1.5–2 inches on screen, which is far enough to avoid
// accidental navigations from smaller scrolls while remaining comfortable to
// swipe.
// Note on naming: We use "touchless" because AppKit's compatibility layer
// translates touchscreen swipes into mouse scroll wheel events without
// delivering -touches*WithEvent: callbacks.
constexpr CGFloat kTouchlessHistorySwipeThreshold = 200.0;

// The horizontal distance required to cause the browser to perform a history
// navigation.
constexpr CGFloat kHistorySwipeThreshold = 0.08;

// The horizontal distance required for this class to start consuming events,
// which stops the events from reaching the renderer.
constexpr CGFloat kConsumeEventThreshold = 0.01;

// If there has been sufficient vertical motion, the gesture can't be intended
// for history swiping.
constexpr CGFloat kCancelEventVerticalThreshold = 0.24;

// If there has been sufficient vertical motion, and more vertical than
// horizontal motion, the gesture can't be intended for history swiping.
constexpr CGFloat kCancelEventVerticalLowerThreshold = 0.01;

}  // namespace

@interface HistorySwiper ()

// Given a touch event, returns the average touch position. The point returned
// is in "normalized" coordinates (i.e. `NSTouch.normalizedPosition`) in that
// each coordinate value is scaled to be in the range [0, 1], with (0, 0) in the
// lower-left corner and (1, 1) in the upper-right corner.
- (NSPoint)averagePositionInTouchEvent:(NSEvent*)event;

// Updates the internal state with the location information from the given touch
// event.
- (void)updateCurrentPointFromTouchEvent:(NSEvent*)event;

// Updates the state machine with the given touch event. Returns NO if no
// further processing of the event should happen.
- (BOOL)processTouchEventForHistorySwiping:(NSEvent*)event;

// Returns whether the wheel event should be consumed, and not passed to the
// renderer.
- (BOOL)shouldConsumeWheelEvent:(NSEvent*)event;

// Shows the history swiper overlay.
- (void)showHistoryOverlay:(history_swiper::NavigationDirection)direction;

// Removes the history swiper overlay. This is safe to call whether or not the
// overlay is currently showing.
- (void)removeHistoryOverlay;

// Called to process a scroll wheel event. Returns YES if the event was consumed
// or NO if it should be passed on to the renderer.
//
// There are multiple types of scroll events handled by this method:
//
//  • Traditional mouse scroll wheel events. These must not initiate history
//    swiping. They can be distinguished by the fact that `phase` and
//    `momentumPhase` both return NSEventPhaseNone. For these events, this
//    method returns NO and does not process them further.
//
//  • Trackpad scroll wheel events derived from touches. Touches contain a lot
//    of data that allows for a deep history swiping experience, so this class
//    is predominantly designed to handle this use case. When processing these
//    events, this method attempts to transition the state machine from
//    kPending -> kPotential. If it performs the transition, it also shows the
//    history overlay. In order for a history swipe gesture to be recognized,
//    the transition must occur.
//
//  • Touchless scroll events (Magic Mouse and touchscreen swiping). These
//    events have no backing -touches*WithEvent: callbacks, and they are
//    tracked by accumulating scroll wheel deltas directly. Therefore, if a
//    scroll wheel event is determined to have no accompanying touch events,
//    processing is shunted to `-handleTouchlessHistorySwipe:`.
//
//  • Residual momentum events. If a user scrolls with velocity, then even after
//    the user completes their swipe, momentum scroll events will come in. Those
//    momentum events will never initiate a history swipe, but once a history
//    swipe is begun, momentum events qualify to continue it.
- (BOOL)handleScrollWheelEvent:(NSEvent*)event;

// Attempts to initiate history swiping for touchless scroll events (Magic Mouse
// and touchscreen swiping). Returns YES if the event was consumed or NO if it
// should be passed on to the renderer.
- (BOOL)handleTouchlessHistorySwipe:(NSEvent*)theEvent;

// Initiates tracking for touchless swipe gestures.
- (void)initiateTouchlessHistorySwipe:(BOOL)isRightScroll event:(NSEvent*)event;

// Updates the progress bar for touchless gestures. Returns YES if the threshold
// has been reached.
- (BOOL)updateTouchlessProgressBar;

@end

@implementation HistorySwiper {
  // This controller will exist if and only if the UI is in history swipe mode.
  HistoryOverlayController* __strong _historyOverlay;

  // Tracks `-touches*WithEvent:` messages. Set to YES when a "begin" event is
  // received, and set to NO when an "ended" or "cancelled" event is received.
  BOOL _receivingTouchEvents;

  // --- Touch processing ---
  //
  // These ivars are used to handle the touches when processing the
  // `-touches*WithEvent:` messages. They are not used during Magic Mouse event
  // processing.
  //
  // The ivars `_touchStartPoint`, `_touchCurrentPoint`, and `_gestureTotalY`
  // are in "normalized" coordinates (i.e. `NSTouch.normalizedPosition`) in that
  // each coordinate value is scaled to be in the range [0, 1], with (0, 0) in
  // the lower-left corner and (1, 1) in the upper-right corner.

  // The location of the fingers when the touches started.
  NSPoint _touchStartPoint;

  // The current location of the fingers in the touches.
  NSPoint _touchCurrentPoint;

  // The total Y distance moved since the beginning of the touches. Note that
  // this is a "total distance" and accumulates as the user swipes up and down.
  CGFloat _gestureTotalY;

  // The user's intended direction with the history swipe. Set during the
  // transition from kPending -> kPotential.
  history_swiper::NavigationDirection _historySwipeDirection;

  // Whether the history swipe gesture has its direction inverted. Set during
  // the transition from kPending -> kPotential.
  BOOL _historySwipeDirectionInverted;

  // Whether:
  //  1) When wheel gestures are disabled if the wheel event with phase
  //     NSEventPhaseBegan was consumed by the renderer.
  //  2) When wheel gestures are enabled and if the first gesture
  //     scroll was not consumed by the renderer.
  // This variables defaults to NO for new gestures.
  BOOL _firstScrollUnconsumed;

  // Whether the overscroll has been triggered by renderer and is not disabled
  // by CSSOverscrollBehavior.
  BOOL _overscrollTriggeredByRenderer;

  // Whether we have received a gesture scroll begin and are awaiting the first
  // gesture scroll update to determine if the event was consumed by the
  // renderer.
  BOOL _waitingForFirstGestureScroll;

  // What state the gesture recognition is in.
  history_swiper::RecognitionState _recognitionState;

  // --- Touchless (Magic Mouse/touchscreen) gesture processing ---

  // Cumulative scroll delta since scroll gesture start. Only valid during
  // scroll gesture handling. Used for history swiping without touch callbacks.
  NSSize _touchlessScrollDelta;
}

@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:(id<HistorySwiperDelegate>)delegate {
  self = [super init];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)dealloc {
  [self removeHistoryOverlay];
}

- (BOOL)handleEvent:(NSEvent*)event {
  if (event.type == NSEventTypeScrollWheel) {
    return [self handleScrollWheelEvent:event];
  }

  return NO;
}

- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin:
      if (event.data.scroll_begin.synthetic ||
          event.data.scroll_begin.inertial_phase ==
              blink::WebGestureEvent::InertialPhaseState::kMomentum) {
        return;
      }
      _waitingForFirstGestureScroll = YES;
      break;
    case blink::WebInputEvent::Type::kGestureScrollUpdate:
      if (_waitingForFirstGestureScroll) {
        _firstScrollUnconsumed = !consumed;
      }
      _waitingForFirstGestureScroll = NO;
      break;
    default:
      break;
  }
}

- (void)onOverscrolled:(const ui::DidOverscrollParams&)params {
  _overscrollTriggeredByRenderer =
      params.overscroll_behavior.PropagatesXScroll();
}

- (NSPoint)averagePositionInTouchEvent:(NSEvent*)event {
  NSPoint position = NSMakePoint(0, 0);
  int pointCount = 0;
  // The event must correspond to a valid gesture, or else -[NSEvent
  // touchesMatchingPhase:inView:] will fail.
  for (NSTouch* touch in [event touchesMatchingPhase:NSTouchPhaseAny
                                              inView:nil]) {
    position.x += touch.normalizedPosition.x;
    position.y += touch.normalizedPosition.y;
    ++pointCount;
  }

  if (pointCount > 1) {
    position.x /= pointCount;
    position.y /= pointCount;
  }

  return position;
}

- (void)updateCurrentPointFromTouchEvent:(NSEvent*)event {
  NSPoint averagePosition = [self averagePositionInTouchEvent:event];
  _gestureTotalY += std::abs(averagePosition.y - _touchCurrentPoint.y);
  _touchCurrentPoint = averagePosition;
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  _receivingTouchEvents = YES;

  // Reset state pertaining to previous trackpad gestures.
  _touchStartPoint = [self averagePositionInTouchEvent:event];
  _touchCurrentPoint = _touchStartPoint;
  _gestureTotalY = 0.0;
  _firstScrollUnconsumed = NO;
  _overscrollTriggeredByRenderer = NO;
  _waitingForFirstGestureScroll = NO;
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
  [self processTouchEventForHistorySwiping:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  _receivingTouchEvents = NO;
  if (![self processTouchEventForHistorySwiping:event]) {
    return;
  }

  [self cancelHistorySwipe];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  _receivingTouchEvents = NO;
  if (![self processTouchEventForHistorySwiping:event]) {
    return;
  }

  if (_historyOverlay) {
    BOOL finished = [self updateProgressBar];

    // If the gesture was completed, perform a navigation.
    if (finished) {
      [self navigateBrowserInDirection:_historySwipeDirection];
    }

    [self removeHistoryOverlay];

    // The gesture was completed.
    _recognitionState = history_swiper::kCompleted;
  }
}

- (BOOL)processTouchEventForHistorySwiping:(NSEvent*)event {
  switch (_recognitionState) {
    case history_swiper::kCancelled:
    case history_swiper::kCompleted:
      return NO;
    case history_swiper::kPending:
    case history_swiper::kPotential:
    case history_swiper::kTracking:
      break;
  }

  [self updateCurrentPointFromTouchEvent:event];

  // Consider cancelling the history swipe gesture.
  if ([self shouldCancelHorizontalSwipeWithCurrentPoint:_touchCurrentPoint
                                             startPoint:_touchStartPoint]) {
    [self cancelHistorySwipe];
    return NO;
  }

  // Don't do any more processing if the state machine is in the pending state.
  if (_recognitionState == history_swiper::kPending) {
    return NO;
  }

  if (_recognitionState == history_swiper::kPotential) {
    // The user is in the process of doing history swiping.  If the history
    // swipe has progressed sufficiently far, stop sending events to the
    // renderer.
    BOOL sufficientlyFar = std::abs(_touchCurrentPoint.x - _touchStartPoint.x) >
                           kConsumeEventThreshold;
    if (sufficientlyFar) {
      _recognitionState = history_swiper::kTracking;

      if (_historySwipeDirection == history_swiper::kBackwards) {
        [_delegate backwardsSwipeNavigationLikely];
      }
    }
  }

  if (_historyOverlay) {
    [self updateProgressBar];
  }
  return YES;
}

// Consider cancelling the horizontal swipe if the user was intending a
// vertical swipe.
- (BOOL)shouldCancelHorizontalSwipeWithCurrentPoint:(NSPoint)currentPoint
                                         startPoint:(NSPoint)startPoint {
  CGFloat yDelta = _gestureTotalY;
  CGFloat xDelta = std::abs(currentPoint.x - startPoint.x);

  // The gesture is pretty clearly more vertical than horizontal.
  if (yDelta > 2 * xDelta) {
    return YES;
  }

  // There's been more vertical distance than horizontal distance.
  if (yDelta * 1.3 > xDelta && yDelta > kCancelEventVerticalLowerThreshold) {
    return YES;
  }

  // There's been a lot of vertical distance.
  if (yDelta > kCancelEventVerticalThreshold) {
    return YES;
  }

  return NO;
}

- (void)cancelHistorySwipe {
  [self removeHistoryOverlay];
  _recognitionState = history_swiper::kCancelled;
}

- (void)removeHistoryOverlay {
  [_historyOverlay dismiss];
  _historyOverlay = nil;
}

// Returns whether the progress bar has been 100% filled.
- (BOOL)updateProgressBar {
  NSPoint currentPoint = _touchCurrentPoint;
  NSPoint startPoint = _touchStartPoint;

  CGFloat progress = 0;
  BOOL finished = NO;

  progress = (currentPoint.x - startPoint.x) / kHistorySwipeThreshold;
  // If the swipe is a backwards gesture, we need to invert progress.
  if (_historySwipeDirection == history_swiper::kBackwards) {
    progress *= -1;
  }

  // If the user has directions reversed, we need to invert progress.
  if (_historySwipeDirectionInverted) {
    progress *= -1;
  }

  if (progress >= 1.0) {
    finished = YES;
  }

  // Progress can't be less than 0 or greater than 1.
  progress = std::clamp(progress, 0.0, 1.0);

  [_historyOverlay setProgress:progress finished:finished];

  return finished;
}

- (void)showHistoryOverlay:(history_swiper::NavigationDirection)direction {
  // We cannot make any assumptions about the current state of the
  // `_historyOverlay`, since users may attempt to use multiple gesture input
  // devices simultaneously, which confuses Cocoa.
  [self removeHistoryOverlay];

  HistoryOverlayController* historyOverlay = [[HistoryOverlayController alloc]
      initForMode:(direction == history_swiper::kForwards)
                      ? kHistoryOverlayModeForward
                      : kHistoryOverlayModeBack];
  [historyOverlay showPanelForView:[_delegate viewThatWantsHistoryOverlay]];
  _historyOverlay = historyOverlay;
}

- (void)navigateBrowserInDirection:
    (history_swiper::NavigationDirection)direction {
  [_delegate navigateInDirection:direction
                        onWindow:_historyOverlay.view.window];
}

- (BOOL)browserCanNavigateInDirection:
            (history_swiper::NavigationDirection)direction
                                event:(NSEvent*)event {
  return [_delegate canNavigateInDirection:direction onWindow:[event window]];
}

- (BOOL)handleTouchlessHistorySwipe:(NSEvent*)theEvent {
  // Momentum events should not initiate history swiping.
  if (theEvent.phase == NSEventPhaseNone) {
    return NO;
  }

  _touchlessScrollDelta.width += theEvent.scrollingDeltaX;
  _touchlessScrollDelta.height += theEvent.scrollingDeltaY;

  BOOL isHorizontalGesture = std::abs(_touchlessScrollDelta.width) >
                             std::abs(_touchlessScrollDelta.height);
  if (!isHorizontalGesture) {
    return NO;
  }

  BOOL isRightScroll = theEvent.scrollingDeltaX < 0;
  history_swiper::NavigationDirection direction =
      isRightScroll ? history_swiper::kForwards : history_swiper::kBackwards;
  BOOL browserCanMove = [self browserCanNavigateInDirection:direction
                                                      event:theEvent];
  if (!browserCanMove) {
    return NO;
  }

  [self initiateTouchlessHistorySwipe:isRightScroll event:theEvent];
  return YES;
}

- (void)initiateTouchlessHistorySwipe:(BOOL)isRightScroll
                                event:(NSEvent*)event {
  _historySwipeDirection =
      isRightScroll ? history_swiper::kForwards : history_swiper::kBackwards;
  _historySwipeDirectionInverted = event.directionInvertedFromDevice;
  _recognitionState = history_swiper::kTracking;

  [self showHistoryOverlay:_historySwipeDirection];
  [self updateTouchlessProgressBar];

  if (_historySwipeDirection == history_swiper::kBackwards) {
    [_delegate backwardsSwipeNavigationLikely];
  }
}

- (BOOL)updateTouchlessProgressBar {
  CGFloat deltaX = _touchlessScrollDelta.width;
  CGFloat progress = 0;
  if (_historySwipeDirection == history_swiper::kForwards) {
    progress = -deltaX / kTouchlessHistorySwipeThreshold;
  } else {
    progress = deltaX / kTouchlessHistorySwipeThreshold;
  }

  BOOL finished = (progress >= 1.0);
  progress = std::clamp(progress, 0.0, 1.0);

  [_historyOverlay setProgress:progress finished:finished];
  return finished;
}

- (BOOL)handleScrollWheelEvent:(NSEvent*)theEvent {
  // If the swipe began, then reset state.
  if (theEvent.phase == NSEventPhaseBegan) {
    _recognitionState = history_swiper::kPending;
    _touchlessScrollDelta = NSZeroSize;

    // Since this might or might not be a history swipe, allow propagation by
    // falling through to the eventual `return NO`.
  }

  // Handle gesture completion/cancellation for touchless gestures when not
  // receiving touch events.
  if (!_receivingTouchEvents &&
      _recognitionState == history_swiper::kTracking) {
    NSEventPhase phase = theEvent.phase != NSEventPhaseNone
                             ? theEvent.phase
                             : theEvent.momentumPhase;
    if (phase == NSEventPhaseEnded) {
      BOOL finished = [self updateTouchlessProgressBar];
      if (finished) {
        [self navigateBrowserInDirection:_historySwipeDirection];
      }

      [self removeHistoryOverlay];
      _recognitionState = history_swiper::kCompleted;
      return YES;
    }

    if (phase == NSEventPhaseCancelled) {
      [self cancelHistorySwipe];
      return YES;
    }
  }

  // The only events that this class consumes have type NSEventPhaseChanged.
  // This simultaneously weeds our regular mouse wheel scroll events, and
  // gesture events with incorrect phase.
  if (theEvent.phase != NSEventPhaseChanged &&
      theEvent.momentumPhase != NSEventPhaseChanged) {
    return NO;
  }

  // We've already processed this gesture.
  if (_recognitionState != history_swiper::kPending) {
    if (!_receivingTouchEvents &&
        _recognitionState == history_swiper::kTracking) {
      _touchlessScrollDelta.width += theEvent.scrollingDeltaX;
      _touchlessScrollDelta.height += theEvent.scrollingDeltaY;

      // Check if vertical motion dominates and cancel if so.
      if (std::abs(_touchlessScrollDelta.height) >
              std::abs(_touchlessScrollDelta.width) * 1.5 &&
          std::abs(_touchlessScrollDelta.height) > 30.0) {
        [self cancelHistorySwipe];
        return NO;
      }

      [self updateTouchlessProgressBar];
      return YES;
    }

    return [self shouldConsumeWheelEvent:theEvent];
  }

  // Don't allow momentum events to start history swiping.
  if (theEvent.momentumPhase != NSEventPhaseNone) {
    return NO;
  }

  if (!NSEvent.swipeTrackingFromScrollEventsEnabled) {
    return NO;
  }

  if (![_delegate shouldAllowHistorySwiping]) {
    return NO;
  }

  // Don't enable history swiping until the renderer has decided to not consume
  // the event with phase NSEventPhaseBegan.
  if (!_firstScrollUnconsumed) {
    return NO;
  }

  // History swiping should be prevented if the renderer hasn't triggered it.
  if (!_overscrollTriggeredByRenderer) {
    return NO;
  }

  // Trackpad scroll events generate -touches*WithEvent: callbacks, whereas
  // Magic Mouse and touchscreen scroll events do not.
  //
  // At this point, if there is a gesture phase but no touches, then this scroll
  // event is coming from a Magic Mouse or touchscreen, and usage of the
  // touchless delta tracking mechanism is required.
  if (!_receivingTouchEvents) {
    return [self handleTouchlessHistorySwipe:theEvent];
  }

  // TODO(erikchen): Ideally, the direction of history swiping should not be
  // determined this early in a gesture, when it's unclear what the user is
  // intending to do. Since it is determined this early, make sure that there
  // is at least a minimal amount of horizontal motion.
  CGFloat xDelta = _touchCurrentPoint.x - _touchStartPoint.x;
  if (std::abs(xDelta) < 0.001) {
    return NO;
  }

  BOOL isRightScroll = xDelta > 0;
  if (theEvent.directionInvertedFromDevice) {
    isRightScroll = !isRightScroll;
  }

  history_swiper::NavigationDirection direction =
      isRightScroll ? history_swiper::kForwards : history_swiper::kBackwards;
  BOOL browserCanMove = [self browserCanNavigateInDirection:direction
                                                      event:theEvent];
  if (!browserCanMove) {
    return NO;
  }

  _historySwipeDirection = direction;
  _historySwipeDirectionInverted = theEvent.directionInvertedFromDevice;
  _recognitionState = history_swiper::kPotential;
  [self showHistoryOverlay:direction];
  return [self shouldConsumeWheelEvent:theEvent];
}

- (BOOL)shouldConsumeWheelEvent:(NSEvent*)event {
  switch (_recognitionState) {
    case history_swiper::kPending:
    case history_swiper::kCancelled:
      return NO;
    case history_swiper::kTracking:
    case history_swiper::kCompleted:
      return YES;
    case history_swiper::kPotential:
      // It is unclear whether the user is attempting to perform history
      // swiping.  If the event has a vertical component, send it on to the
      // renderer.
      return event.scrollingDeltaY == 0;
  }
}

@end
