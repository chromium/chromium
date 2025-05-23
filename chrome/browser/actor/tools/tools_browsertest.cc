// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/actor.mojom-data-view.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d.h"

using base::test::ScopedFeatureList;
using base::test::TestFuture;
using content::ChildFrameAt;
using content::EvalJs;
using content::ExecJs;
using content::GetDOMNodeId;
using content::JsReplace;
using content::RenderFrameHost;
using content::TestNavigationManager;
using content::TestNavigationObserver;
using content::ToRenderFrameHost;
using content::WebContents;
using optimization_guide::proto::BrowserAction;
using optimization_guide::proto::ClickAction;
using optimization_guide::proto::NavigateAction;
using tabs::TabInterface;

namespace actor {

namespace {

gfx::RectF GetBoundingClientRect(RenderFrameHost& rfh, std::string_view query) {
  double width =
      content::EvalJs(
          &rfh,
          JsReplace("document.querySelector($1).getBoundingClientRect().width",
                    query))
          .ExtractDouble();
  double height =
      content::EvalJs(
          &rfh,
          JsReplace("document.querySelector($1).getBoundingClientRect().height",
                    query))
          .ExtractDouble();
  double x =
      content::EvalJs(
          &rfh,
          JsReplace("document.querySelector($1).getBoundingClientRect().x",
                    query))
          .ExtractDouble();
  double y =
      content::EvalJs(
          &rfh,
          JsReplace("document.querySelector($1).getBoundingClientRect().y",
                    query))
          .ExtractDouble();

  return gfx::RectF(x, y, width, height);
}

int GetRangeValue(RenderFrameHost& rfh, std::string_view query) {
  return content::EvalJs(
             &rfh,
             JsReplace("parseInt(document.querySelector($1).value)", query))
      .ExtractInt();
}

constexpr int32_t kNonExistentContentNodeId = 12345;

class ActorToolsTest : public InProcessBrowserTest {
 public:
  ActorToolsTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorToolsTest(const ActorToolsTest&) = delete;
  ActorToolsTest& operator=(const ActorToolsTest&) = delete;

  ~ActorToolsTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // TODO(crbug.com/409564704): Mock the delay so that tests can run at
    // reasonable speed. Remove once there is a more permanent approach.
    OverrideActionObservationDelay(base::Milliseconds(10));

    actor_coordinator_ =
        std::make_unique<ActorCoordinator>(browser()->profile());
    actor_coordinator().StartTaskForTesting(browser()->GetActiveTabInterface());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
  }

  void TearDownOnMainThread() override {
    // The coordinator has a pointer to the profile, which must be released
    // before the browser is torn down to avoid a dangling pointer.
    actor_coordinator_.reset();
  }

  void GoBack() {
    TestNavigationObserver observer(web_contents());
    web_contents()->GetController().GoBack();
    observer.Wait();
  }

  void TinyWait() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ActorCoordinator& actor_coordinator() { return *actor_coordinator_; }

  std::string GetSelectElementCurrentValue(std::string_view query_selector) {
    return EvalJs(web_contents(),
                  JsReplace("document.querySelector($1).value", query_selector))
        .ExtractString();
  }

 private:
  ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ActorCoordinator> actor_coordinator_;
};

// ===============================================
// Please keep the tests in this file grouped by tool.
// ===============================================

// ===============================================
// Click Tool
// ===============================================

// Basic test to ensure sending a click to an element works.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_SentToElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to the document body.
  {
    std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
    ASSERT_TRUE(body_id);

    BrowserAction action = MakeClick(*main_frame(), body_id.value());
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ("mousedown[BODY#],mouseup[BODY#],click[BODY#]",
              EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }

  ASSERT_TRUE(ExecJs(web_contents(), "mouse_event_log = []"));

  // Send a second click to the button.
  {
    std::optional<int> button_id =
        GetDOMNodeId(*main_frame(), "button#clickable");
    ASSERT_TRUE(button_id);

    BrowserAction action = MakeClick(*main_frame(), button_id.value());
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(
        "mousedown[BUTTON#clickable],mouseup[BUTTON#clickable],click[BUTTON#"
        "clickable]",
        EvalJs(web_contents(), "mouse_event_log.join(',')"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "button_clicked"));
  }
}

// Sending a click to an element that doesn't exist fails.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_NonExistentElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  BrowserAction action = MakeClick(*main_frame(), kNonExistentContentNodeId);
  TestFuture<mojom::ActionResultPtr> result_fail;
  actor_coordinator().Act(action, result_fail.GetCallback());
  // The node id doesn't exist so the tool will return false.
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kInvalidDomNodeId);

  // The page should not have received any events.
  EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Sending a click to a disabled element should fail without dispatching events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_DisabledElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id = GetDOMNodeId(*main_frame(), "button#disabled");
  ASSERT_TRUE(button_id);

  BrowserAction action = MakeClick(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr> result_fail;
  actor_coordinator().Act(action, result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kElementDisabled);

  // The page should not have received any events.
  EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Sending a click to an element that's not in the viewport should fail without
// dispatching events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_OffscreenElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#offscreen");
  ASSERT_TRUE(button_id);

  BrowserAction action = MakeClick(*main_frame(), button_id.value());
  TestFuture<mojom::ActionResultPtr> result_fail;
  actor_coordinator().Act(action, result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kElementOffscreen);

  // The page should not have received any events.
  EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
}

// Ensure clicks can be sent to elements that are only partially onscreen.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_ClippedElements) {
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

    BrowserAction action = MakeClick(*main_frame(), button_id.value());
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(button, EvalJs(web_contents(), "clicked_button"));

    ASSERT_TRUE(ExecJs(web_contents(), "clicked_button = ''"));
  }
}

