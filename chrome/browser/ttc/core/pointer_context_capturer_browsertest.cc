// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_context_capturer.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace ttc {
namespace {

using optimization_guide::proto::ContentNode;

// Returns whether any node below `node` is hit-testable, i.e. has a z-order
// and so can be found by a hit test of the page content.
bool HasHitTestableDescendant(const ContentNode& node) {
  for (const ContentNode& child : node.children_nodes()) {
    if (child.content_attributes()
            .interaction_info()
            .has_document_scoped_z_order() ||
        HasHitTestableDescendant(child)) {
      return true;
    }
  }
  return false;
}

class PointerContextCapturerBrowserTest : public TtcCoreBrowserTestBase {
 public:
  void NavigateToPageWithText() {
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(), embedded_test_server()->GetURL("/simple.html")));
    content::WaitForCopyableViewInWebContents(web_contents());
  }
};

// Runs at several device scale factors, so that what is tested does not depend
// on the display of the machine running the test.
class PointerContextCapturerScaleBrowserTest
    : public PointerContextCapturerBrowserTest,
      public testing::WithParamInterface<float> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PointerContextCapturerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::NumberToString(GetParam()));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PointerContextCapturerScaleBrowserTest,
                         testing::Values(1.0f, 1.5f, 2.0f));

IN_PROC_BROWSER_TEST_P(PointerContextCapturerScaleBrowserTest,
                       CapturesScreenshotAndPageContent) {
  NavigateToPageWithText();
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  ASSERT_EQ(view->GetDeviceScaleFactor(), GetParam());

  PointerContextCapturer capturer(TestTimeouts::action_timeout());
  base::test::TestFuture<PointerContextCapturer::Result> future;
  capturer.Capture(web_contents(), future.GetCallback());
  PointerContextCapturer::Result result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->scale_factor, GetParam());

  // The screenshot is the viewport in device pixels.
  const SkBitmap screenshot = gfx::JPEGCodec::Decode(result->screenshot);
  ASSERT_FALSE(screenshot.isNull());
  const gfx::Size expected_size =
      gfx::ScaleToRoundedSize(view->GetViewBounds().size(), GetParam());
  EXPECT_NEAR(screenshot.width(), expected_size.width(), 1);
  EXPECT_NEAR(screenshot.height(), expected_size.height(), 1);

  // The page content has geometry, and its text can be hit-tested. The size
  // of the root's box is not checked: it is smaller than the viewport when
  // the page is zoomed in, as it is by default on Android desktop.
  const ContentNode& root = result->page_content.root_node();
  EXPECT_TRUE(root.content_attributes().geometry().has_visible_bounding_box());
  EXPECT_TRUE(HasHitTestableDescendant(root));
}

IN_PROC_BROWSER_TEST_F(PointerContextCapturerBrowserTest, TimesOut) {
  NavigateToPageWithText();
  // No time at all: the capture cannot finish before the timer fires.
  PointerContextCapturer capturer(base::TimeDelta{});
  base::test::TestFuture<PointerContextCapturer::Result> future;
  capturer.Capture(web_contents(), future.GetCallback());
  PointerContextCapturer::Result result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error().code,
            actor::mojom::ActionResultCode::kToolTimeout);

  // The capturer can be used again.
  capturer.Capture(web_contents(), future.GetCallback());
  EXPECT_FALSE(future.Take().has_value());
}

IN_PROC_BROWSER_TEST_F(PointerContextCapturerBrowserTest, CrashedTab) {
  NavigateToPageWithText();
  content::CrashTab(web_contents());
  PointerContextCapturer capturer(TestTimeouts::action_timeout());
  base::test::TestFuture<PointerContextCapturer::Result> future;
  capturer.Capture(web_contents(), future.GetCallback());
  PointerContextCapturer::Result result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error().code,
            actor::mojom::ActionResultCode::kRendererCrashed);
}

}  // namespace
}  // namespace ttc
