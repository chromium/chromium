// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"
#include "ui/events/base_event_utils.h"
#include "url/gurl.h"

namespace {

// The swiper code relies on both NSTouches (-touchesBeganWithEvent:,
// -touchesMovedWithEvent:, -touchesEndedWithEvent: messages) and -scrollWheel:
// messages. This enum is used to indicate which message should be used to
// replay the event.
enum class Deployment {
  // -[NSView touchesBeganWithEvent:]
  kTouchesBegan,
  // -[NSView touchesMovedWithEvent:]
  kTouchesMoved,
  // -[NSView touchesEndedWithEvent:]
  kTouchesEnded,
  // -[NSView scrollWheel:]
  kScrollWheel,
};

enum class AfterReplayMessageLoop {
  kDoNotRun,
  kRun,
};

}  // namespace

// A wrapper object for events queued for replay.
@interface QueuedEvent : NSObject

// Whether the message loop should be run after this event has been replayed.
@property(nonatomic, assign) AfterReplayMessageLoop afterReplayMessageLoop;
// How this event should be replayed.
@property(nonatomic, assign) Deployment deployment;
// The event to be replayed.
@property(nonatomic, strong) NSEvent* event;
@end

@implementation QueuedEvent

@synthesize deployment = _deployment;
@synthesize event = _event;
@synthesize afterReplayMessageLoop = _afterReplayMessageLoop;

@end