// Ensure clicks can be sent to a coordinate onscreen.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_SentToCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to a (0,0) coordinate inside the document.
  {
    BrowserAction action = MakeClick(*main_frame(), gfx::Point(0, 0));
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ("mousedown[HTML#],mouseup[HTML#],click[HTML#]",
              EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }

  ASSERT_TRUE(ExecJs(web_contents(), "mouse_event_log = []"));

  // Send a second click to a coordinate on the button.
  {
    gfx::Point click_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "clickable"));

    BrowserAction action = MakeClick(*main_frame(), click_point);
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(
        "mousedown[BUTTON#clickable],mouseup[BUTTON#clickable],click[BUTTON#"
        "clickable]",
        EvalJs(web_contents(), "mouse_event_log.join(',')"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "button_clicked"));
  }
}

// Sending a click to a coordinate not in the viewport should fail without
// dispatching events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_SentToCoordinateOffScreen) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Send a click to a negative coordinate offscreen.
  {
    gfx::Point negative_offscreen = {-1, 0};
    BrowserAction action = MakeClick(*main_frame(), negative_offscreen);
    TestFuture<mojom::ActionResultPtr> result_fail;
    actor_coordinator().Act(action, result_fail.GetCallback());
    ExpectErrorResult(result_fail,
                      mojom::ActionResultCode::kCoordinatesOutOfBounds);

    // The page should not have received any events.
    EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }

  // Send a click to a positive coordinate offscreen.
  {
    gfx::Point positive_offscreen = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));
    BrowserAction action = MakeClick(*main_frame(), positive_offscreen);
    TestFuture<mojom::ActionResultPtr> result_fail;
    actor_coordinator().Act(action, result_fail.GetCallback());
    ExpectErrorResult(result_fail,
                      mojom::ActionResultCode::kCoordinatesOutOfBounds);
    // The page should not have received any events.
    EXPECT_EQ("", EvalJs(web_contents(), "mouse_event_log.join(',')"));
  }
}

// Ensure click is using viewport coordinate.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ClickTool_ViewportCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Scroll the window by 100vh so #offscreen button is in viewport.
  ASSERT_TRUE(ExecJs(web_contents(), "window.scrollBy(0, window.innerHeight)"));

  // Send a click to button's viewport coordinate.
  {
    gfx::Point click_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));

    BrowserAction action = MakeClick(*main_frame(), click_point);
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(
        "mousedown[BUTTON#offscreen],mouseup[BUTTON#offscreen],click[BUTTON#"
        "offscreen]",
        EvalJs(web_contents(), "mouse_event_log.join(',')"));

    // Ensure the button's event handler was invoked.
    EXPECT_EQ(true, EvalJs(web_contents(), "offscreen_button_clicked"));
  }
}

// ===============================================
// Type Tool
// ===============================================

// Basic test of the TypeTool - ensure typed string is entered into an input
// box.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_TextInput) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "test";
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  BrowserAction action = MakeType(*main_frame(), input_id.value(), typed_string,
                                  /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// TypeTool fails when target is non-existent.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_NonExistentNode) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "test";
  BrowserAction action =
      MakeType(*main_frame(), kNonExistentContentNodeId, typed_string,
               /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
  EXPECT_EQ("",
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Ensure type tool sends the expected events to an input box.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_Events) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  std::string typed_string = "ab";

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  BrowserAction action = MakeType(*main_frame(), input_id.value(), typed_string,
                                  /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(
      // a
      "keydown,input,keyup,"
      // b
      "keydown,input,keyup,"
      // enter (causes submit to "click")
      "keydown,change,click,keyup",
      EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool can be used without text to send an enter key in an
// input.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_EmptyText) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  std::string typed_string = "";

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);
  BrowserAction action = MakeType(*main_frame(), input_id.value(), typed_string,
                                  /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(
      // enter (causes submit to "click")
      "keydown,click,keyup",
      EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool correctly sends the enter key after input if specified.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_FollowByEnter) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  // Send 'a' followed by enter. Ensure the click event is seen.
  {
    std::string typed_string = "a";
    BrowserAction action = MakeType(*main_frame(), input_id.value(),
                                    typed_string, /*follow_by_enter=*/true);

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(
      // a
      "keydown,input,keyup,"
      // enter (causes submit to "click")
      "keydown,change,click,keyup",
      EvalJs(web_contents(), "input_event_log.join(',')"));

  ASSERT_TRUE(ExecJs(web_contents(), "input_event_log = []"));

  // Send 'b' without an enter. Ensure the click event is _not_ seen.
  {
    std::string typed_string = "b";
    BrowserAction action = MakeType(*main_frame(), input_id.value(),
                                    typed_string, /*follow_by_enter=*/false);

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(
      // b
      "keydown,input,keyup",
      EvalJs(web_contents(), "input_event_log.join(',')"));
}

// Ensure the type tool doesn't fail if the keydown event is handled (page
// called preventDefault).
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_PageHandlesKeyEvents) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::optional<int> input_id =
      GetDOMNodeId(*main_frame(), "#keyHandlingInput");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  BrowserAction action = MakeType(*main_frame(), input_id.value(), typed_string,
                                  /*follow_by_enter=*/true);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);
}

// Ensure that the default mode is for the type tool to replace any existing
// text in the targeted element.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_ReplacesText) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.getElementById('input').value = 'foo bar'"));
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  BrowserAction action = MakeType(*main_frame(), input_id.value(), typed_string,
                                  /*follow_by_enter=*/false);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input').value"));
}

// Ensure that if the page moves focus immediately to a different input box, the
// type tool correctly operates on the new input box.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_FocusMovesFocus) {
  const GURL url = embedded_test_server()->GetURL("/actor/input.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Setup the first input box to immediately move focus to the second input
  // box. Ensure the existing text in the second box is replaced.
  ASSERT_TRUE(ExecJs(web_contents(),
                     R"JS(
                        let input = document.getElementById('input');
                        let input2 = document.getElementById('input2');
                        input2.value = 'foo bar';
                        input.addEventListener('focus', () => {
                          input2.focus();
                        });
                      )JS"));
  std::optional<int> input_id = GetDOMNodeId(*main_frame(), "#input");
  ASSERT_TRUE(input_id);

  std::string typed_string = "abc";
  BrowserAction action = MakeType(*main_frame(), input_id.value(), typed_string,
                                  /*follow_by_enter=*/false);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);

  // Since focusing the first input causes the second input to become focused,
  // the tool should operate on the second input.
  EXPECT_EQ("",
            EvalJs(web_contents(), "document.getElementById('input').value"));
  EXPECT_EQ(typed_string,
            EvalJs(web_contents(), "document.getElementById('input2').value"));
}

// Basic test of the TypeTool coordinate target - ensure typed string is entered
// into a node at the coordinate.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_TextInputAtCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::string typed_string = "test";
  // Type into coordinate of input box.
  {
    gfx::Point type_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "input"));
    BrowserAction action = MakeType(*main_frame(), type_point, typed_string,
                                    /*follow_by_enter=*/true);

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(typed_string,
              EvalJs(web_contents(), "document.getElementById('input').value"));
  }
  // Type into coordinate of editable div.
  {
    gfx::Point type_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "editableDiv"));
    BrowserAction action = MakeType(*main_frame(), type_point, typed_string,
                                    /*follow_by_enter=*/true);

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(typed_string,
              EvalJs(web_contents(),
                     "document.getElementById('editableDiv').textContent"));
  }
}

