// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_controller_impl.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace ttc {
namespace {

class SessionControllerImplGetPointerBrowserTest
    : public TtcCoreBrowserTestBase {
 public:
  SessionControllerImplGetPointerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kTtcGetPointer);
  }

  // Starts a session and waits for it to learn of the active tab, which the
  // contents tracker reports asynchronously; the session then invalidates the
  // conversation's page context.
  SessionControllerImpl& StartSession() {
    ttc_service().StartSession();
    base::RunLoop run_loop;
    EXPECT_CALL(*conversation(), OnPageContextInvalidated())
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(conversation());
    return *static_cast<SessionControllerImpl*>(
        ttc_service().session_controller());
  }

  // Clicks at `point` and returns the tracker's locations once the click has
  // reached the renderer. A click rather than a move, since the tracker drops
  // moves that follow another sample too closely.
  std::vector<PointerTracker::Location> PressAndResolve(
      SessionControllerImpl& controller,
      const gfx::Point& point) {
    content::InputEventAckWaiter ack_waiter(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kMouseDown);
    content::SimulateMouseEvent(web_contents(),
                                blink::WebInputEvent::Type::kMouseDown, point);
    ack_waiter.Wait();
    return controller.pointer_tracker()->ResolveLocations(
        controller.user_turn_start(), base::TimeTicks::Now());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// With the feature on, the session records the pointer over the voice-focused
// contents, and stops when there are none.
IN_PROC_BROWSER_TEST_F(SessionControllerImplGetPointerBrowserTest,
                       TracksPointerOverVoiceFocusedContents) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));
  SessionControllerImpl& controller = StartSession();
  ASSERT_TRUE(controller.pointer_tracker());
  controller.OnSpeakerChanged(Speaker::kUser);

  std::vector<PointerTracker::Location> locations =
      PressAndResolve(controller, gfx::Point(50, 60));
  ASSERT_EQ(locations.size(), 1u);
  EXPECT_POINTF_EQ(gfx::PointF(50, 60), locations[0].point);

  // Losing the contents forgets their positions and stops recording.
  controller.OnVoiceFocusedContentsChanged(nullptr);
  EXPECT_TRUE(PressAndResolve(controller, gfx::Point(70, 80)).empty());

  // Getting contents again resumes.
  controller.OnVoiceFocusedContentsChanged(web_contents());
  locations = PressAndResolve(controller, gfx::Point(90, 100));
  ASSERT_EQ(locations.size(), 1u);
  EXPECT_POINTF_EQ(gfx::PointF(90, 100), locations[0].point);
}

}  // namespace
}  // namespace ttc