class ChromeRenderWidgetHostViewMacHistorySwiperTest
    : public InProcessBrowserTest {
 public:
  ChromeRenderWidgetHostViewMacHistorySwiperTest() {
    const base::FilePath base_path(FILE_PATH_LITERAL("scroll"));
    url1_ = chrome_test_utils::GetTestUrl(
        base_path, base::FilePath(FILE_PATH_LITERAL("text.html")));
    url2_ = chrome_test_utils::GetTestUrl(
        base_path, base::FilePath(FILE_PATH_LITERAL("blank.html")));
    url_iframe_ = chrome_test_utils::GetTestUrl(
        base_path, base::FilePath(FILE_PATH_LITERAL("iframe.html")));
  }

  ChromeRenderWidgetHostViewMacHistorySwiperTest(
      const ChromeRenderWidgetHostViewMacHistorySwiperTest&) = delete;
  ChromeRenderWidgetHostViewMacHistorySwiperTest& operator=(
      const ChromeRenderWidgetHostViewMacHistorySwiperTest&) = delete;

  void SetUpOnMainThread() override {
    event_queue_ = [[NSMutableArray alloc] init];
    touch_ = CGPointMake(0.5, 0.5);

    // Ensure that the navigation stack is not empty.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1_));
    ASSERT_EQ(url1_, GetWebContents()->GetURL());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2_));
    ASSERT_EQ(url2_, GetWebContents()->GetURL());

    mock_clock_.Advance(base::Milliseconds(100));
    ui::SetEventTickClockForTesting(&mock_clock_);
  }

  void TearDownOnMainThread() override {
    ui::SetEventTickClockForTesting(nullptr);
    event_queue_ = nil;
  }

 protected:
  // Returns the active web contents.
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Returns the value of `query` from Javascript as an int.
  int GetScriptIntValue(const std::string& query) {
    return content::EvalJs(GetWebContents(), query).ExtractInt();
  }

  // Returns the vertical scroll offset of the current page.
  int GetScrollTop() { return GetScriptIntValue("document.body.scrollTop"); }

  // Create mock events --------------------------------------------------------

  // Creates a mock scroll wheel event that is backed by a real CGEvent.
  id MockScrollWheelEvent(NSPoint delta, NSEventType type) {
    // This simulates a Magic Mouse scroll event with two wheels, the first
    // being vertical and the second being horizontal. This is the standard
    // scroll event behind the scenes.
    base::apple::ScopedCFTypeRef<CGEventRef> cg_event(
        CGEventCreateScrollWheelEvent(/*source=*/nullptr,
                                      kCGScrollEventUnitLine, /*wheelCount=*/2,
                                      /*wheel1=*/0, /*wheel2=*/0));
    CGEventSetIntegerValueField(cg_event.get(), kCGScrollWheelEventIsContinuous,
                                1);
    CGEventSetIntegerValueField(cg_event.get(),
                                kCGScrollWheelEventPointDeltaAxis2, delta.x);
    CGEventSetIntegerValueField(cg_event.get(),
                                kCGScrollWheelEventPointDeltaAxis1, delta.y);
    NSEvent* event = [NSEvent eventWithCGEvent:cg_event.get()];

    id mock_event = [OCMockObject partialMockForObject:event];
    [[[mock_event stub] andReturnBool:NO] isDirectionInvertedFromDevice];
    // An arbitrary non-zero timestamp (as a zero value will trigger DCHECKs).
    NSTimeInterval timestamp = 1;
    [(NSEvent*)[[mock_event stub] andReturnValue:OCMOCK_VALUE(timestamp)]
        timestamp];
    [(NSEvent*)[[mock_event stub] andReturnValue:OCMOCK_VALUE(type)] type];

    // We need to assign a locationInWindow for the event so that the wheel
    // event happens inside the page.
    NSPoint locationInWindow = NSMakePoint(400, 400);
    [(NSEvent*)[[mock_event stub] andReturnValue:OCMOCK_VALUE(locationInWindow)]
        locationInWindow];

    return mock_event;
  }

  // Returns a scroll wheel event with the given parameters.
  id ScrollWheelEventWithPhase(NSEventPhase phase,
                               NSEventPhase momentum_phase,
                               CGFloat scrolling_delta_x,
                               CGFloat scrolling_delta_y) {
    id event =
        MockScrollWheelEvent(NSMakePoint(scrolling_delta_x, scrolling_delta_y),
                             NSEventTypeScrollWheel);
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(phase)] phase];
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(momentum_phase)]
        momentumPhase];
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(scrolling_delta_x)]
        scrollingDeltaX];
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(scrolling_delta_y)]
        scrollingDeltaY];
    NSUInteger modifierFlags = 0;
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(modifierFlags)]
        modifierFlags];
    // An arbitrary non-zero timestamp (as a zero value will trigger DCHECKs).
    NSTimeInterval timestamp = 1;
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(timestamp)] timestamp];

    NSView* view = GetWebContents()
                       ->GetRenderViewHost()
                       ->GetWidget()
                       ->GetView()
                       ->GetNativeView()
                       .GetNativeNSView();
    NSWindow* window = [view window];
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(window)] window];

    return event;
  }

  // Queue events for playback -------------------------------------------------

  void QueueEvent(id event,
                  Deployment deployment,
                  AfterReplayMessageLoop after_replay_message_loop) {
    QueuedEvent* queued_event = [[QueuedEvent alloc] init];
    queued_event.event = event;
    queued_event.deployment = deployment;
    queued_event.afterReplayMessageLoop = after_replay_message_loop;
    [event_queue_ addObject:queued_event];
  }

  // Queues a trackpad scroll event (e.g. -[NSView scrollWheel:])
  void QueueTrackpadScroll(int dx,
                           int dy,
                           NSEventPhase phase,
                           AfterReplayMessageLoop after_replay_message_loop) {
    id event = ScrollWheelEventWithPhase(phase, NSEventPhaseNone, dx, dy);
    QueueEvent(event, Deployment::kScrollWheel, after_replay_message_loop);
  }

  // Queues a touch event with absolute coordinates `x` and `y`.
  void QueueTouch(CGFloat x,
                  CGFloat y,
                  Deployment deployment,
                  NSEventType type,
                  short subtype,
                  AfterReplayMessageLoop after_replay_message_loop) {
    id event = [OCMockObject mockForClass:[NSEvent class]];
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(type)] type];
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(subtype)] subtype];

    id mock_touch = [OCMockObject mockForClass:[NSTouch class]];
    [[[mock_touch stub] andReturnNSPoint:NSMakePoint(x, y)] normalizedPosition];
    NSArray* touches = @[ mock_touch ];
    [[[event stub] andReturn:touches] touchesMatchingPhase:NSTouchPhaseAny
                                                    inView:[OCMArg any]];
    [[[event stub] andReturnBool:NO] isDirectionInvertedFromDevice];
    // An arbitrary non-zero timestamp (as a zero value will trigger DCHECKs).
    NSTimeInterval timestamp = 1;
    [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(timestamp)] timestamp];

    QueueEvent(event, deployment, after_replay_message_loop);
  }

  // Convenience methods for event queuing -------------------------------------

  // Trackpad scroll events are roughly related to touch events. Given a
  // trackpad scroll delta, approximate the change to the touch event.
  void UpdateTouchLocationFromTrackpadScroll(int dx, int dy) {
    touch_.x -= dx * 0.001;
    touch_.y -= dy * 0.001;
  }

  // Queue the typical events at the beginning of a new swipe gesture. The
  // ordering and values were determined by recording real swipe events.
  void QueueBeginningEvents(int dx, int dy) {
    QueueTouch(Deployment::kTouchesBegan, NSEventTypeGesture,
               NSEventSubtypeMouseEvent, AfterReplayMessageLoop::kDoNotRun);
    QueueTrackpadScroll(0, 0, NSEventPhaseMayBegin,
                        AfterReplayMessageLoop::kRun);
    QueueTouch(Deployment::kTouchesMoved, NSEventTypeGesture,
               NSEventSubtypeMouseEvent, AfterReplayMessageLoop::kDoNotRun);

    QueueTrackpadScroll(dx, dy, NSEventPhaseBegan,
                        AfterReplayMessageLoop::kDoNotRun);
    QueueTouch(Deployment::kTouchesMoved, NSEventTypeBeginGesture,
               NSEventSubtypeTouch, AfterReplayMessageLoop::kDoNotRun);
    QueueTouch(Deployment::kTouchesMoved, NSEventTypeGesture,
               NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
    UpdateTouchLocationFromTrackpadScroll(dx, dy);
    QueueTouch(Deployment::kTouchesMoved, NSEventTypeGesture,
               NSEventSubtypeTouch, AfterReplayMessageLoop::kDoNotRun);
  }

  // Queue the typical events at the end of a new swipe gesture. The ordering
  // and values were determined by recording real swipe events.
  void QueueEndEvents() {
    QueueTouch(Deployment::kTouchesMoved, NSEventTypeEndGesture,
               NSEventSubtypeMouseEvent, AfterReplayMessageLoop::kDoNotRun);
    QueueTouch(Deployment::kTouchesEnded, NSEventTypeEndGesture,
               NSEventSubtypeMouseEvent, AfterReplayMessageLoop::kDoNotRun);
    QueueTrackpadScroll(0, 0, NSEventPhaseEnded, AfterReplayMessageLoop::kRun);
  }

  // Queues a trackpad scroll movement and a touch movement event.
  void QueueScrollAndTouchMoved(int dx, int dy) {
    QueueTrackpadScroll(dx, dy, NSEventPhaseChanged,
                        AfterReplayMessageLoop::kDoNotRun);
    UpdateTouchLocationFromTrackpadScroll(dx, dy);
    QueueTouch(Deployment::kTouchesMoved, NSEventTypeGesture,
               NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  }

  // Queues a touch event with the stored touch coordinates.
  void QueueTouch(Deployment deployment,
                  NSEventType type,
                  short subtype,
                  AfterReplayMessageLoop afterReplayMessageLoop) {
    QueueTouch(touch_.x, touch_.y, deployment, type, subtype,
               afterReplayMessageLoop);
  }

  // Replays the events from the queue.
  void RunQueuedEvents() {
    while (event_queue_.count) {
      QueuedEvent* queued_event = event_queue_.firstObject;
      NSEvent* event = queued_event.event;
      NSView* view = GetWebContents()
                         ->GetRenderViewHost()
                         ->GetWidget()
                         ->GetView()
                         ->GetNativeView()
                         .GetNativeNSView();
      switch (queued_event.deployment) {
        case Deployment::kScrollWheel:
          [view scrollWheel:event];
          break;
        case Deployment::kTouchesBegan:
          [view touchesBeganWithEvent:event];
          break;
        case Deployment::kTouchesEnded:
          [view touchesEndedWithEvent:event];
          break;
        case Deployment::kTouchesMoved:
          [view touchesMovedWithEvent:event];
          break;
      }

      [event_queue_ removeObjectAtIndex:0];

      if (queued_event.afterReplayMessageLoop ==
          AfterReplayMessageLoop::kDoNotRun) {
        continue;
      }

      // Give time for the IPC to make it to the renderer process. If the IPC
      // doesn't have time to make it to the renderer process, that's okay,
      // since that simulates realistic conditions.
      [NSRunLoop.currentRunLoop
          runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]];
      // The renderer process returns an IPC, which needs to be handled.
      base::RunLoop().RunUntilIdle();
    }
  }

  void ExpectUrlAndOffset(const GURL& url, int offset) {
    EXPECT_TRUE(content::WaitForLoadStop(GetWebContents()));
    EXPECT_EQ(url, GetWebContents()->GetURL());

    const int scroll_offset = GetScrollTop();
    EXPECT_EQ(offset, scroll_offset);
  }

  base::SimpleTestTickClock mock_clock_;

  GURL url1_;
  GURL url2_;
  GURL url_iframe_;
  NSMutableArray* __strong event_queue_ = nil;
  // The current location of the user's fingers on the track pad.
  CGPoint touch_ = CGPointZero;
};