// Ensure the type tool correctly sends the events to element at the
// coordinates.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_EventsSentToCoordinates) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  // Send event to an editable div.
  {
    gfx::Point type_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "editableDiv"));

    // Send 'a'. Ensure a click event is observed first on element at the
    // coordinate.
    std::string typed_string = "a";
    BrowserAction action = MakeType(*main_frame(), type_point, typed_string,
                                    /*follow_by_enter=*/false);

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(
        // click
        "mousedown(" + type_point.ToString() + "),mouseup(" +
            type_point.ToString() + "),click(" + type_point.ToString() +
            "),"
            // a
            "keydown,input,keyup",
        EvalJs(web_contents(), "input_event_log.join(',')"));
  }

  ASSERT_TRUE(ExecJs(web_contents(), "input_event_log = []"));

  // Send event to a focusable but not editable div.
  {
    gfx::Point type_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "focusableDiv"));

    // Send 'a'. Ensure a click event is observed first on element at the
    // coordinate.
    std::string typed_string = "a";
    BrowserAction action = MakeType(*main_frame(), type_point, typed_string,
                                    /*follow_by_enter=*/false);

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);

    EXPECT_EQ(
        // click
        "mousedown(" + type_point.ToString() + "),mouseup(" +
            type_point.ToString() + "),click(" + type_point.ToString() +
            "),"
            // a
            "keydown,keyup",
        EvalJs(web_contents(), "input_event_log.join(',')"));
  }
}

// Ensure the type tool correctly sends the events to an unfocusable element at
// the coordinates.
IN_PROC_BROWSER_TEST_F(ActorToolsTest,
                       TypeTool_EventsSentToUnfocusableCoordinate) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  // Set coordinate to an unfocusable div.

  gfx::Point type_point = gfx::ToFlooredPoint(
      GetCenterCoordinatesOfElementWithId(web_contents(), "unfocusableDiv"));

  // Send 'a'. Ensure a click event is observed first on element at the
  // coordinate.
  std::string typed_string = "a";
  BrowserAction action = MakeType(*main_frame(), type_point, typed_string,
                                  /*follow_by_enter=*/false);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);

  // Only the click is handled by the node at coordinate.
  EXPECT_EQ(
      // click
      "mousedown(" + type_point.ToString() + "),mouseup(" +
          type_point.ToString() + "),click(" + type_point.ToString() + ")",
      EvalJs(web_contents(), "input_event_log.join(',')"));
  // The keydown and keyup event will go to the body now that div is
  // unfocusable.
  EXPECT_EQ(
      // a
      "keydown,keyup",
      EvalJs(web_contents(), "body_input_event_log.join(',')"));
}

// Ensure the type tool will fail if target coordinate is offscreen.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, TypeTool_SentToOffScreenCoordinates) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/type_input_coordinate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));

  // Send 'a' to an offscreen coordinate and observe failure.
  std::string typed_string = "a";
  BrowserAction action = MakeType(*main_frame(), gfx::Point(-1, 0),
                                  typed_string, /*follow_by_enter=*/false);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);

  EXPECT_EQ("", EvalJs(web_contents(), "input_event_log.join(',')"));
}

// ===============================================
// Mouse Move Tool
// ===============================================

// Test the MouseMove tool fails on a non-existent content node.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, MouseMoveTool_NonExistentNode) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Use a random node id that doesn't exist.
  BrowserAction action =
      MakeMouseMove(*main_frame(), kNonExistentContentNodeId);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kInvalidDomNodeId);
}

