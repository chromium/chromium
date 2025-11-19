// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <tuple>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_conversions.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

using base::test::TestFuture;
using content::ChildFrameAt;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;
using content::NavigateIframeToURL;
using content::RenderFrameHost;

namespace actor {

namespace {

#if BUILDFLAG(ENABLE_PDF) && !BUILDFLAG(IS_CHROMEOS)
void CheckForConditionAndWaitMoreIfNeeded(
    base::RepeatingCallback<bool()> condition,
    base::OnceClosure quit_closure) {
  if (condition.Run()) {
    std::move(quit_closure).Run();
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CheckForConditionAndWaitMoreIfNeeded,
                     std::move(condition), std::move(quit_closure)),
      TestTimeouts::tiny_timeout());
}

// Wait until |condition| returns true.
void WaitForCondition(base::RepeatingCallback<bool()> condition,
                      const std::string& description) {
  base::RunLoop run_loop;
  CheckForConditionAndWaitMoreIfNeeded(condition, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(condition.Run())
      << "Timeout waiting for condition: " << description;
}
#endif

class ActorClickToolBrowserTest : public ActorToolsTest,
                                  public ::testing::WithParamInterface<
                                      ::features::ActorPaintStabilityMode> {
 public:
  ActorClickToolBrowserTest() {
    auto paint_stability_mode = GetParam();
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kActorPaintStabilityMode.name,
          ::features::kActorPaintStabilityMode.GetName(paint_stability_mode)},
         {features::kGlicActorClickDelay.name, "200ms"}});
  }

  ~ActorClickToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Basic test to ensure sending a click to an element works.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_SentToElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to the document body.
  {
    std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
    ASSERT_TRUE(body_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), body_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[BODY#],mousedown[BODY#],mouseup[BODY#],click[BODY#]"));
  }

  ASSERT_TRUE(ExecJs(web_contents(), "mouse_event_log = []"));

  // Send a second click to the button.
  {
    std::optional<int> button_id =
        GetDOMNodeId(*main_frame(), "button#clickable");
    ASSERT_TRUE(button_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), button_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[BUTTON#clickable],mousedown[BUTTON#clickable],"
            "mouseup[BUTTON#clickable],click[BUTTON#clickable]"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "button_clicked"));
  }
}

// Sending a click to an element that doesn't exist fails.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_NonExistentElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), kNonExistentContentNodeId);
  ActResultFuture result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  // The node id doesn't exist so the tool will return false.
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kInvalidDomNodeId);

  // The page should not have received any click events.
  EXPECT_THAT(
      EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
      testing::Not(testing::HasSubstr("mousedown")));
}

// Sending a click to a disabled element should fail without dispatching events.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_DisabledElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "button#disabled");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result_fail;
  actor_task().Act(ToRequestList(action), result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kElementDisabled);

  // The page should not have received any click events.
  EXPECT_THAT(
      EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
      testing::Not(testing::HasSubstr("mousedown")));
}

// Sending a click to an element that's not in the viewport should cause it to
// first be scrolled into view then clicked.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_OffscreenElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Page starts unscrolled
  ASSERT_EQ(0, EvalJs(web_contents(), "window.scrollY"));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#offscreen");
  ASSERT_TRUE(button_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Page is now scrolled.
  ASSERT_GT(EvalJs(web_contents(), "window.scrollY"), 0);
  // The page should not have received any events.
  EXPECT_THAT(
      EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
      testing::EndsWith(
          "mousemove[BUTTON#offscreen],mousedown[BUTTON#offscreen],"
          "mouseup[BUTTON#offscreen],click[BUTTON#offscreen]"));
}

// Ensure clicks can be sent to elements that are only partially onscreen.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_ClippedElements) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_with_overflow_clip.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::vector<std::string> test_cases = {
      "offscreenButton", "overflowHiddenButton", "overflowScrollButton"};

  for (auto button : test_cases) {
    SCOPED_TRACE(testing::Message() << "WHILE TESTING: " << button);
    std::optional<int> button_id =
        GetDOMNodeId(*main_frame(), base::StrCat({"#", button}));
    ASSERT_TRUE(button_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), button_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(button, EvalJs(web_contents(), "clicked_button"));

    ASSERT_TRUE(ExecJs(web_contents(), "clicked_button = ''"));
  }
}

