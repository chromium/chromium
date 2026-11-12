// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_input_observer.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace ttc {
namespace {

class PointerInputObserverBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpCommandLine(command_line);
    // Android phones do not isolate sites by default, and the iframe test
    // needs the cross-site iframe in its own process.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    // AndroidBrowserTest serves chrome/test/data over HTTPS only.
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  // Where the tracker says the user pointed since the test began.
  std::vector<PointerTracker::Location> Resolve() {
    return tracker_.ResolveLocations(turn_start_, base::TimeTicks::Now());
  }

  // Sends a mouse event at `point` (in the contents' coordinates) and waits
  // for the tracker to record something; nullopt if it never does. Events are
  // delivered asynchronously.
  std::optional<PointerTracker::Location> SendAndResolve(
      blink::WebInputEvent::Type type,
      const gfx::Point& point) {
    tracker_.Clear();
    content::SimulateMouseEvent(web_contents(), type, point);
    std::vector<PointerTracker::Location> locations;
    if (!base::test::RunUntil([&] {
          locations = Resolve();
          return !locations.empty();
        })) {
      return std::nullopt;
    }
    return locations.front();
  }

  // Sends a mouse event at `point` and returns whether the tracker recorded
  // nothing by the time the renderer acked it. Observers are notified before
  // the ack.
  bool SendAndExpectNothing(blink::WebInputEvent::Type type,
                            const gfx::Point& point) {
    tracker_.Clear();
    content::InputEventAckWaiter ack_waiter(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(), type);
    content::SimulateMouseEvent(web_contents(), type, point);
    ack_waiter.Wait();
    return Resolve().empty();
  }

 protected:
  const base::TimeTicks turn_start_ = base::TimeTicks::Now();
  PointerTracker tracker_;
};

IN_PROC_BROWSER_TEST_F(PointerInputObserverBrowserTest,
                       RecordsMouseMovesAndPresses) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));
  PointerInputObserver observer(&tracker_, web_contents());

  std::optional<PointerTracker::Location> location = SendAndResolve(
      blink::WebInputEvent::Type::kMouseMove, gfx::Point(50, 60));
  ASSERT_TRUE(location);
  EXPECT_POINTF_EQ(gfx::PointF(50, 60), location->point);
  EXPECT_FALSE(location->is_press);

  location = SendAndResolve(blink::WebInputEvent::Type::kMouseDown,
                            gfx::Point(70, 80));
  ASSERT_TRUE(location);
  EXPECT_POINTF_EQ(gfx::PointF(70, 80), location->point);
  EXPECT_TRUE(location->is_press);

  // Releases are not pointing.
  EXPECT_TRUE(SendAndExpectNothing(blink::WebInputEvent::Type::kMouseUp,
                                   gfx::Point(70, 80)));
}

// Events over an out-of-process iframe go to the iframe's own widget, in its
// own coordinates; they must still be recorded in root coordinates. The
// iframe is created while observing, so its widget is picked up as it appears.
IN_PROC_BROWSER_TEST_F(PointerInputObserverBrowserTest,
                       RecordsOutOfProcessIframeEventsInRootCoordinates) {
  PointerInputObserver observer(&tracker_, web_contents());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html")));
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* child = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child);
  ASSERT_NE(child->GetRenderWidgetHost(), main_frame->GetRenderWidgetHost())
      << "iframe is not out of process; the test would prove nothing";
  content::WaitForHitTestData(child);

  // A point well inside the iframe, in viewport DIPs: on Android the page has
  // no viewport meta tag, so it is laid out wider than the screen and scaled
  // down, and CSS pixels are not DIPs.
  const content::EvalJsResult center_result = content::EvalJs(main_frame, R"(
        (() => {
          const r = document.getElementById('frame1').getBoundingClientRect();
          const s = visualViewport.scale;
          return [(r.left + r.width / 2) * s, (r.top + r.height / 2) * s];
        })())");
  const base::ListValue& center_xy = center_result.ExtractList();
  const gfx::PointF center(center_xy[0].GetDouble(), center_xy[1].GetDouble());

  // The first move into the iframe also sends the root widget a synthetic
  // move at the same spot, which would be recorded whether or not the child's
  // widget is observed. So move in first, then test with a press, which only
  // the child's widget sees.
  content::InputEventAckWaiter child_move(
      child->GetRenderWidgetHost(), blink::WebInputEvent::Type::kMouseMove);
  content::SimulateMouseEvent(web_contents(),
                              blink::WebInputEvent::Type::kMouseMove,
                              gfx::ToRoundedPoint(center));
  child_move.Wait();
  content::InputEventAckWaiter child_press(
      child->GetRenderWidgetHost(), blink::WebInputEvent::Type::kMouseDown);
  std::optional<PointerTracker::Location> location = SendAndResolve(
      blink::WebInputEvent::Type::kMouseDown, gfx::ToRoundedPoint(center));
  child_press.Wait();

  ASSERT_TRUE(location);
  EXPECT_TRUE(location->is_press);
  EXPECT_POINTF_NEAR(center, location->point, 1.0f);
}

// A cross-site navigation replaces the main frame's widget; the new one is
// observed and the old one's destruction is survived.
IN_PROC_BROWSER_TEST_F(PointerInputObserverBrowserTest,
                       FollowsCrossSiteNavigation) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  PointerInputObserver observer(&tracker_, web_contents());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // Otherwise a.com's widget may live on in the back/forward cache.
  web_contents()->GetController().GetBackForwardCache().Flush();

  std::optional<PointerTracker::Location> location = SendAndResolve(
      blink::WebInputEvent::Type::kMouseMove, gfx::Point(30, 40));
  ASSERT_TRUE(location);
  EXPECT_POINTF_EQ(gfx::PointF(30, 40), location->point);
}

// Positions refer to the page they were recorded on, so a new page in the
// same tab starts afresh.
IN_PROC_BROWSER_TEST_F(PointerInputObserverBrowserTest,
                       ForgetsThePreviousPage) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));
  PointerInputObserver observer(&tracker_, web_contents());
  std::optional<PointerTracker::Location> location = SendAndResolve(
      blink::WebInputEvent::Type::kMouseDown, gfx::Point(70, 80));
  ASSERT_TRUE(location);
  ASSERT_TRUE(location->is_press);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title2.html")));
  // The click is gone. Not checking for no locations at all: the window
  // system may send the new page a mouse move for where the real mouse is.
  EXPECT_TRUE(
      std::ranges::none_of(Resolve(), &PointerTracker::Location::is_press));
}

IN_PROC_BROWSER_TEST_F(PointerInputObserverBrowserTest,
                       StopsRecordingWhenDestroyed) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));
  {
    PointerInputObserver observer(&tracker_, web_contents());
  }
  EXPECT_TRUE(SendAndExpectNothing(blink::WebInputEvent::Type::kMouseMove,
                                   gfx::Point(50, 60)));
}

}  // namespace
}  // namespace ttc