// Test basic movements using MouseMove tool generates the expected events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, MouseMoveTool_Events) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Move mouse over #first DIV
  {
    std::optional<int> first_id = GetDOMNodeId(*main_frame(), "#first");
    BrowserAction action = MakeMouseMove(*main_frame(), first_id.value());

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ("mouseenter[DIV#first],mousemove[DIV#first]",
            EvalJs(web_contents(), "event_log.join(',')"));
  ASSERT_TRUE(ExecJs(web_contents(), "event_log = []"));

  // Move mouse over #second DIV
  {
    std::optional<int> second_id = GetDOMNodeId(*main_frame(), "#second");
    BrowserAction action = MakeMouseMove(*main_frame(), second_id.value());

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(
      "mouseleave[DIV#first],mouseenter[DIV#second],mousemove[DIV#second]",
      EvalJs(web_contents(), "event_log.join(',')"));
}

// Test mouse move returns failure if a target is offscreen.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, MouseMoveTool_TargetOutsideViewport) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Move mouse over #offscreen DIV. This should fail since #offscreen is
  // outside the viewport.
  {
    std::optional<int> offscreen_id = GetDOMNodeId(*main_frame(), "#offscreen");
    BrowserAction action = MakeMouseMove(*main_frame(), offscreen_id.value());

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kElementOffscreen);
  }

  // The action should fail without generating any events.
  EXPECT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Scroll the element into the viewport.
  ASSERT_TRUE(ExecJs(web_contents(),
                     "document.getElementById('offscreen').scrollIntoView()"));

  // Try moving the mouse over #offscreen again. This time it should succeed
  // since it was scrolled into the viewport.
  {
    std::optional<int> offscreen_id = GetDOMNodeId(*main_frame(), "#offscreen");
    BrowserAction action = MakeMouseMove(*main_frame(), offscreen_id.value());

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ("mouseenter[DIV#offscreen],mousemove[DIV#offscreen]",
            EvalJs(web_contents(), "event_log.join(',')"));
}

// Ensure mouse can be moved to a coordinate onscreen.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, MouseMoveTool_MoveToCoordinate) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Move mouse over #first DIV
  gfx::Point move_point = gfx::ToFlooredPoint(
      GetCenterCoordinatesOfElementWithId(web_contents(), "first"));
  BrowserAction action = MakeMouseMove(*main_frame(), move_point);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ("mouseenter[DIV#first],mousemove[DIV#first]",
            EvalJs(web_contents(), "event_log.join(',')"));
}

// Moving mouse to a coordinate not in the viewport should fail without
// dispatching events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest,
                       MouseMoveTool_MoveToCoordinateOffScreen) {
  const GURL url = embedded_test_server()->GetURL("/actor/mouse_log.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Move mouse over #offscreen DIV. This should fail since #offscreen is
  // outside the viewport.
  {
    gfx::Point move_point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), "offscreen"));
    BrowserAction action = MakeMouseMove(*main_frame(), move_point);

    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kCoordinatesOutOfBounds);
  }

  // The action should fail without generating any events.
  EXPECT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));
}

// ===============================================
// Scroll Tool
// ===============================================

IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_FailOnInvalidNodeID) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Use a random node id that doesn't exist.
  float scroll_offset_y = 50;
  BrowserAction action = MakeScroll(*main_frame(), kNonExistentContentNodeId,
                                    /*scroll_offset_x=*/0, scroll_offset_y);

  TestFuture<mojom::ActionResultPtr> result_fail;
  actor_coordinator().Act(action, result_fail.GetCallback());
  ExpectErrorResult(result_fail, mojom::ActionResultCode::kInvalidDomNodeId);

  EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
}

// Test scrolling the viewport vertically.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_ScrollPageVertical) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_y = 50;

  {
    // If no node id is passed, it will scroll the page's viewport.
    BrowserAction action =
        MakeScroll(*main_frame(), /*content_node_id=*/std::nullopt,
                   /*scroll_offset_x=*/0, scroll_offset_y);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(scroll_offset_y, EvalJs(web_contents(), "window.scrollY"));
  }

  {
    BrowserAction action =
        MakeScroll(*main_frame(), /*content_node_id=*/std::nullopt,
                   /*scroll_offset_x=*/0, scroll_offset_y);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(2 * scroll_offset_y, EvalJs(web_contents(), "window.scrollY"));
  }
}

// Test scrolling the viewport horizontally.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_ScrollPageHorizontal) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_x = 50;

  {
    // If no node id is passed, it will scroll the page's viewport.
    BrowserAction action = MakeScroll(
        *main_frame(), /*content_node_id=*/std::nullopt, scroll_offset_x,
        /*scroll_offset_y=*/0);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(scroll_offset_x, EvalJs(web_contents(), "window.scrollX"));
  }

  {
    BrowserAction action = MakeScroll(
        *main_frame(), /*content_node_id=*/std::nullopt, scroll_offset_x,
        /*scroll_offset_y=*/0);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(2 * scroll_offset_x, EvalJs(web_contents(), "window.scrollX"));
  }
}

// Test scrolling in a sub-scroller on the page.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_ScrollElement) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_x = 50;
  int scroll_offset_y = 80;

  int scroller = GetDOMNodeId(*main_frame(), "#scroller").value();

  {
    BrowserAction action = MakeScroll(*main_frame(), scroller, scroll_offset_x,
                                      /*scroll_offset_y=*/0);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(scroll_offset_x,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollLeft"));
  }

  {
    BrowserAction action = MakeScroll(*main_frame(), scroller,
                                      /*scroll_offset_x=*/0, scroll_offset_y);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(scroll_offset_y,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollTop"));
  }
}

// Test scrolling over a non-scrollable element returns failure.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_NonScrollable) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset_y = 80;

  int scroller = GetDOMNodeId(*main_frame(), "#nonscroll").value();

  {
    BrowserAction action = MakeScroll(*main_frame(), scroller,
                                      /*scroll_offset_x=*/0, scroll_offset_y);
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kScrollTargetNotUserScrollable);
    EXPECT_EQ(0, EvalJs(web_contents(),
                        "document.getElementById('nonscroll').scrollTop"));
    EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
  }
}

// Test that a scrolling over a scroller with overflow in one axis only works
// correctly.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_OneAxisScroller) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  int scroll_offset = 80;

  int scroller = GetDOMNodeId(*main_frame(), "#horizontalscroller").value();

  // Try a vertical scroll - it should fail since the scroller has only
  // horizontal overflow.
  {
    BrowserAction action = MakeScroll(*main_frame(), scroller,
                                      /*scroll_offset_x=*/0, scroll_offset);
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kScrollTargetNotUserScrollable);
    EXPECT_EQ(
        0, EvalJs(web_contents(),
                  "document.getElementById('horizontalscroller').scrollTop"));
    EXPECT_EQ(0, EvalJs(web_contents(), "window.scrollY"));
  }

  // Horizontal scroll should succeed.
  {
    BrowserAction action = MakeScroll(*main_frame(), scroller, scroll_offset,
                                      /*scroll_offset_y=*/0);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(
        scroll_offset,
        EvalJs(web_contents(),
               "document.getElementById('horizontalscroller').scrollLeft"));
  }
}

