// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"

#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"
#include "ui/events/blink/did_overscroll_params.h"

@interface HistorySwiper (MacHistorySwiperTest)
- (BOOL)browserCanNavigateInDirection:
        (history_swiper::NavigationDirection)forward
                                event:(NSEvent*)event;
- (void)removeHistoryOverlay;
- (void)showHistoryOverlay:(history_swiper::NavigationDirection)direction;
- (void)navigateBrowserInDirection:(history_swiper::NavigationDirection)forward;
- (void)initiateMagicMouseHistorySwipe:(BOOL)isRightScroll
                                 event:(NSEvent*)event;
@end

class MacHistorySwiperTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();

    [HistorySwiper resetMagicMouseState];

    view_ = [[NSView alloc] init];
    id mockDelegate =
        [OCMockObject mockForProtocol:@protocol(HistorySwiperDelegate)];
    [[[mockDelegate stub] andReturn:view_] viewThatWantsHistoryOverlay];
    [[[mockDelegate stub] andReturnBool:YES] shouldAllowHistorySwiping];
    [[[mockDelegate stub] andDo:^(NSInvocation* invocation) {
      got_backwards_hint_ = true;
    }] backwardsSwipeNavigationLikely];

    HistorySwiper* historySwiper =
        [[HistorySwiper alloc] initWithDelegate:mockDelegate];
    id mockHistorySwiper = [OCMockObject partialMockForObject:historySwiper];
    [[[mockHistorySwiper stub] andReturnBool:YES]
        browserCanNavigateInDirection:history_swiper::kForwards
                                event:[OCMArg any]];
    [[[mockHistorySwiper stub] andReturnBool:YES]
        browserCanNavigateInDirection:history_swiper::kBackwards
                                event:[OCMArg any]];
    [[[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        ++begin_count_;
        // showHistoryOverlay: calls removeHistoryOverlay internally.
        --end_count_;
    }] andForwardToRealObject] showHistoryOverlay:history_swiper::kForwards];
    [[[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        ++begin_count_;
        // showHistoryOverlay: calls removeHistoryOverlay internally.
        --end_count_;
    }] andForwardToRealObject] showHistoryOverlay:history_swiper::kBackwards];
    [[[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        ++end_count_;
    }] andForwardToRealObject] removeHistoryOverlay];
    [[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        navigated_right_ = true;
    }] navigateBrowserInDirection:history_swiper::kForwards];
    [[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        navigated_left_ = true;
    }] navigateBrowserInDirection:history_swiper::kBackwards];

    [[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        magic_mouse_history_swipe_ = true;
    }] initiateMagicMouseHistorySwipe:YES event:[OCMArg any]];
    [[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        magic_mouse_history_swipe_ = true;
    }] initiateMagicMouseHistorySwipe:NO event:[OCMArg any]];

    historySwiper_ = mockHistorySwiper;

    begin_count_ = 0;
    end_count_ = 0;
    navigated_right_ = false;
    navigated_left_ = false;
    magic_mouse_history_swipe_ = false;
    got_backwards_hint_ = false;
  }

  // These methods send all 3 types of events: gesture, scroll, and touch.
  void startGestureInMiddle();
  void moveGestureInMiddle();
  void moveGestureAtPoint(NSPoint point);
  void momentumMoveGestureAtPoint(NSPoint point);
  void endGestureAtPoint(NSPoint point);
  void rendererACKForBeganEvent();
  void onOverscrolled(cc::OverscrollBehavior::Type);

  // These methods send a single type of event.
  void sendBeginGestureEventInMiddle();
  void sendEndGestureEventAtPoint(NSPoint point);

  HistorySwiper* __strong historySwiper_;
  NSView* __strong view_;
  int begin_count_;
  int end_count_;
  bool navigated_right_;
  bool navigated_left_;
  bool magic_mouse_history_swipe_;
  bool got_backwards_hint_;
};

NSPoint makePoint(CGFloat x, CGFloat y) {
  NSPoint point;
  point.x = x;
  point.y = y;
  return point;
}

id mockEventWithPoint(NSPoint point, NSEventType type) {
  id mockEvent = [OCMockObject mockForClass:[NSEvent class]];
  id mockTouch = [OCMockObject mockForClass:[NSTouch class]];
  [[[mockTouch stub] andReturnNSPoint:point] normalizedPosition];
  NSArray* touches = @[mockTouch];
  [[[mockEvent stub] andReturn:touches] touchesMatchingPhase:NSTouchPhaseAny
    inView:[OCMArg any]];
  [[[mockEvent stub] andReturnBool:NO] isDirectionInvertedFromDevice];
  [(NSEvent*)[[mockEvent stub] andReturnValue:OCMOCK_VALUE(type)] type];

  return mockEvent;
}

