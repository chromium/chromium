// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPER_H_

#import <Cocoa/Cocoa.h>

namespace blink {
class WebGestureEvent;
class WebMouseWheelEvent;
}

namespace ui {
struct DidOverscrollParams;
}

namespace history_swiper {
enum NavigationDirection {
  kBackwards = 0,
  kForwards,
};

// The states of a state machine that recognizes the history swipe gesture.
// When a gesture first begins, the state is reset to kPending. The state
// machine only applies to trackpad gestures. Magic Mouse gestures use a
// different mechanism.
enum RecognitionState {
  // Waiting to see whether the renderer will handle the event with phase
  // NSEventPhaseBegan. The state machine will also stay in this state if
  // external conditions prohibit the initialization of history swiping. New
  // gestures always start in this state.
  // Events are forwarded to the renderer.
  kPending,
  // The gesture looks like the beginning of a history swipe.
  // Events are forwarded to the renderer.
  // The history overlay is visible.
  kPotential,
  // The gesture is definitely a history swipe.
  // Events are not forwarded to the renderer.
  // The history overlay is visible.
  kTracking,
  // The history swipe gesture has finished.
  // Events are not forwarded to the renderer.
  kCompleted,
  // The history swipe gesture was cancelled.
  // Events are forwarded to the renderer.
  kCancelled,
};
} // history_swiper

@class HistorySwiper;
@protocol HistorySwiperDelegate
// Return NO from this method if the view/render_widget_host should not
// allow history swiping.
- (BOOL)shouldAllowHistorySwiping;
// The history overlay is added to the view returned from this method.
- (NSView*)viewThatWantsHistoryOverlay;
// Check whether can navigate in direction.
- (BOOL)canNavigateInDirection:(history_swiper::NavigationDirection)direction
                      onWindow:(NSWindow*)window;
// Navigate in direction.
- (void)navigateInDirection:(history_swiper::NavigationDirection)direction
                   onWindow:(NSWindow*)window;

@end

// History swiping is the feature wherein a horizontal 2-finger swipe of of a
// trackpad causes the browser to navigate forwards or backwards.
// Unfortunately, the act of 2-finger swiping is overloaded, and has 3 possible
// effects. In descending order of priority, the swipe should:
//   1. Scroll the content on the web page.
//   2. Perform a history swipe.
//   3. Rubberband/overscroll the content past the edge of the window.
// Effects (1) and (3) are managed by the renderer, whereas effect (2) is
// managed by this class.
//
// Touches on the trackpad enter the run loop as NSEvents, grouped into
// gestures. The phases of NSEvents within a gesture follow a well defined
// order.
//   1. NSEventPhaseMayBegin. (exactly 1 event with this phase)
//   2. NSEventPhaseBegan. (exactly 1 event with this phase)
//   3. NSEventPhaseMoved. (many events with this phase)
//   4. NSEventPhaseEnded. (exactly 1 event with this phase)
// Events with the phase NSEventPhaseCancelled may come in at any time, and
// generally mean that an entity within the Cocoa framework has consumed the
// gesture, and wants to "cancel" previous NSEvents that have been passed to
// this class.
//
// The event handling stack in Chrome passes all events to this class, which is
// given the opportunity to process and consume the event. If the event is not
// consumed, it is passed to the renderer via IPC. The renderer returns an IPC
// indicating whether the event was consumed. To prevent spamming the renderer
// with IPCs, the browser waits for an ACK from the renderer from the previous
// event before sending the next one. While waiting for an ACK, the browser
// coalesces NSEvents with the same phase. It is common for dozens of events
// with the phase NSEventPhaseMoved to be coalesced.
//
// It is difficult to determine from the initial events in a gesture whether
// the gesture was intended to be a history swipe. The loss of information from
// the coalescing of events with phase NSEventPhaseMoved before they are passed
// to the renderer is also problematic. The general approach is as follows:
//   1. Wait for the renderer to return an ACK for the event with phase
//   NSEventPhaseBegan. If that event was not consumed, change the state to
//   kPotential.  If the renderer is not certain about whether the event should
//   be consumed, it tries to not consume the event.
//   2. In the state kPotential, this class will process events and update its
//   internal state machine, but it will also continue to pass events to the
//   renderer. This class tries to aggressively cancel history swiping to make
//   up for the fact that the renderer errs on the side of allowing history
//   swiping to occur.
//   3. As more events come in, if the gesture continues to appear horizontal,
//   then this class will transition to the state kTracking. Events are
//   consumed, and not passed to the renderer.
//
// There are multiple callbacks that provide information about gestures on the
// trackpad. This class uses two different sets of callbacks.
//   1. The -[NSView touches*WithEvent:] callbacks provide detailed information
//   about the touches within a gesture. The callbacks happen with more
//   frequency, and have higher accuracy. These callbacks are used to
//   transition between all states except for kPending -> kPotential.
//   2. The -[NSView scrollWheel:] callback provides less information, but the
//   events are passed to the renderer. This class must process these events so
//   that it can decide whether to consume the events and prevent them from
//   being passed to the renderer. This API is used to transition from kPending
//   -> kPotential.
//
//  This class is also responsible for handling gestures from a Magic Mouse.
//  Magic Mouse gestures do not generate -touches*WithEvent: callbacks, so this
//  class must use the API -[NSEvent trackSwipeEventWithOptions:...] to track
//  the Magic Mouse gesture. Due to an AppKit bug, once this API is invoked,
//  views no longer reliable receive -touches*WithEvent: callbacks. As such,
//  once this class invokes the -[NSEvent trackSwipeEventWithOptions:...] API,
//  it must continue to use that API, since it no longer receives touch events.
//
//  TODO(erikchen): Even for users that do not have a Magic Mouse, this class
//  will sometime transition into Magic Mouse mode. This is very undesirable.
//  See http://crbug.com/317161 for more details.
@class HistoryOverlayController;
@interface HistorySwiper : NSObject {
 @private
  // This controller will exist if and only if the UI is in history swipe mode.
  HistoryOverlayController* _historyOverlay;
  // The location of the fingers when the gesture started.
  NSPoint _gestureStartPoint;
  // The current location of the fingers in the gesture.
  NSPoint _gestureCurrentPoint;
  // The total Y distance moved since the beginning of the gesture.
  CGFloat _gestureTotalY;
  // A flag that indicates that there is an ongoing gesture. Only used to
  // determine whether swipe events are coming from a Magic Mouse.
  BOOL _inGesture;
  // A flag that indicates that Chrome is receiving a series of touch events.
  BOOL _receivingTouches;
  // Each time a new gesture begins, we must get a new start point.
  // This ivar determines whether the start point is valid.
  int _gestureStartPointValid;

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