// Ensure scroll distances are correctly scaled when browser zoom is applied.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_BrowserZoom) {
  // Set the default browser page zoom to 150%.
  double level = blink::ZoomFactorToZoomLevel(1.5);
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(level);

  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 60 physical pixels translates to 40 CSS pixels when the zoom factor is 1.5
  // (3 physical pixels : 2 CSS Pixels)
  int scroll_offset_physical = 60;
  int expected_offset_css = 40;
  int scroller = GetDOMNodeId(*main_frame(), "#scroller").value();

  {
    BrowserAction action =
        MakeScroll(*main_frame(), scroller,
                   /*scroll_offset_x=*/0, scroll_offset_physical);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(expected_offset_css,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollTop"));
  }
}

// Ensure scroll distances are correctly scaled when applied to a CSS zoomed
// scroller.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_CSSZoom) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 60 physical pixels translates to 120 CSS pixels since the scroller is
  // inside a `zoom:0.5` subtree (1 physical pixels : 2 CSS Pixels)
  int scroll_offset_physical = 60;
  int expected_offset_css = 120;
  int scroller = GetDOMNodeId(*main_frame(), "#zoomedscroller").value();

  {
    BrowserAction action =
        MakeScroll(*main_frame(), scroller,
                   /*scroll_offset_x=*/0, scroll_offset_physical);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(expected_offset_css,
              EvalJs(web_contents(),
                     "document.getElementById('zoomedscroller').scrollTop"));
  }
}

class ActorToolsTestDSF2 : public ActorToolsTest {
 public:
  ActorToolsTestDSF2() = default;
  explicit ActorToolsTestDSF2(const ActorToolsTest&) = delete;
  ActorToolsTestDSF2& operator=(const ActorToolsTestDSF2&) = delete;

  ~ActorToolsTestDSF2() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

// Ensure scroll distances are correctly scaled when using a non-1 device scale
// factor
IN_PROC_BROWSER_TEST_F(ActorToolsTestDSF2, ScrollTool_ScrollDSF) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // 80 physical pixels translates to 40 CSS pixels when the device scale factor
  // = 2 (2 physical pixels : 1 CSS pixel);
  int scroll_offset_physical = 80;
  int expected_offset_css = 40;
  int scroller = GetDOMNodeId(*main_frame(), "#scroller").value();

  {
    BrowserAction action =
        MakeScroll(*main_frame(), scroller,
                   /*scroll_offset_x=*/0, scroll_offset_physical);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
    EXPECT_EQ(expected_offset_css,
              EvalJs(web_contents(),
                     "document.getElementById('scroller').scrollTop"));
  }
}

IN_PROC_BROWSER_TEST_F(ActorToolsTest, ScrollTool_ZeroIdTargetsViewport) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/scrollable_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // DOMNodeIDs start at 1 so 0 should be interpreted as viewport.
  constexpr int kViewportId = 0;
  float scroll_offset_y = 50;
  BrowserAction action = MakeScroll(*main_frame(), kViewportId,
                                    /*scroll_offset_x=*/0, scroll_offset_y);

  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(action, result.GetCallback());
  ExpectOkResult(result);

  // Not sure why, since all zooms should be exactly 1.0, but some numerical
  // instability seems to creep in. Using ExtractDouble and EXPECT_FLOAT_EQ for
  // that reason.
  EXPECT_FLOAT_EQ(scroll_offset_y,
                  EvalJs(web_contents(), "window.scrollY").ExtractDouble());
}

// ===============================================
// Drag and Release Tool
// ===============================================