// Ensure clicks can be sent to a coordinate onscreen.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_SentToCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to a (0,0) coordinate inside the document.
  {
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), gfx::Point(0, 0));
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[HTML#],mousedown[HTML#],mouseup[HTML#],click[HTML#]"));
  }

  ASSERT_TRUE(ExecJs(web_contents(), "mouse_event_log = []"));

  // Send a second click to a coordinate on the button.
  {
    gfx::Point click_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "clickable"));

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), click_point);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[BUTTON#clickable],mousedown[BUTTON#clickable],"
            "mouseup[BUTTON#clickable],click[BUTTON#clickable]"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "button_clicked"));
  }
}

// Sending a click to a coordinate not in the viewport should fail without
// dispatching events.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_SentToCoordinateOffScreen) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to a negative coordinate offscreen.
  {
    gfx::Point negative_offscreen = {-1, 0};
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), negative_offscreen);
    ActResultFuture result_fail;
    actor_task().Act(ToRequestList(action), result_fail.GetCallback());
    ExpectErrorResult(result_fail,
                      mojom::ActionResultCode::kCoordinatesOutOfBounds);

    // The page should not have received any click events.
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::Not(testing::HasSubstr("mousedown")));
  }

  // Send a click to a positive coordinate offscreen.
  {
    gfx::Point positive_offscreen = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), positive_offscreen);
    ActResultFuture result_fail;
    actor_task().Act(ToRequestList(action), result_fail.GetCallback());
    ExpectErrorResult(result_fail,
                      mojom::ActionResultCode::kCoordinatesOutOfBounds);
    // The page should not have received any click events.
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::Not(testing::HasSubstr("mousedown")));
  }
}

// Ensure click is using viewport coordinate.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_ViewportCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Scroll the window by 100vh so #offscreen button is in viewport.
  ASSERT_TRUE(ExecJs(web_contents(), "window.scrollBy(0, window.innerHeight)"));

  // Send a click to button's viewport coordinate.
  {
    gfx::Point click_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), click_point);
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_THAT(
        EvalJs(web_contents(), "mouse_event_log.join(',')").ExtractString(),
        testing::EndsWith(
            "mousemove[BUTTON#offscreen],mousedown[BUTTON#offscreen],"
            "mouseup[BUTTON#offscreen],click[BUTTON#offscreen]"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "offscreen_button_clicked"));
  }
}

// Ensure click works correctly when clicking on a cross process iframe using a
// DomNodeId
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_Subframe_DomNodeId) {
  // This test only applies if cross-origin frames are put into separate
  // processes.
  if (!content::AreAllSitesIsolatedForTesting()) {
    GTEST_SKIP();
  }

  const GURL url = embedded_https_test_server().GetURL(
      "foo.com", "/actor/positioned_iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL subframe_url = embedded_https_test_server().GetURL(
      "bar.com", "/actor/page_with_clickable_element.html");
  ASSERT_TRUE(NavigateIframeToURL(web_contents(), "iframe", subframe_url));

  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ASSERT_TRUE(subframe->IsCrossProcessSubframe());

  // Send a click to the button in the subframe.
  std::optional<int> button_id = GetDOMNodeId(*subframe, "button#clickable");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*subframe, button_id.value());

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Ensure the button's event handler was invoked.
  EXPECT_EQ(true, EvalJs(subframe, "button_clicked"));
}

// Ensure that page tools (click is arbitrary here) correctly add the acted on
// tab to the task's tab set.
IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest,
                       ClickTool_RecordActingOnTask) {
  ASSERT_TRUE(actor_task().GetTabs().empty());

  // Send a click to the document body.
  std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
  ASSERT_TRUE(body_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), body_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(actor_task().GetTabs().contains(active_tab()->GetHandle()));
}

