// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_pointer/pointer_context_capturer.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace actor {
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

class PointerContextCapturerBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

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
  // Android applies --force-device-scale-factor by scaling the display's
  // native density, which can round off by 1 ULP (e.g. 1.50000012 at 420dpi).
  ASSERT_NEAR(view->GetDeviceScaleFactor(), GetParam(), 0.001f);

  // On Android the view can still be resized after the page has loaded, as
  // the browser's UI settles. The screenshot of a capture during which that
  // happens has the size that the view had when the capture began. So capture
  // until the view kept its size throughout.
  PointerContextCapturer capturer(TestTimeouts::action_timeout());
  base::test::TestFuture<PointerContextCapturer::Result> future;
  gfx::Size view_size;
  auto capture = [&]() {
    view_size = view->GetViewBounds().size();
    capturer.Capture(web_contents(), future.GetCallback());
  };
  capture();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    if (!future.IsReady()) {
      return false;
    }
    if (view->GetViewBounds().size() == view_size) {
      return true;
    }
    future.Clear();
    capture();
    return false;
  })) << "No capture finished with the view at the size it began with.";
  PointerContextCapturer::Result result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_NEAR(result->scale_factor, GetParam(), 0.001f);

  // The screenshot is the viewport in device pixels.
  const SkBitmap screenshot = gfx::JPEGCodec::Decode(result->screenshot);
  ASSERT_FALSE(screenshot.isNull());
  const gfx::Size expected_size =
      gfx::ScaleToRoundedSize(view_size, GetParam());
  EXPECT_NEAR(screenshot.width(), expected_size.width(), 1);
  EXPECT_NEAR(screenshot.height(), expected_size.height(), 1);

  // The page content has geometry, and its text can be hit-tested. The size
  // of the root's box is not checked: it is smaller than the viewport when
  // the page is zoomed in, as it is by default on Android desktop.
  const ContentNode& root = result->page_content.root_node();
  EXPECT_TRUE(root.content_attributes().geometry().has_visible_bounding_box());
  EXPECT_TRUE(HasHitTestableDescendant(root));
}

// A page with fields whose values must not leave the device: the fields, and
// the features, off by default, that painting over them needs.
struct SensitiveFields {
  const char* name;
  const char* path;
  std::vector<const char*> selectors;
  std::vector<base::test::FeatureRef> features;
};