// Test the drag and release tool by moving the thumb on a range slider control.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, DragAndReleaseTool_Range) {
  const GURL url = embedded_test_server()->GetURL("/actor/drag.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  gfx::RectF range_rect = GetBoundingClientRect(*main_frame(), "#range");

  ASSERT_EQ(0, GetRangeValue(*main_frame(), "#range"));

  // Padding to roughly hit the center of the range drag thumb.
  const int thumb_padding = range_rect.height() / 2;

  gfx::Point start(range_rect.x() + thumb_padding,
                   range_rect.y() + thumb_padding);
  gfx::Point end = gfx::ToFlooredPoint(range_rect.CenterPoint());

  BrowserAction action = MakeDragAndRelease(*main_frame(), start, end);

  TestFuture<mojom::ActionResultPtr> result_success;
  actor_coordinator().Act(action, result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(50, GetRangeValue(*main_frame(), "#range"));
}

// Ensure the drag tool sends the expected mouse down, move and up events.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, DragAndReleaseTool_Events) {
  const GURL url = embedded_test_server()->GetURL("/actor/drag.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // The dragLogger starts in the bottom right of the viewport. Scroll it to the
  // top left to ensure client coordinates are being used (i.e. drag coordinates
  // should not be affected by scroll and should match the mousemove client
  // coordinates reported by the page).
  ASSERT_TRUE(ExecJs(web_contents(), "window.scrollTo(450, 250)"));

  // Log starts off empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  gfx::RectF target_rect = GetBoundingClientRect(*main_frame(), "#dragLogger");

  // Arbitrary pad to hit a few pixels inside the logger element.
  const int kPadding = 10;
  gfx::Vector2d delta(100, 150);
  gfx::Point start(target_rect.x() + kPadding, target_rect.y() + kPadding);
  gfx::Point end = start + delta;

  BrowserAction action = MakeDragAndRelease(*main_frame(), start, end);

  TestFuture<mojom::ActionResultPtr> result_success;
  actor_coordinator().Act(action, result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(base::StrCat({"mousemove[", start.ToString(), "],", "mousedown[",
                          start.ToString(), "],", "mousemove[", end.ToString(),
                          "],", "mouseup[", end.ToString(), "]"}),
            EvalJs(web_contents(), "event_log.join(',')"));
}

// Ensure coordinates outside of the viewport are rejected.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, DragAndReleaseTool_Offscreen) {
  const GURL url = embedded_test_server()->GetURL("/actor/drag.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Log starts off empty.
  ASSERT_EQ("", EvalJs(web_contents(), "event_log.join(',')"));

  // Try to drag the range - it should fail since the range is offscreen (and so
  // the range_rect has bounds outside the viewport).
  {
    gfx::RectF range_rect =
        GetBoundingClientRect(*main_frame(), "#offscreenRange");

    // Padding to roughly hit the center of the range drag thumb.
    const int thumb_padding = range_rect.height() / 2;
    gfx::Point start(range_rect.x() + thumb_padding,
                     range_rect.y() + thumb_padding);
    gfx::Point end = gfx::ToFlooredPoint(range_rect.CenterPoint());

    BrowserAction action = MakeDragAndRelease(*main_frame(), start, end);
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kDragAndReleaseFromOffscreen);
  }

  // Scroll the range into the viewport.
  ASSERT_TRUE(
      ExecJs(web_contents(),
             "document.getElementById('offscreenRange').scrollIntoView()"));

  // Try to drag the range - now that it's been scrolled into the viewport this
  // should succeed.
  {
    // Recompute the client rect since it depends on scroll offset.
    gfx::RectF range_rect =
        GetBoundingClientRect(*main_frame(), "#offscreenRange");
    const int thumb_padding = range_rect.height() / 2;
    gfx::Point start(range_rect.x() + thumb_padding,
                     range_rect.y() + thumb_padding);
    gfx::Point end = gfx::ToFlooredPoint(range_rect.CenterPoint());

    BrowserAction action = MakeDragAndRelease(*main_frame(), start, end);
    TestFuture<mojom::ActionResultPtr> result_success;
    actor_coordinator().Act(action, result_success.GetCallback());
    ExpectOkResult(result_success);
  }

  EXPECT_EQ(50, GetRangeValue(*main_frame(), "#offscreenRange"));
}

// ===============================================
// Navigate Tool
// ===============================================

// Basic test of the NavigateTool.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, NavigateTool) {
  const GURL url_start =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_target =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  BrowserAction action;
  NavigateAction* navigate =
      action.add_action_information()->mutable_navigate();
  navigate->mutable_url()->assign(url_target.spec());

  TestFuture<mojom::ActionResultPtr> result_success;
  actor_coordinator().Act(action, result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_target);
}

// ===============================================
// History Tool
// ===============================================

// TODO(crbug.com/415385900): Add a test for navigation API canceling a
// same-document navigation.

// Basic test of the HistoryTool going back.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_Back) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TestFuture<mojom::ActionResultPtr> result_success;
  actor_coordinator().Act(MakeHistoryBack(), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_first);
}

// Basic test of the HistoryTool going forward
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_Forward) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  GoBack();
  ASSERT_EQ(web_contents()->GetURL(), url_first);

  TestFuture<mojom::ActionResultPtr> result_success;
  actor_coordinator().Act(MakeHistoryForward(), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_second);
}

// Basic test will, under normal circumstances use BFCache. Ensure coverage
// without BFCache as well.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_BackNoBFCache) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TestFuture<mojom::ActionResultPtr> result_success;
  actor_coordinator().Act(MakeHistoryBack(), result_success.GetCallback());
  ExpectOkResult(result_success);

  EXPECT_EQ(web_contents()->GetURL(), url_first);
}

// Test that tool fails validation if there's no further session history in the
// direction of travel.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_FailNoSessionHistory) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?first");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?second");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  // Attempting a forward history navigation should fail since we're at the
  // latest entry.
  {
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(MakeHistoryForward(), result.GetCallback());
    ExpectErrorResult(result,
                      mojom::ActionResultCode::kHistoryNoForwardEntries);
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }

  // Prune all earlier entries so we can't go back.
  web_contents()->GetController().PruneAllButLastCommitted();
  ASSERT_FALSE(web_contents()->GetController().CanGoBack());

  // Attempting a back history navigation should fail since we're at the first
  // entry.
  {
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(MakeHistoryBack(), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kHistoryNoBackEntries);
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }
}

// Test history tool across same document navigations
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_BackSameDocument) {
  const GURL url_first = embedded_test_server()->GetURL("/actor/blank.html");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html#foo");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  {
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(MakeHistoryBack(), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(web_contents()->GetURL(), url_first);
  }

  {
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(MakeHistoryForward(), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(web_contents()->GetURL(), url_second);
  }
}

// Test history tool across same document navigations
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_BasicIframeBack) {
  const GURL main_frame_url =
      embedded_test_server()->GetURL("/actor/simple_iframe.html");
  const GURL child_frame_url_1 =
      embedded_test_server()->GetURL("/actor/blank.html");
  const GURL child_frame_url_2 =
      embedded_test_server()->GetURL("/actor/blank.html?next");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate the child frame to a new document.
  RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);
  ASSERT_EQ(child_frame->GetLastCommittedURL(), child_frame_url_1);
  ASSERT_TRUE(
      content::NavigateToURLFromRenderer(child_frame, child_frame_url_2));
  child_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_EQ(child_frame->GetLastCommittedURL(), child_frame_url_2);

  // Invoke the history back tool. The iframe should be navigated back.
  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(MakeHistoryBack(), result.GetCallback());
  ExpectOkResult(result);
  child_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(child_frame->GetLastCommittedURL(), child_frame_url_1);
  EXPECT_EQ(web_contents()->GetURL(), main_frame_url);
}