IN_PROC_BROWSER_TEST_P(ActorClickToolBrowserTest, ClickTool_Delay) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
  ASSERT_TRUE(body_id);

  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), body_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const double mousedown_timestamp = EvalJs(main_frame(), R"(
        let index = mouse_event_log.findIndex(
          (entry) => entry.startsWith('mousedown'));
        mouse_event_timestamps[index]
      )")
                                         .ExtractDouble();
  const double mouseup_timestamp = EvalJs(main_frame(), R"(
        let index = mouse_event_log.findIndex(
          (entry) => entry.startsWith('mouseup'));
        mouse_event_timestamps[index]
      )")
                                       .ExtractDouble();
  const base::TimeDelta delta =
      base::Milliseconds(mouseup_timestamp - mousedown_timestamp);

  EXPECT_GE(delta, features::kGlicActorClickDelay.Get());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ActorClickToolBrowserTest,
    testing::Values(::features::ActorPaintStabilityMode::kDisabled,
                    ::features::ActorPaintStabilityMode::kLogOnly,
                    ::features::ActorPaintStabilityMode::kEnabled),
    [](const testing::TestParamInfo<::features::ActorPaintStabilityMode>&
           info) { return DescribePaintStabilityMode(info.param); });

class ActorClickToolScaledBrowserTest : public ActorToolsTest {
 public:
  ActorClickToolScaledBrowserTest() = default;
  ~ActorClickToolScaledBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ActorToolsTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForceDeviceScaleFactor);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

// Ensure clicks can be sent to elements that are only partially onscreen with
// scaling.
IN_PROC_BROWSER_TEST_F(ActorClickToolScaledBrowserTest,
                       ClickTool_ScaledClippedElements) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/click_with_overflow_clip.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::vector<std::string> test_cases = {
      "offscreenButton", "overflowHiddenButton", "overflowScrollButton"};

  for (auto button : test_cases) {
    SCOPED_TRACE(testing::Message() << "WHILE TESTING: " << button);
    std::optional<int> button_id =
        GetDOMNodeId(*main_frame(), base::StrCat({"#", button}));
    ASSERT_TRUE(button_id);

    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*main_frame(), button_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(button, EvalJs(web_contents(), "clicked_button"));

    ASSERT_TRUE(ExecJs(web_contents(), "clicked_button = ''"));
  }
}

#if BUILDFLAG(ENABLE_PDF) && !BUILDFLAG(IS_CHROMEOS)

class ActorClickToolPDFBrowserTest
    : public ActorToolsTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ActorClickToolPDFBrowserTest() {
    if (BypaassTOUValidationForGuestView()) {
      feature_list_.InitWithFeatures({kActorBypassTOUValidationForGuestView},
                                     {chrome_pdf::features::kPdfOopif});
    } else {
      feature_list_.InitWithFeatures({},
                                     {chrome_pdf::features::kPdfOopif,
                                      kActorBypassTOUValidationForGuestView});
    }
  }

  ~ActorClickToolPDFBrowserTest() override = default;

  bool BypaassTOUValidationForGuestView() { return GetParam(); }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "BypassGuestViewTOU" : "CheckGuestViewTOU";
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Ensure clicks can rotate on a PDF.
IN_PROC_BROWSER_TEST_P(ActorClickToolPDFBrowserTest, Click) {
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  WaitForCondition(
      base::BindLambdaForTesting([this]() {
        auto* pdf_helper =
            pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents());
        if (!pdf_helper) {
          return false;
        }
        return pdf_helper->IsDocumentLoadComplete();
      }),
      "PDF Loaded");

  while (true) {
    GetPageApc();
    std::unique_ptr<ToolRequest> action =
        MakeClickRequest(*active_tab(), gfx::Point(650, 25));
    ActResultFuture future;
    actor_task().Act(ToRequestList(action), future.GetCallback());
    if (BypaassTOUValidationForGuestView()) {
      // This should always pass the first time.
      ExpectOkResult(future);
      break;
    } else {
      // Sometimes it might be allowed, but it will fail eventually. Keep
      // looping until we fail.
      const auto& result = *(future.Get<0>());
      if (result.code ==
          mojom::ActionResultCode::kFrameLocationChangedSinceObservation) {
        break;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ActorClickToolPDFBrowserTest,
                         ::testing::Bool(),
                         &ActorClickToolPDFBrowserTest::DescribeParams);

#endif  // BUILDFLAG(ENABLE_PDF) && !BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace actor