// The ordering, timing, and parameters of the events was determined by
// recording a real swipe.
IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacHistorySwiperTest,
                       TestBackwardsHistoryNavigationRealData) {
  QueueTouch(0.510681, 0.444672, Deployment::kTouchesBegan, NSEventTypeGesture,
             NSEventSubtypeMouseEvent, AfterReplayMessageLoop::kDoNotRun);
  QueueTrackpadScroll(0, 0, NSEventPhaseMayBegin, AfterReplayMessageLoop::kRun);
  QueueTouch(0.510681, 0.444672, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeMouseEvent, AfterReplayMessageLoop::kDoNotRun);

  QueueTrackpadScroll(1, 0, NSEventPhaseBegan,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.510681, 0.444672, Deployment::kTouchesMoved,
             NSEventTypeBeginGesture, NSEventSubtypeTouch,
             AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.510681, 0.444672, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTouch(0.507019, 0.444092, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kDoNotRun);
  QueueTrackpadScroll(3, 0, NSEventPhaseChanged, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(3, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.502861, 0.443512, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(6, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.497002, 0.44294, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(5, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.487236, 0.44149, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(8, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.480392, 0.440628, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.475266, 0.440338, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(6, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTrackpadScroll(10, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.467934, 0.439758, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(6, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.462807, 0.439186, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTrackpadScroll(12, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.454018, 0.438316, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(6, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.449623, 0.438026, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(9, 0, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.443275, 0.437744, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTouch(0.437164, 0.437164, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(9, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.431305, 0.436874, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTrackpadScroll(8, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.425926, 0.436295, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTrackpadScroll(7, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.420311, 0.43573, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(7, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.415184, 0.43544, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTrackpadScroll(6, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.410057, 0.43457, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTouch(0.40493, 0.43399, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTrackpadScroll(7, -1, NSEventPhaseChanged, AfterReplayMessageLoop::kRun);
  QueueTrackpadScroll(3, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.402489, 0.433701, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTrackpadScroll(5, 0, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.398094, 0.433418, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  QueueTrackpadScroll(4, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.394669, 0.433128, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTouch(0.391006, 0.432549, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTrackpadScroll(4, -1, NSEventPhaseChanged,
                      AfterReplayMessageLoop::kDoNotRun);
  QueueTrackpadScroll(5, 0, NSEventPhaseChanged, AfterReplayMessageLoop::kRun);
  QueueTouch(0.386848, 0.432259, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);
  QueueTouch(0.38343, 0.432259, Deployment::kTouchesMoved, NSEventTypeGesture,
             NSEventSubtypeTouch, AfterReplayMessageLoop::kRun);

  // Skipped a bunch of events. The data on the gesture end events are fudged.

  QueueTouch(0.38343, 0.432259, Deployment::kTouchesMoved,
             NSEventTypeEndGesture, NSEventSubtypeMouseEvent,
             AfterReplayMessageLoop::kDoNotRun);
  QueueTouch(0.38343, 0.432259, Deployment::kTouchesEnded,
             NSEventTypeEndGesture, NSEventSubtypeMouseEvent,
             AfterReplayMessageLoop::kDoNotRun);
  QueueTrackpadScroll(0, 0, NSEventPhaseEnded, AfterReplayMessageLoop::kRun);

  RunQueuedEvents();
  ExpectUrlAndOffset(url1_, 0);
}

// Each movement event that has non-zero parameters has both horizontal and
// vertical motion. This should not trigger history navigation.
//
// TODO(https://crbug.com/41121608): Disabled due to the expected offset not
// matching; fix.
IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacHistorySwiperTest,
                       DISABLED_TestAllDiagonalSwipes) {
  QueueBeginningEvents(1, -1);
  for (int i = 0; i < 150; ++i) {
    QueueScrollAndTouchMoved(1, -1);
  }

  QueueEndEvents();
  RunQueuedEvents();
  ExpectUrlAndOffset(url2_, 150);
}

// The movements are equal part diagonal, horizontal, and vertical. This should
// not trigger history navigation.
//
// TODO(https://crbug.com/41121608): Disabled due to the expected offset not
// matching; fix.
IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacHistorySwiperTest,
                       DISABLED_TestStaggeredDiagonalSwipe) {
  QueueBeginningEvents(1, 0);
  for (int i = 0; i < 150; ++i) {
    switch (i % 3) {
      case 0:
        QueueScrollAndTouchMoved(1, -1);
        break;
      case 1:
        QueueScrollAndTouchMoved(0, -1);
        break;
      case 2:
        QueueScrollAndTouchMoved(1, 0);
        break;
      default:
        NOTREACHED();
    }
  }

  QueueEndEvents();
  RunQueuedEvents();

  EXPECT_TRUE(content::WaitForLoadStop(GetWebContents()));
  EXPECT_EQ(url2_, GetWebContents()->GetURL());

  // TODO(https://crbug.com/41108752): Depending on the timing of the IPCs, some
  // of the initial events might be recognized as part of the history swipe, and
  // not forwarded to the renderer, resulting in a non-deterministic scroll
  // offset. This is bad, as some vertical motion is lost. Once the history
  // swiper logic is fixed, this should become a direct comparison between
  // 'scroll_offset' and 100.
  const int scroll_offset = GetScrollTop();
  // TODO(https://crbug.com/41110421): Depending on the timing of the IPCs
  // between Chrome and the renderer, more than 15% of the vertical motion can
  // be lost. This assertion should eventually become an equality comparison
  // against 100.
  EXPECT_GT(scroll_offset, 1);
}

// The movement events are mostly in the horizontal direction, which should
// trigger a history swipe. This should trigger history navigation.
IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacHistorySwiperTest,
                       TestMostlyHorizontal) {
  QueueBeginningEvents(1, 1);
  for (int i = 0; i < 150; ++i) {
    if (i % 10 == 0) {
      QueueScrollAndTouchMoved(0, -1);
    } else if (i % 5 == 0) {
      QueueScrollAndTouchMoved(1, -1);
    } else {
      QueueScrollAndTouchMoved(1, 0);
    }
  }

  QueueEndEvents();
  RunQueuedEvents();
  ExpectUrlAndOffset(url1_, 0);
}

// Each movement event is horizontal, except the first two. This should trigger
// history navigation. This test is DISABLED because it has never worked. Once
// the flaw in the history swiper logic has been corrected, this test should be
// enabled. TODO(https://crbug.com/41108750): Do so.
IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacHistorySwiperTest,
                       DISABLED_TestAllHorizontalButFirst) {
  QueueBeginningEvents(0, -1);
  QueueScrollAndTouchMoved(0, -1);
  for (int i = 0; i < 149; ++i) {
    QueueScrollAndTouchMoved(1, 0);
  }

  QueueEndEvents();
  RunQueuedEvents();
  ExpectUrlAndOffset(url1_, 0);
}

// Initial movements are vertical, and scroll the iframe. Subsequent movements
// are horizontal, and should not trigger history swiping.
IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacHistorySwiperTest,
                       TestIframeHistorySwiping) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_iframe_));
  ASSERT_EQ(url_iframe_, GetWebContents()->GetURL());

  content::InputEventAckWaiter wheel_end_ack_waiter(
      GetWebContents()->GetRenderViewHost()->GetWidget(),
      base::BindRepeating([](blink::mojom::InputEventResultSource,
                             blink::mojom::InputEventResultState,
                             const blink::WebInputEvent& event) {
        return event.GetType() == blink::WebInputEvent::Type::kMouseWheel &&
               static_cast<const blink::WebMouseWheelEvent&>(event).phase ==
                   blink::WebMouseWheelEvent::kPhaseEnded;
      }));

  QueueBeginningEvents(0, -1);
  for (int i = 0; i < 10; ++i) {
    QueueScrollAndTouchMoved(0, -1);
  }
  for (int i = 0; i < 149; ++i) {
    QueueScrollAndTouchMoved(1, 0);
  }

  QueueEndEvents();
  RunQueuedEvents();

  // Wait for the scroll to end.
  wheel_end_ack_waiter.Wait();

  EXPECT_TRUE(content::WaitForLoadStop(GetWebContents()));
  EXPECT_EQ(url_iframe_, GetWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ChromeRenderWidgetHostViewMacHistorySwiperTest,
                       InnerScrollersOverscrollBehaviorPreventsNavigation) {
  const base::FilePath base_path(FILE_PATH_LITERAL("scroll"));
  GURL url_overscroll_behavior = chrome_test_utils::GetTestUrl(
      base_path, base::FilePath(FILE_PATH_LITERAL("overscroll_behavior.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_overscroll_behavior));
  ASSERT_EQ(url_overscroll_behavior, GetWebContents()->GetURL());

  QueueBeginningEvents(1, 0);
  for (int i = 0; i < 10; ++i) {
    QueueScrollAndTouchMoved(10, 0);
  }

  QueueEndEvents();
  RunQueuedEvents();
  // If navigation were to occur, the URL would be url2_.
  ExpectUrlAndOffset(url_overscroll_behavior, 0);
}