// Ensure the history tool doesn't return until the navigation completes.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_SlowBack) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::DisableForTestingReason::
                          TEST_REQUIRES_NO_CACHING);

  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  TestNavigationManager back_navigation(web_contents(), url_first);
  TestFuture<mojom::ActionResultPtr> result_success;
  actor_coordinator().Act(MakeHistoryBack(), result_success.GetCallback());
  ASSERT_TRUE(back_navigation.WaitForResponse());
  EXPECT_FALSE(result_success.IsReady());

  for (int i = 0; i < 3; ++i) {
    TinyWait();
    EXPECT_FALSE(result_success.IsReady());
  }

  ASSERT_TRUE(back_navigation.WaitForNavigationFinished());
  ExpectOkResult(result_success);
}

// Test a case where history back causes navigation in two frames.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_ConcurrentNavigations) {
  const GURL main_frame_url =
      embedded_test_server()->GetURL("/actor/concurrent_navigations.html");
  const GURL child_frame_1_start_url =
      embedded_test_server()->GetURL("/actor/blank.html?A1");
  const GURL child_frame_1_target_url =
      embedded_test_server()->GetURL("/actor/blank.html?A2");
  const GURL child_frame_2_start_url =
      embedded_test_server()->GetURL("/actor/blank.html?B1");
  const GURL child_frame_2_target_url =
      embedded_test_server()->GetURL("/actor/blank.html?B2");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_frame_url));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate the child frame to a new document.
  RenderFrameHost* child_frame_1 =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  RenderFrameHost* child_frame_2 =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  ASSERT_TRUE(child_frame_1);
  ASSERT_TRUE(child_frame_2);
  ASSERT_EQ(child_frame_1->GetLastCommittedURL(), child_frame_1_start_url);
  ASSERT_EQ(child_frame_2->GetLastCommittedURL(), child_frame_2_start_url);

  ASSERT_TRUE(content::NavigateToURLFromRenderer(child_frame_1,
                                                 child_frame_1_target_url));
  child_frame_1 =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_EQ(child_frame_1->GetLastCommittedURL(), child_frame_1_target_url);

  // The first frame navigated to A2 so the session history looks like:
  // [about:blank], [Main, A1, B1], [Main, A2, B1]

  // Now navigate the second iframe but with replacement so we get:
  // [about:blank], [Main, A1, B1], [Main, A2, B2]
  TestNavigationManager replace_navigation(web_contents(),
                                           child_frame_2_target_url);
  ASSERT_TRUE(ExecJs(child_frame_2, JsReplace("location.replace($1);",
                                              child_frame_2_target_url)));
  ASSERT_TRUE(replace_navigation.WaitForNavigationFinished());
  child_frame_2 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  ASSERT_EQ(child_frame_2->GetLastCommittedURL(), child_frame_2_target_url);

  // Invoke the history back tool. Both should be navigated back to their
  // starting URL.
  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(MakeHistoryBack(), result.GetCallback());
  ExpectOkResult(result);

  child_frame_1 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  child_frame_2 = ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  EXPECT_EQ(child_frame_1->GetLastCommittedURL(), child_frame_1_start_url);
  EXPECT_EQ(child_frame_2->GetLastCommittedURL(), child_frame_2_start_url);
  EXPECT_EQ(web_contents()->GetURL(), main_frame_url);
}

// Ensure the history tool works correctly when a before unload handler is
// present (but doesn't cause a prompt to show).
IN_PROC_BROWSER_TEST_F(ActorToolsTest, HistoryTool_HasBeforeUnload) {
  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));

  // Add a no-op beforeunload handler. This won't show the prompt but may force
  // the browser to send an event to the renderer to confirm which can change
  // the async path taken by the navigation.
  ASSERT_TRUE(ExecJs(web_contents(),
                     R"JS(
                      addEventListener('beforeunload', () => {});
                      )JS"));

  TestFuture<mojom::ActionResultPtr> result_success;
  actor_coordinator().Act(MakeHistoryBack(), result_success.GetCallback());
  ExpectOkResult(result_success);
  EXPECT_EQ(web_contents()->GetURL(), url_first);
}

// ===============================================
// Select Tool
// ===============================================

// Test that the SelectTool can select an ordinary <option> in a <select>
// element.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_OptionSelected) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  const int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();

  ASSERT_EQ(GetSelectElementCurrentValue(plain_select_id), "alpha");

  {
    BrowserAction select =
        MakeSelect(*main_frame(), plain_select_dom_node_id, "beta");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(plain_select_id), "beta");

  {
    BrowserAction select =
        MakeSelect(*main_frame(), plain_select_dom_node_id, "gamma");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());

    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(plain_select_id), "gamma");

  // Test selecting by value. The option with value last has text "omega".
  {
    BrowserAction select =
        MakeSelect(*main_frame(), plain_select_dom_node_id, "last");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());

    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(plain_select_id), "last");
}

// Test that the SelectTool causes the change and input events to fire on the
// <select> element.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_Events) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  const int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();

  ASSERT_EQ(GetSelectElementCurrentValue(plain_select_id), "alpha");
  ASSERT_EQ("", EvalJs(web_contents(), "select_event_log.join(',')"));

  {
    BrowserAction select =
        MakeSelect(*main_frame(), plain_select_dom_node_id, "beta");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ("input,change",
              EvalJs(web_contents(), "select_event_log.join(',')"));
  }
}

// Test that attempting to select a value that does not exist in the <option>
// list fails and does not change the current selection.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_NonExistentValueFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();

  const std::string initial_value =
      GetSelectElementCurrentValue(plain_select_id);
  ASSERT_EQ(initial_value, "alpha");

  BrowserAction select =
      MakeSelect(*main_frame(), plain_select_dom_node_id, "nonexistentValue");
  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(select, result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kSelectNoSuchOption);

  EXPECT_EQ(GetSelectElementCurrentValue(plain_select_id), initial_value);
}