id scrollWheelEventWithPhase(NSEventPhase phase,
                             NSEventPhase momentumPhase,
                             CGFloat scrollingDeltaX,
                             CGFloat scrollingDeltaY) {
  // The point isn't used, so we pass in bogus data.
  id event = mockEventWithPoint(makePoint(0, 0), NSEventTypeScrollWheel);
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(phase)] phase];
  [(NSEvent*)
      [[event stub] andReturnValue:OCMOCK_VALUE(momentumPhase)] momentumPhase];
  [(NSEvent*)[[event stub]
       andReturnValue:OCMOCK_VALUE(scrollingDeltaX)] scrollingDeltaX];
  [(NSEvent*)[[event stub]
       andReturnValue:OCMOCK_VALUE(scrollingDeltaY)] scrollingDeltaY];
  return event;
}

id scrollWheelEventWithPhase(NSEventPhase phase,
                             NSEventPhase momentumPhase) {
  return scrollWheelEventWithPhase(phase, momentumPhase, 0, 0);
}

id scrollWheelEventWithPhase(NSEventPhase phase) {
  return scrollWheelEventWithPhase(phase, NSEventPhaseNone);
}

void MacHistorySwiperTest::startGestureInMiddle() {
  NSEvent* event = mockEventWithPoint(makePoint(0.5, 0.5), NSEventTypeGesture);
  [historySwiper_ touchesBeganWithEvent:event];
  [historySwiper_ beginGestureWithEvent:event];
  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseBegan);
  [historySwiper_ handleEvent:scrollEvent];
}

void MacHistorySwiperTest::moveGestureInMiddle() {
  moveGestureAtPoint(makePoint(0.5, 0.5));

  // Callbacks from blink to set the relevant state for history swiping.
  rendererACKForBeganEvent();
}

void MacHistorySwiperTest::moveGestureAtPoint(NSPoint point) {
  NSEvent* event = mockEventWithPoint(point, NSEventTypeGesture);
  [historySwiper_ touchesMovedWithEvent:event];

  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseChanged);
  [historySwiper_ handleEvent:scrollEvent];
}

void MacHistorySwiperTest::momentumMoveGestureAtPoint(NSPoint point) {
  NSEvent* event = mockEventWithPoint(point, NSEventTypeGesture);
  [historySwiper_ touchesMovedWithEvent:event];

  NSEvent* scrollEvent =
      scrollWheelEventWithPhase(NSEventPhaseNone, NSEventPhaseChanged);
  [historySwiper_ handleEvent:scrollEvent];
}

void MacHistorySwiperTest::endGestureAtPoint(NSPoint point) {
  NSEvent* event = mockEventWithPoint(point, NSEventTypeGesture);
  [historySwiper_ touchesEndedWithEvent:event];

  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseEnded);
  [historySwiper_ handleEvent:scrollEvent];

  sendEndGestureEventAtPoint(point);
}

void MacHistorySwiperTest::onOverscrolled(
    cc::OverscrollBehavior::Type behavior) {
  ui::DidOverscrollParams params;
  params.overscroll_behavior.x = behavior;
  [historySwiper_ onOverscrolled:params];
}

void MacHistorySwiperTest::rendererACKForBeganEvent() {
  blink::WebMouseWheelEvent event;
  event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  [historySwiper_ rendererHandledWheelEvent:event consumed:NO];
}

void MacHistorySwiperTest::sendBeginGestureEventInMiddle() {
  NSEvent* event = mockEventWithPoint(makePoint(0.5, 0.5), NSEventTypeGesture);
  [historySwiper_ beginGestureWithEvent:event];
}

void MacHistorySwiperTest::sendEndGestureEventAtPoint(NSPoint point) {
  NSEvent* event = mockEventWithPoint(point, NSEventTypeGesture);
  [historySwiper_ endGestureWithEvent:event];
}

// Test that a simple left-swipe causes navigation.
TEST_F(MacHistorySwiperTest, SwipeLeft) {
  startGestureInMiddle();
  moveGestureInMiddle();
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  moveGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  endGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_TRUE(navigated_left_);
  EXPECT_TRUE(got_backwards_hint_);
}