  // Whether we have received a gesture scroll begin and are awiting on the
  // first gesture scroll update to deteremine of the event was consumed by
  // the renderer.
  BOOL _waitingForFirstGestureScroll;

  history_swiper::RecognitionState _recognitionState;

  id<HistorySwiperDelegate> _delegate;

  // Cumulative scroll delta since scroll gesture start. Only valid during
  // scroll gesture handling. Only used to trigger Magic Mouse history swiping.
  NSSize _mouseScrollDelta;
}

// Many event types are passed in, but the only one we care about is
// NSEventTypeScrollWheel. We look at the phase to determine whether to trigger
// history swiping
- (BOOL)handleEvent:(NSEvent*)event;
- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed;
- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed;

// This is called whenever an overscroll event is generated on the renderer
// side. This is called before InputEventAck. For an overscroll event, the
// ack_result of "unconsumed" will trigger the swipe navigation. However, the
// renderer can plumb the value of overscroll_behavior by DidOverscroll,
// to prevent the swipe navigation before the ack_result arrives.
// This code makes the assumption that the DidOverscroll() event arrives
// before InputEventAcks of GestureScrollUpdate/GestureScrollEnd
// (GestureScrollBegin does not trigger history_swiper) are returned from the
// renderer. As such, it's safe to just set a flag and prevent history swipe
// from starting.
// If this assumption ever becomes false, we will need to update the logic of
// this method to cancel any ongoing history swipes.
- (void)onOverscrolled:(const ui::DidOverscrollParams&)params;

// The event passed in is a gesture event, and has touch data associated with
// the trackpad.
// Once the method -[NSEvent trackSwipeEventWithOptions:...] is invoked, the
// methods -touches*WithEvent: are no longer guaranteed to be called for
// subsequent gestures. http://crbug.com/317161
- (void)touchesBeganWithEvent:(NSEvent*)event;
- (void)touchesMovedWithEvent:(NSEvent*)event;
- (void)touchesCancelledWithEvent:(NSEvent*)event;
- (void)touchesEndedWithEvent:(NSEvent*)event;

- (void)beginGestureWithEvent:(NSEvent*)event;
- (void)endGestureWithEvent:(NSEvent*)event;

// Designated initializer.
- (instancetype)initWithDelegate:(id<HistorySwiperDelegate>)delegate;

@property (nonatomic, assign) id<HistorySwiperDelegate> delegate;

@end

// Exposed only for unit testing, do not call directly.
@interface HistorySwiper (PrivateExposedForTesting)
+ (void)resetMagicMouseState;
@end

#endif // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPER_H_