// Test that attempting to select a value corresponding to a non-<option>
// element fails. The select tool should only target valid options.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_NonOptionNodeValueFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string non_options_select_id = "#nonOptionsSelect";
  int32_t non_options_select_dom_node_id =
      GetDOMNodeId(*main_frame(), non_options_select_id).value();

  const std::string initial_value =
      GetSelectElementCurrentValue(non_options_select_id);
  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select "beta", which is the text of a <span>, not an <option>
  // value.  Expect the action to fail.
  {
    BrowserAction select =
        MakeSelect(*main_frame(), non_options_select_dom_node_id, "beta");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kSelectNoSuchOption);
  }

  // Expect the value to remain unchanged
  EXPECT_EQ(GetSelectElementCurrentValue(non_options_select_id), initial_value);

  // Attempt to select "gamma", which is the value property of a <button>
  // element, not an <option> value.  Expect the action to fail.
  {
    BrowserAction select =
        MakeSelect(*main_frame(), non_options_select_dom_node_id, "gamma");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kSelectNoSuchOption);
  }

  // Expect the value to remain unchanged
  EXPECT_EQ(GetSelectElementCurrentValue(non_options_select_id), initial_value);

  // Attempt to select "epsilon". This should succeed as there is an <option>
  // with value epsilon, despite there also being a <button> with value
  // "epsilon".
  {
    BrowserAction select =
        MakeSelect(*main_frame(), non_options_select_dom_node_id, "epsilon");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(GetSelectElementCurrentValue(non_options_select_id), "epsilon");
  }
}

// Test that matching option values is case-sensitive.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_ValueIsCaseSensitive) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();
  const std::string initial_value =
      GetSelectElementCurrentValue(plain_select_id);

  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select "BETA" which has different casing than the option "beta"
  // Expect the action to fail due to case mismatch.
  BrowserAction select =
      MakeSelect(*main_frame(), plain_select_dom_node_id, "BETA");
  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(select, result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kSelectNoSuchOption);

  // The select value should be unchanged.
  EXPECT_EQ(GetSelectElementCurrentValue(plain_select_id), initial_value);
}

// Test that attempting to select a disabled <option> fails.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_DisabledOptionFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();
  const std::string initial_value =
      GetSelectElementCurrentValue(plain_select_id);

  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select the value of the disabled option. Expect the action to
  // fail and the select's value to be unchanged.
  BrowserAction select =
      MakeSelect(*main_frame(), plain_select_dom_node_id, "disabledOption");
  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(select, result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kSelectOptionDisabled);
  EXPECT_EQ(GetSelectElementCurrentValue(plain_select_id), initial_value);
}

// Test that attempting to select a <option> in a disabled <optgroup> fails.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_DisabledOptGroupFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string group_select_id = "#groupedSelect";
  int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), group_select_id).value();
  const std::string initial_value =
      GetSelectElementCurrentValue(group_select_id);

  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select the option with value "foobar". The option itself is
  // enabled but is in a disabled optgroup. Expect the action to fail and the
  // select's value to be unchanged.
  BrowserAction select =
      MakeSelect(*main_frame(), plain_select_dom_node_id, "foobar");
  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(select, result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kSelectOptionDisabled);
  EXPECT_EQ(GetSelectElementCurrentValue(group_select_id), initial_value);
}

// Test that attempting to select any option in a disabled <select> element
// fails.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_DisabledSelectFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string disabled_select_id = "#disabledSelect";
  int32_t disabled_select_dom_node_id =
      GetDOMNodeId(*main_frame(), disabled_select_id).value();
  const std::string initial_value =
      GetSelectElementCurrentValue(disabled_select_id);

  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select an otherwise valid option value ("beta"). Expect the
  // action to fail without affecting the <select>.
  BrowserAction select =
      MakeSelect(*main_frame(), disabled_select_dom_node_id, "beta");
  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(select, result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementDisabled);
  EXPECT_EQ(GetSelectElementCurrentValue(disabled_select_id), initial_value);
}

// Test that options within <optgroup> elements can be selected.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_GroupedOptionSelected) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string grouped_select_id = "#groupedSelect";
  int32_t grouped_select_dom_node_id =
      GetDOMNodeId(*main_frame(), grouped_select_id).value();

  ASSERT_EQ(GetSelectElementCurrentValue(grouped_select_id), "alpha");

  // Select an option from the first group
  {
    BrowserAction select =
        MakeSelect(*main_frame(), grouped_select_dom_node_id, "gamma");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(grouped_select_id), "gamma");

  // Select an option from the second group
  {
    BrowserAction select =
        MakeSelect(*main_frame(), grouped_select_dom_node_id, "b");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(grouped_select_id), "b");
}

// Test that an option can be selected in a <select> element rendered as a
// listbox (size attribute > 1).
IN_PROC_BROWSER_TEST_F(ActorToolsTest, SelectTool_ListboxOptionSelected) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string listbox_select_id = "#listboxSelect";
  int32_t listbox_select_dom_node_id =
      GetDOMNodeId(*main_frame(), listbox_select_id).value();

  // List box starts with no element selected.
  ASSERT_EQ(GetSelectElementCurrentValue(listbox_select_id), "");

  {
    BrowserAction select =
        MakeSelect(*main_frame(), listbox_select_dom_node_id, "beta");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(listbox_select_id), "beta");

  {
    BrowserAction select =
        MakeSelect(*main_frame(), listbox_select_dom_node_id, "delta");
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(select, result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(listbox_select_id), "delta");
}

// ===============================================
// Wait Tool
// ===============================================

IN_PROC_BROWSER_TEST_F(ActorToolsTest, WaitTool) {
  WaitTool::SetNoDelayForTesting();

  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  BrowserAction wait = MakeWait();
  TestFuture<mojom::ActionResultPtr> result;
  actor_coordinator().Act(wait, result.GetCallback());
  ExpectOkResult(result);
}

}  // namespace

}  // namespace actor