// Test that a simple right-swipe causes navigation.
TEST_F(MacHistorySwiperTest, SwipeRight) {
  startGestureInMiddle();
  moveGestureInMiddle();
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  moveGestureAtPoint(makePoint(0.8, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  endGestureAtPoint(makePoint(0.8, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_TRUE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
  EXPECT_FALSE(got_backwards_hint_);
}

// If the user doesn't swipe enough, the history swiper should begin, but the
// browser should not navigate.
TEST_F(MacHistorySwiperTest, SwipeLeftSmallAmount) {
  startGestureInMiddle();
  moveGestureInMiddle();
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);
  moveGestureAtPoint(makePoint(0.45, 0.5));
  endGestureAtPoint(makePoint(0.45, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  // Even though the gesture did not result in a navigation, it was far enough
  // along to produce a hint of a possible back navigation.
  EXPECT_TRUE(got_backwards_hint_);
}

// Diagonal swipes with a slight horizontal bias should not start the history
// swiper.
TEST_F(MacHistorySwiperTest, SwipeDiagonal) {
  startGestureInMiddle();
  moveGestureInMiddle();
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);
  moveGestureInMiddle();
  moveGestureAtPoint(makePoint(0.6, 0.59));
  endGestureAtPoint(makePoint(0.6, 0.59));

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// Swiping left and then down should cancel the history swiper without
// navigating.
TEST_F(MacHistorySwiperTest, SwipeLeftThenDown) {
  startGestureInMiddle();
  moveGestureInMiddle();
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);
  moveGestureAtPoint(makePoint(0.4, 0.5));
  moveGestureAtPoint(makePoint(0.4, 0.3));
  endGestureAtPoint(makePoint(0.2, 0.2));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// Sometimes Cocoa gets confused and sends us a momentum swipe event instead of
// a swipe gesture event. Momentum events should not cause history swiping.
TEST_F(MacHistorySwiperTest, MomentumSwipeLeft) {
  startGestureInMiddle();

  // Send a momentum move gesture.
  momentumMoveGestureAtPoint(makePoint(0.5, 0.5));
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  // Callbacks from blink to set the relevant state for history swiping.
  rendererACKForBeganEvent();

  momentumMoveGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  endGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
}

// Momentum scroll events for magic mouse should not attempt to trigger the
// `trackSwipeEventWithOptions:` api, as that throws an exception.
TEST_F(MacHistorySwiperTest, MagicMouseMomentumSwipe) {
  // Magic mouse events don't generate 'touches*' callbacks.
  NSEvent* event = mockEventWithPoint(makePoint(0.5, 0.5), NSEventTypeGesture);
  [historySwiper_ beginGestureWithEvent:event];
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);
  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseBegan);
  [historySwiper_ handleEvent:scrollEvent];

  // Callbacks from blink to set the relevant state for history swiping.
  rendererACKForBeganEvent();

  // Send a momentum move gesture.
  scrollEvent =
      scrollWheelEventWithPhase(NSEventPhaseNone, NSEventPhaseChanged, 5.0, 0);
  [historySwiper_ handleEvent:scrollEvent];

  EXPECT_FALSE(magic_mouse_history_swipe_);
}

// User starts a swipe but doesn't move.
TEST_F(MacHistorySwiperTest, NoSwipe) {
  startGestureInMiddle();
  moveGestureInMiddle();
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);

  // Starts the gesture.
  moveGestureAtPoint(makePoint(0.44, 0.44));

  // No movement.
  endGestureAtPoint(makePoint(0.44, 0.44));

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// After a gesture is successfully recognized, momentum events should be
// swallowed, but new events should pass through.
TEST_F(MacHistorySwiperTest, TouchEventAfterGestureFinishes) {
  // Successfully pass through a gesture.
  startGestureInMiddle();
  moveGestureInMiddle();
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);
  moveGestureAtPoint(makePoint(0.8, 0.5));
  endGestureAtPoint(makePoint(0.8, 0.5));
  EXPECT_TRUE(navigated_right_);

  // Momentum events should be swallowed.
  NSEvent* momentumEvent = scrollWheelEventWithPhase(NSEventPhaseNone,
                                                     NSEventPhaseChanged);
  EXPECT_TRUE([historySwiper_ handleEvent:momentumEvent]);

  // New events should not be swallowed.
  NSEvent* beganEvent = scrollWheelEventWithPhase(NSEventPhaseBegan);
  EXPECT_FALSE([historySwiper_ handleEvent:beganEvent]);
}

// The history swipe logic should be resilient against the timing of the
// different callbacks that result from scrolling.
TEST_F(MacHistorySwiperTest, SwipeRightEventOrdering) {
  // Touches began.
  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseBegan);
  NSEvent* event = mockEventWithPoint(makePoint(0.5, 0.5), NSEventTypeGesture);
  [historySwiper_ touchesBeganWithEvent:event];
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);
  [historySwiper_ handleEvent:scrollEvent];
  rendererACKForBeganEvent();

  // Touches moved.
  moveGestureAtPoint(makePoint(0.5, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  // Touches moved.
  moveGestureAtPoint(makePoint(0.52, 0.5));

  // Begin gesture callback is delayed.
  [historySwiper_ beginGestureWithEvent:event];

  // Touches moved.
  moveGestureAtPoint(makePoint(0.52, 0.5));

  // Complete the rest of the gesture.
  moveGestureAtPoint(makePoint(0.60, 0.5));
  scrollEvent = scrollWheelEventWithPhase(NSEventPhaseChanged);
  [historySwiper_ handleEvent:scrollEvent];
  endGestureAtPoint(makePoint(0.70, 0.5));

  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_TRUE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// Substantial vertical scrolling followed by horizontal scrolling should not
// result in navigation.
TEST_F(MacHistorySwiperTest, SubstantialVerticalThenHorizontal) {
  startGestureInMiddle();
  moveGestureInMiddle();
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);

  // Move up, then move down.
  for (CGFloat y = 0.51; y < 0.6; y += 0.01)
    moveGestureAtPoint(makePoint(0.5, y));
  for (CGFloat y = 0.59; y > 0.5; y -= 0.01)
    moveGestureAtPoint(makePoint(0.5, y));

  // Large movement to the right.
  moveGestureAtPoint(makePoint(0.6, 0.51));
  endGestureAtPoint(makePoint(0.6, 0.51));

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// Magic Mouse gestures don't send -touches*WithEvent: callbacks. The history
// swiper should still handle this gracefully. It should not turn vertical
// motion into history swipes.
TEST_F(MacHistorySwiperTest, MagicMouseStateResetsCorrectly) {
  // Magic mouse events don't generate '-touches*WithEvent:' callbacks.
  // Send the following events:
  //  - beginGesture
  //  - scrollWheel: (phase=Began)
  //  - scrollWheel: (phase=Changed), significant horizontal motion.
  //  - scrollWheel: (phase=Ended)
  //  - endGesture
  sendBeginGestureEventInMiddle();
  [historySwiper_ handleEvent:scrollWheelEventWithPhase(NSEventPhaseBegan)];
  onOverscrolled(cc::OverscrollBehavior::Type::kAuto);

  // Callback from Blink to set the relevant state for history swiping.
  rendererACKForBeganEvent();

  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseChanged,
                                                   NSEventPhaseNone, 200.0, 0);
  [historySwiper_ handleEvent:scrollEvent];
  [historySwiper_ handleEvent:scrollWheelEventWithPhase(NSEventPhaseEnded)];
  sendEndGestureEventAtPoint(makePoint(0.7, 0.5));

  // Expect this sequence of events to trigger a magic mouse history swipe.
  EXPECT_TRUE(magic_mouse_history_swipe_);

  // Reset state.
  magic_mouse_history_swipe_ = false;

  // Send the following events:
  //  - beginGesture
  //  - scrollWheel: (phase=Began)
  //  - scrollWheel: (phase=Changed), significant vertical motion.
  //  - scrollWheel: (phase=Ended)
  //  - endGesture
  sendBeginGestureEventInMiddle();
  [historySwiper_ handleEvent:scrollWheelEventWithPhase(NSEventPhaseBegan)];

  // Callback from Blink to set the relevant state for history swiping.
  rendererACKForBeganEvent();

  scrollEvent =
      scrollWheelEventWithPhase(NSEventPhaseChanged, NSEventPhaseNone, 0, 20);
  [historySwiper_ handleEvent:scrollEvent];
  [historySwiper_ handleEvent:scrollWheelEventWithPhase(NSEventPhaseEnded)];
  sendEndGestureEventAtPoint(makePoint(0.5, 0.7));

  // Vertical motion should never trigger a history swipe!
  EXPECT_FALSE(magic_mouse_history_swipe_);
}

// With overscroll-behavior value as contain, the page should not navigate,
// nor should the history overlay appear.
TEST_F(MacHistorySwiperTest, OverscrollBehaviorContainPreventsNavigation) {
  startGestureInMiddle();
  moveGestureInMiddle();

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  onOverscrolled(cc::OverscrollBehavior::Type::kContain);
  moveGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  endGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// With overscroll-behavior value as none, the page should not navigate,
// nor should the history overlay appear.
TEST_F(MacHistorySwiperTest, OverscrollBehaviorNonePreventsNavigation) {
  startGestureInMiddle();
  moveGestureInMiddle();

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  onOverscrolled(cc::OverscrollBehavior::Type::kNone);
  moveGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  endGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// Without overscroll msg from renderer, the page should not navigate, nor
// should the history overlay appear.
TEST_F(MacHistorySwiperTest, NoNavigationWithoutOverscrollMsg) {
  startGestureInMiddle();
  moveGestureInMiddle();

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  moveGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  endGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}