// The screenshot is the page context fetcher's, which paints over fields
// with sensitive values. get_pointer sends the model a part of it, so check
// that it does so for a capture made the way the capturer makes it.
class PointerContextCapturerRedactionBrowserTest
    : public PointerContextCapturerBrowserTest,
      public testing::WithParamInterface<SensitiveFields> {
 public:
  PointerContextCapturerRedactionBrowserTest() {
    // Passwords are painted over by default; payment fields and one-time
    // codes only with features. Each page gets only the features for its own
    // kind of field, so that it is not painted over thanks to another's.
    if (!GetParam().features.empty()) {
      scoped_feature_list_.InitWithFeatures(GetParam().features, {});
    }
  }

  void SetUp() override {
    // Without this the screenshot is black all over, and so would be where
    // a field should have been painted over but was not.
    EnablePixelOutput();
    PointerContextCapturerBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PointerContextCapturerRedactionBrowserTest,
    testing::Values(
        SensitiveFields{"Password", "/password.html", {"#filled-password"}, {}},
        SensitiveFields{
            "CreditCard",
            "/optimization_guide/credit_card.html",
            {"#filled-cc-number", "#filled-cc-exp"},
            {page_content_annotations::kGlicScreenshotSensitivePaymentRedaction,
             optimization_guide::features::
                 kAnnotatedPageContentAutofillCreditCardRedactions}},
        SensitiveFields{"OneTimeCode",
                        "/optimization_guide/otp_redaction.html",
                        {"#otp"},
                        {optimization_guide::features::
                             kAnnotatedPageContentAutofillOtpRedactions}}),
    [](const testing::TestParamInfo<SensitiveFields>& info) {
      return std::string(info.param.name);
    });

IN_PROC_BROWSER_TEST_P(PointerContextCapturerRedactionBrowserTest,
                       SensitiveFieldsArePaintedOver) {
  base::HistogramTester histograms;
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(GetParam().path)));
  content::WaitForCopyableViewInWebContents(web_contents());
  if (!GetParam().features.empty()) {
    // Payment fields and one-time codes are known as such from Autofill,
    // which parses a page's forms asynchronously. A capture made before it
    // has would leave the fields as they are.
    autofill::ContentAutofillDriver* driver =
        autofill::ContentAutofillDriver::GetForRenderFrameHost(
            web_contents()->GetPrimaryMainFrame());
    ASSERT_TRUE(driver);
    ASSERT_TRUE(autofill::WaitForMatchingForm(
        &driver->GetAutofillManager(),
        base::BindRepeating(
            [](const autofill::FormStructure&) { return true; })));
  }

  PointerContextCapturer capturer(TestTimeouts::action_timeout());
  base::test::TestFuture<PointerContextCapturer::Result> future;
  capturer.Capture(web_contents(), future.GetCallback());
  PointerContextCapturer::Result result = future.Take();
  ASSERT_TRUE(result.has_value());
  const SkBitmap screenshot = gfx::JPEGCodec::Decode(result->screenshot);
  ASSERT_FALSE(screenshot.drawsNothing());

  for (const char* selector : GetParam().selectors) {
    SCOPED_TRACE(selector);
    // The middle of the field, in the screenshot's pixels however the page is
    // zoomed and scaled, is the color that sensitive fields are painted over
    // with: black.
    const content::EvalJsResult center_result = content::EvalJs(
        web_contents(),
        content::JsReplace(
            "(() => {"
            "  const r = document.querySelector($1).getBoundingClientRect();"
            "  const s = window.devicePixelRatio * visualViewport.scale;"
            "  return [Math.floor((r.left + r.width / 2) * s),"
            "          Math.floor((r.top + r.height / 2) * s)];"
            "})()",
            selector));
    ASSERT_TRUE(center_result.is_list()) << center_result;
    const base::ListValue& center = center_result.ExtractList();
    ASSERT_EQ(center.size(), 2u);
    ASSERT_TRUE(center[0].is_int());
    ASSERT_TRUE(center[1].is_int());
    const int x = center[0].GetInt();
    const int y = center[1].GetInt();
    ASSERT_GE(x, 0);
    ASSERT_LT(x, screenshot.width());
    ASSERT_GE(y, 0);
    ASSERT_LT(y, screenshot.height());
    const SkColor field_color = screenshot.getColor(x, y);
    EXPECT_LT(SkColorGetR(field_color) + SkColorGetG(field_color) +
                  SkColorGetB(field_color),
              0x30u);
  }
  // The page, whose fields are all near its top left, is white elsewhere.
  const SkColor page_color =
      screenshot.getColor(screenshot.width() / 2, screenshot.height() / 2);
  EXPECT_GT(SkColorGetR(page_color) + SkColorGetG(page_color) +
                SkColorGetB(page_color),
            3 * 0xE0u);
  // The fetcher records whether it painted over anything.
  histograms.ExpectUniqueSample("Glic.PageContextFetcher.ScreenshotRedacted",
                                true, 1);
}

IN_PROC_BROWSER_TEST_F(PointerContextCapturerBrowserTest, TimesOut) {
  NavigateToPageWithText();
  // No time at all: the capture cannot finish before the timer fires.
  PointerContextCapturer capturer(base::TimeDelta{});
  base::test::TestFuture<PointerContextCapturer::Result> future;
  capturer.Capture(web_contents(), future.GetCallback());
  PointerContextCapturer::Result result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mojom::ActionResultCode::kToolTimeout);

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
  EXPECT_EQ(result.error().code, mojom::ActionResultCode::kRendererCrashed);
}

}  // namespace
}  // namespace actor
