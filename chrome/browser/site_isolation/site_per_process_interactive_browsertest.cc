// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/constants.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/test/with_feature_override.h"
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "pdf/pdf_features.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

// Counts and returns the number of RenderWidgetHosts in the browser process.
size_t GetNumberOfRenderWidgetHosts() {
  std::unique_ptr<content::RenderWidgetHostIterator> all_widgets =
      content::RenderWidgetHost::GetRenderWidgetHosts();
  size_t count = 0;
  while (all_widgets->GetNextHost()) {
    count++;
  }
  return count;
}

// Waits and polls the current number of RenderWidgetHosts and stops when the
// number reaches |target_count|.
void WaitForRenderWidgetHostCount(size_t target_count) {
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetNumberOfRenderWidgetHosts() == target_count; }));
}

}  // namespace

class SitePerProcessInteractiveBrowserTest : public InProcessBrowserTest {
 public:
  SitePerProcessInteractiveBrowserTest() {}

  SitePerProcessInteractiveBrowserTest(
      const SitePerProcessInteractiveBrowserTest&) = delete;
  SitePerProcessInteractiveBrowserTest& operator=(
      const SitePerProcessInteractiveBrowserTest&) = delete;

  ~SitePerProcessInteractiveBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  gfx::Size GetScreenSize() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestView(
            web_contents->GetRenderWidgetHostView()->GetNativeView());
    return display.bounds().size();
  }

  enum class FullscreenExitMethod {
    JS_CALL,
    ESC_PRESS,
  };

  void FullscreenElementInABA(FullscreenExitMethod exit_method);
};

class SitePerProcessInteractiveFencedFrameBrowserTest
    : public SitePerProcessInteractiveBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  SitePerProcessInteractiveFencedFrameBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    https_server()->ServeFilesFromSourceDirectory("content/test/data");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(https_server());

    ASSERT_TRUE(https_server()->Start());
  }

  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    return info.param;
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::HistogramTester histogram_tester_;
};

// Check that document.hasFocus() works properly with out-of-process iframes.
// The test builds a page with four cross-site frames and then focuses them one
// by one, checking the value of document.hasFocus() in all frames.  For any
// given focused frame, document.hasFocus() should return true for that frame
// and all its ancestor frames.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest, DocumentHasFocus) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child1 = ChildFrameAt(main_frame, 0);
  ASSERT_NE(nullptr, child1);
  content::RenderFrameHost* child2 = ChildFrameAt(main_frame, 1);
  ASSERT_NE(nullptr, child2);
  content::RenderFrameHost* grandchild = ChildFrameAt(child1, 0);
  ASSERT_NE(nullptr, grandchild);

  EXPECT_NE(main_frame->GetSiteInstance(), child1->GetSiteInstance());
  EXPECT_NE(main_frame->GetSiteInstance(), child2->GetSiteInstance());
  EXPECT_NE(child1->GetSiteInstance(), grandchild->GetSiteInstance());

  // Helper function to check document.hasFocus() for a given frame.
  auto document_has_focus = [](content::RenderFrameHost* rfh) -> bool {
    return EvalJs(rfh, "document.hasFocus()").ExtractBool();
  };

  // The main frame should be focused to start with.
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_FALSE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecJs(child1, "window.focus();"));
  EXPECT_EQ(child1, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_TRUE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecJs(grandchild, "window.focus();"));
  EXPECT_EQ(grandchild, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_TRUE(document_has_focus(child1));
  EXPECT_TRUE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecJs(child2, "window.focus();"));
  EXPECT_EQ(child2, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_FALSE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_TRUE(document_has_focus(child2));
}

// Ensure that a cross-process subframe can receive keyboard events when in
// focus.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       SubframeKeyboardEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_input_field.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "child0", frame_url));

  // Focus the subframe and then its input field.  The return value
  // "input-focus" will be sent once the input field's focus event fires.
  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  std::string script =
      "function onInput(e) {"
      "  resultQueue.push(getInputFieldText());"
      "}"
      "inputField = document.getElementById('text-field');"
      "inputField.addEventListener('input', onInput, false);";
  EXPECT_TRUE(ExecJs(child, script));
  EXPECT_EQ("input-focus", EvalJs(child, "window.focus(); focusInputField();"));

  // The subframe should now be focused.
  EXPECT_EQ(child, web_contents->GetFocusedFrame());

  // Generate a few keyboard events and route them to currently focused frame.
  // We wait for replies to be sent back from the page, since keystrokes may
  // take time to propagate to the renderer's main thread.
  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('F'),
                   ui::DomCode::US_F, ui::VKEY_F, false, false, false, false);
  EXPECT_EQ("F", EvalJs(child, "waitForInput();"));

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('O'),
                   ui::DomCode::US_O, ui::VKEY_O, false, false, false, false);
  EXPECT_EQ("FO", EvalJs(child, "waitForInput();"));

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('O'),
                   ui::DomCode::US_O, ui::VKEY_O, false, false, false, false);
  EXPECT_EQ("FOO", EvalJs(child, "waitForInput();"));
}

// Ensure that sequential focus navigation (advancing focused elements with
// <tab> and <shift-tab>) works across cross-process subframes. This has 2 test
// cases that check sequential focus navigation for both <iframe> and
// <fencedframe> elements.
// The test sets up six inputs fields in a page with two cross-process
// subframes:
//                 child1            child2
//             /------------\    /------------\.
//             | 2. <input> |    | 4. <input> |
//  1. <input> | 3. <input> |    | 5. <input> |  6. <input>
//             \------------/    \------------/.
//
// The test then presses <tab> six times to cycle through focused elements 1-6.
// The test then repeats this with <shift-tab> to cycle in reverse order.
IN_PROC_BROWSER_TEST_P(SitePerProcessInteractiveFencedFrameBrowserTest,
                       SequentialFocusNavigation) {
  GURL main_url(https_server()->GetURL(
      "a.test", GetParam() == std::string("iframe")
                    ? "/cross_site_iframe_factory.html?a.test(b.test,c.test)"
                    : "/cross_site_iframe_factory.html?a.test(b.test{fenced},c."
                      "test{fenced})"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child1 = nullptr;
  content::RenderFrameHost* child2 = nullptr;

  if (GetParam() == std::string("iframe")) {
    child1 = ChildFrameAt(main_frame, 0);
    ASSERT_NE(nullptr, child1);
    child2 = ChildFrameAt(main_frame, 1);
    ASSERT_NE(nullptr, child2);
  } else {
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(main_frame);
    ASSERT_EQ(child_frames.size(), 2u);
    child1 = child_frames[0];
    child2 = child_frames[1];
  }

  content::WaitForHitTestData(child1);
  content::WaitForHitTestData(child2);

  // Assign a name to each frame.  This will be sent along in test messages
  // from focus events.
  EXPECT_TRUE(ExecJs(main_frame, "window.name = 'root';"));
  EXPECT_TRUE(ExecJs(child1, "window.name = 'child1';"));
  EXPECT_TRUE(ExecJs(child2, "window.name = 'child2';"));

  // This script will insert two <input> fields in the document, one at the
  // beginning and one at the end.  For root frame, this means that we will
  // have an <input>, then two <iframe> elements, then another <input>.
  std::string script =
      "function onFocus(e) {"
      "  domAutomationController.send(window.name + '-focused-' + e.target.id);"
      "}"
      "var input1 = document.createElement('input');"
      "input1.id = 'input1';"
      "var input2 = document.createElement('input');"
      "input2.id = 'input2';"
      "document.body.insertBefore(input1, document.body.firstChild);"
      "document.body.appendChild(input2);"
      "input1.addEventListener('focus', onFocus, false);"
      "input2.addEventListener('focus', onFocus, false);";

  // Add two input fields to each of the three frames.
  EXPECT_TRUE(ExecJs(main_frame, script));
  EXPECT_TRUE(ExecJs(child1, script));
  EXPECT_TRUE(ExecJs(child2, script));

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message = [web_contents](bool reverse) {
    content::DOMMessageQueue msg_queue(web_contents);
    std::string reply;
    SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, reverse /* shift */, false, false);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    return reply;
  };

  // Press <tab> six times to focus each of the <input> elements in turn.
  EXPECT_EQ("\"root-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child1-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(child1, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child1-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ("\"child2-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(child2, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child2-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ("\"root-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  // Now, press <shift-tab> to navigate focus in the reverse direction.
  EXPECT_EQ("\"child2-focused-input2\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ(child2, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child2-focused-input1\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ("\"child1-focused-input2\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ(child1, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child1-focused-input1\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ("\"root-focused-input1\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());
}

// Similar to the test above, but check that sequential focus navigation works
// with <object> tags that contain OOPIFs.
//
// The test sets up four inputs fields in a page with a <object> that contains
// an OOPIF:
//                <object>
//             /------------\.
//             | 2. <input> |
//  1. <input> | 3. <input> |  4. <input>
//             \------------/.
//
// The test then presses <tab> 4 times to cycle through focused elements 1-4.
// The test then repeats this with <shift-tab> to cycle in reverse order.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       SequentialFocusNavigationWithObject) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_object_fallback.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  content::TestNavigationObserver observer(web_contents);
  GURL object_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(content::ExecJs(
      main_frame,
      content::JsReplace("document.querySelector('object').data = $1;",
                         object_url)));
  observer.Wait();
  content::RenderFrameHost* object = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(object);
  ASSERT_TRUE(object->IsCrossProcessSubframe());
  EXPECT_EQ(object_url, object->GetLastCommittedURL());

  // Assign a name to each frame.  This will be sent along in test messages
  // from focus events.
  EXPECT_TRUE(ExecJs(main_frame, "window.name = 'root';"));
  EXPECT_TRUE(ExecJs(object, "window.name = 'object';"));

  // This script will insert two <input> fields in the document, one at the
  // beginning and one at the end.  For root frame, this means that we will
  // have an <input>, then an <object> element, then another <input>.
  std::string script =
      "function onFocus(e) {"
      "  domAutomationController.send(window.name + '-focused-' + e.target.id);"
      "}"
      "var input1 = document.createElement('input');"
      "input1.id = 'input1';"
      "var input2 = document.createElement('input');"
      "input2.id = 'input2';"
      "document.body.insertBefore(input1, document.body.firstChild);"
      "document.body.appendChild(input2);"
      "input1.addEventListener('focus', onFocus, false);"
      "input2.addEventListener('focus', onFocus, false);";

  // Add two input fields to each of the two frames.
  EXPECT_TRUE(ExecJs(main_frame, script));
  EXPECT_TRUE(ExecJs(object, script));

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message = [web_contents](bool reverse) {
    content::DOMMessageQueue msg_queue(web_contents);
    std::string reply;
    SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, reverse /* shift */, false, false);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    return reply;
  };

  // Press <tab> four times to focus each of the <input> elements in turn.
  EXPECT_EQ("\"root-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"object-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(object, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"object-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ("\"root-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  // Now, press <shift-tab> to navigate focus in the reverse direction.
  EXPECT_EQ("\"object-focused-input2\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ(object, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"object-focused-input1\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ("\"root-focused-input1\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());
}

// Ensure that frames get focus when wrapping focus using <tab> or <shift-tab>.
// This has 2 test cases that check sequential focus navigation for both
// <iframe> and <fencedframe> elements.
// The test sets up two input fields in a page with one cross-process subframe:
//                  child
//              /------------\.
//  1. <input>  | 2. <input> |
//              \------------/.
//
// The test then presses <tab> to focus on elements 1, then <shift-tab> twice to
// focus on the omnibox followed by element 2. This tests that focus works as
// expected when wrapping through non-page UI elements. Specifically, this tests
// that fenced frames can properly get and verify focus if its RenderWidgetHost
// loses focus.
IN_PROC_BROWSER_TEST_P(SitePerProcessInteractiveFencedFrameBrowserTest,
                       SequentialFocusNavigationWrapAround) {
  GURL main_url(https_server()->GetURL(
      "a.test", GetParam() == std::string("fencedframe")
                    ? "/cross_site_iframe_factory.html?a.test(b.test{fenced})"
                    : "/cross_site_iframe_factory.html?a.test(b.test)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child = nullptr;

  if (GetParam() == std::string("iframe")) {
    child = ChildFrameAt(main_frame, 0);
    ASSERT_NE(nullptr, child);
  } else {
    child =
        fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(main_frame);
    ASSERT_NE(nullptr, child);
  }

  content::WaitForHitTestData(child);

  // Assign a name to each frame.  This will be sent along in test messages
  // from focus events.
  EXPECT_TRUE(ExecJs(main_frame, "window.name = 'root';"));
  EXPECT_TRUE(ExecJs(child, "window.name = 'child';"));

  // This script will insert one <input> field at the beginning of the document.
  // For root frame, this means that we will have an <input> element followed by
  // an <iframe>.
  std::string script =
      "function onFocus(e) {"
      "  domAutomationController.send(window.name + '-focused-' + e.target.id);"
      "}"
      "var input1 = document.createElement('input');"
      "input1.id = 'input1';"
      "document.body.insertBefore(input1, document.body.firstChild);"
      "input1.addEventListener('focus', onFocus, false);";

  // Add two input fields to each of the three frames.
  EXPECT_TRUE(ExecJs(main_frame, script));
  EXPECT_TRUE(ExecJs(child, script));

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message = [web_contents](bool reverse) {
    content::DOMMessageQueue msg_queue(web_contents);
    std::string reply;
    SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, reverse /* shift */, false, false);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    return reply;
  };

  // Press <tab> to focus the <input> element in the main frame.
  EXPECT_EQ("\"root-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  auto frame_focused = std::make_unique<content::FrameFocusedObserver>(child);
  // Press <shift-tab> twice to focus on the UI and then wrap around to the
  // child frame.
  SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, /*shift=*/true, false, false);
  SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, /*shift=*/true, false, false);

  // Wait for the child frame to get focus.
  frame_focused->Wait();
}

// Ensure that frames can pass focus to subsequent frames if there is nothing
// focusable left in their frame. The test sets up two input fields in a page
// with three cross-process subframes:
//                  child1          child3
//              /------------\. /------------\.
//  1. <input>  |   child2   |  | 2. <input> |
//              | /--------\ |  \------------/.
//              | \--------/ |
//              \------------/.
//
// The test then presses <tab> twice to focus on elements 1 and 2.
// TODO(crbug.com/40276413): Re-enable this test once this bug is fixed.
IN_PROC_BROWSER_TEST_P(SitePerProcessInteractiveFencedFrameBrowserTest,
                       SequentialFocusNavigationPassThrough) {
  GURL main_url(https_server()->GetURL(
      "a.test",
      GetParam() == std::string("fencedframe")
          ? "/cross_site_iframe_factory.html?a.test(b.test{fenced}(c.test{"
            "fenced}),d.test{fenced})"
          : "/cross_site_iframe_factory.html?a.test(b.test(c.test),d.test)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child1 = nullptr;
  content::RenderFrameHost* child2 = nullptr;
  content::RenderFrameHost* child3 = nullptr;

  if (GetParam() == std::string("iframe")) {
    child1 = ChildFrameAt(main_frame, 0);
    child2 = ChildFrameAt(child1, 0);
    child3 = ChildFrameAt(main_frame, 1);
  } else {
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(main_frame);
    ASSERT_EQ(child_frames.size(), 2u);
    child1 = child_frames[0];
    child2 = fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(child1);
    child3 = child_frames[1];
  }

  ASSERT_NE(nullptr, child1);
  ASSERT_NE(nullptr, child2);
  ASSERT_NE(nullptr, child3);

  content::WaitForHitTestData(child1);
  content::WaitForHitTestData(child2);
  content::WaitForHitTestData(child3);

  // Assign a name to each frame.  This will be sent along in test messages
  // from focus events.
  EXPECT_TRUE(ExecJs(main_frame, "window.name = 'root';"));
  EXPECT_TRUE(ExecJs(child1, "window.name = 'child1';"));
  EXPECT_TRUE(ExecJs(child2, "window.name = 'child2';"));
  EXPECT_TRUE(ExecJs(child3, "window.name = 'child3';"));

  // This script will insert one <input> field at the beginning of the document.
  // For root frame, this means that we will have an <input> element followed by
  // a <fencedframe>.
  std::string script =
      "function onFocus(e) {"
      "  console.log(window.name + '-focused-' + e.target.id);"
      "  domAutomationController.send(window.name + '-focused-' + e.target.id);"
      "}"
      "var input1 = document.createElement('input');"
      "input1.id = 'input1';"
      "document.body.insertBefore(input1, document.body.firstChild);"
      "input1.addEventListener('focus', onFocus, false);";

  // Add one input field to the main frame and last fenced frame.
  EXPECT_TRUE(ExecJs(main_frame, script));
  EXPECT_TRUE(ExecJs(child3, script));

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message = [web_contents](bool reverse) {
    content::DOMMessageQueue msg_queue(web_contents);
    std::string reply;
    SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, reverse /* shift */, false, false);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    return reply;
  };

  // Press <tab> to focus the <input> element in the main frame.
  EXPECT_EQ("\"root-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  // Press <tab> to move focus to the <input> element in child3 fenced frame.
  EXPECT_EQ("\"child3-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(child3, web_contents->GetFocusedFrame());
}

// Ensure that frames can pass focus to subsequent frames if there is nothing
// focusable left in their frame. The test sets up two input fields in the last
// subframe loaded into a page:
//       child1          child3
//   /------------\. /-------------\.
//   |   child2   |  | 1. <input1> |
//   | /--------\ |  \ 2. <input2> /
//   | \--------/ |  \-------------/.
//   \------------/.
//
// The test then presses <tab> twice to focus on elements 1 and 2, <tab> to
// move focus to the UI, and <tab> one more time to focus on element 1 again.
// TODO(crbug.com/40276413): Re-enable this test once this bug is fixed.
IN_PROC_BROWSER_TEST_P(SitePerProcessInteractiveFencedFrameBrowserTest,
                       SequentialFocusWrapBackIntoChildFrame) {
  GURL main_url(https_server()->GetURL(
      "a.test",
      GetParam() == std::string("fencedframe")
          ? "/cross_site_iframe_factory.html?a.test(b.test{fenced}(c.test{"
            "fenced}),d.test{fenced})"
          : "/cross_site_iframe_factory.html?a.test(b.test(c.test),d.test)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child1 = nullptr;
  content::RenderFrameHost* child2 = nullptr;
  content::RenderFrameHost* child3 = nullptr;

  if (GetParam() == std::string("iframe")) {
    child1 = ChildFrameAt(main_frame, 0);
    child2 = ChildFrameAt(child1, 0);
    child3 = ChildFrameAt(main_frame, 1);
  } else {
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(main_frame);
    ASSERT_EQ(child_frames.size(), 2u);
    child1 = child_frames[0];
    child2 = fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(child1);
    child3 = child_frames[1];
  }

  ASSERT_NE(nullptr, child1);
  ASSERT_NE(nullptr, child2);
  ASSERT_NE(nullptr, child3);

  content::WaitForHitTestData(child1);
  content::WaitForHitTestData(child2);
  content::WaitForHitTestData(child3);

  // Assign a name to each frame.  This will be sent along in test messages
  // from focus events.
  EXPECT_TRUE(ExecJs(main_frame, "window.name = 'root';"));
  EXPECT_TRUE(ExecJs(child1, "window.name = 'child1';"));
  EXPECT_TRUE(ExecJs(child2, "window.name = 'child2';"));
  EXPECT_TRUE(ExecJs(child3, "window.name = 'child3';"));

  // This script will insert two <input> fields at the beginning of the
  // document.
  std::string script =
      "function onFocus(e) {"
      "  console.log(window.name + '-focused-' + e.target.id);"
      "  domAutomationController.send(window.name + '-focused-' + e.target.id);"
      "}"
      "var input2 = document.createElement('input');"
      "input2.id = 'input2';"
      "document.body.insertBefore(input2, document.body.firstChild);"
      "input2.addEventListener('focus', onFocus, false);"
      "var input1 = document.createElement('input');"
      "input1.id = 'input1';"
      "document.body.insertBefore(input1, document.body.firstChild);"
      "input1.addEventListener('focus', onFocus, false);";

  // Add two input fields to the last fenced frame.
  EXPECT_TRUE(ExecJs(child3, script));

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message = [web_contents](bool reverse) {
    content::DOMMessageQueue msg_queue(web_contents);
    std::string reply;
    SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, reverse /* shift */, false, false);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    return reply;
  };

  // Press <tab> to move focus to the <input> element in child3 fenced frame.
  EXPECT_EQ("\"child3-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ("\"child3-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(child3, web_contents->GetFocusedFrame());

  // Press <tab> twice to focus on the UI and then wrap around back to the
  // child frame.
  SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, /*shift=*/true, false, false);

  EXPECT_EQ("\"child3-focused-input1\"", press_tab_and_wait_for_message(false));
}

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || BUILDFLAG(IS_WIN)
// Ensures that renderers know to advance focus to sibling frames and parent
// frames in the presence of mouse click initiated focus changes.
// Verifies against regression of https://crbug.com/702330
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       TabAndMouseFocusNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child1 = ChildFrameAt(main_frame, 0);
  ASSERT_NE(nullptr, child1);
  content::RenderFrameHost* child2 = ChildFrameAt(main_frame, 1);
  ASSERT_NE(nullptr, child2);

  // Needed to avoid flakiness with --enable-browser-side-navigation.
  content::WaitForHitTestData(child1);
  content::WaitForHitTestData(child2);

  // Assign a name to each frame.  This will be sent along in test messages
  // from focus events.
  EXPECT_TRUE(ExecJs(main_frame, "window.name = 'root';"));
  EXPECT_TRUE(ExecJs(child1, "window.name = 'child1';"));
  EXPECT_TRUE(ExecJs(child2, "window.name = 'child2';"));

  // This script will insert two <input> fields in the document, one at the
  // beginning and one at the end.  For root frame, this means that we will
  // have an <input>, then two <iframe> elements, then another <input>.
  // The script will send back the coordinates to click for each <input>, in the
  // document's space. Additionally, the outer frame will return  the top left
  // point of each <iframe> to transform the coordinates of the inner <input>
  // elements. For example, main frame: 497,18;497,185:381,59;499,59 and each
  // iframe: 55,18;55,67
  std::string script = R"(
      /* Invariant: eventsOrGets is either all event records, or all
       * `resolve` functions.
       */
      var eventsOrGets = [];
      function waitForFocusEvent() {
        if (eventsOrGets.length && (typeof eventsOrGets.at(-1) !=
           'function')) {
          return eventsOrGets.pop();
        }
        return new Promise(resolve => {
          eventsOrGets.push(resolve);
        });
      }
      function onFocus(e) {
        var result = window.name + '-focused-' + e.target.id;
        console.log(result);
        if (eventsOrGets.length && (typeof eventsOrGets.at(-1) ==
            'function')) {
          eventsOrGets.pop()(result);
          return;
        }
        eventsOrGets.push(result);
      }

      function getElementCoords(element) {
        var rect = element.getBoundingClientRect();
        return Math.floor(rect.left + 0.5 * rect.width) +','+
               Math.floor(rect.top + 0.5 * rect.height);
      }
      function getIframeCoords(element) {
        var rect = element.getBoundingClientRect();
        return Math.floor(rect.left) +','+
               Math.floor(rect.top);
      }
      function onClick(e) {
       console.log('Click event ' + window.name + ' at: ' + e.x + ', ' + e.y
                   + ' screen: ' + e.screenX + ', ' + e.screenY);
      }

      window.addEventListener('click', onClick);
      console.log(document.location.origin);
      document.styleSheets[0].insertRule('input {width:100%;margin:0;}', 1);
      document.styleSheets[0].insertRule('h2 {margin:0;}', 1);
      var input1 = document.createElement('input');
      input1.id = 'input1';
      input1.addEventListener('focus', onFocus, false);
      var input2 = document.createElement('input');
      input2.id = 'input2';
      input2.addEventListener('focus', onFocus, false);
      document.body.insertBefore(input1, document.body.firstChild);
      document.body.appendChild(input2);

      var frames = document.querySelectorAll('iframe');
      frames = Array.prototype.map.call(frames, getIframeCoords).join(';');
      var inputCoords = [input1, input2].map(getElementCoords).join(';');
      if (frames) {
        inputCoords = inputCoords + ':' + frames;
      }
      inputCoords;
      )";

  auto parse_points = [](const std::string& input, const gfx::Point& offset) {
    base::StringPairs pieces;
    base::SplitStringIntoKeyValuePairs(input, ',', ';', &pieces);
    std::vector<gfx::Point> points;
    for (const auto& piece : pieces) {
      int x, y;
      EXPECT_TRUE(base::StringToInt(piece.first, &x));
      EXPECT_TRUE(base::StringToInt(piece.second, &y));
      points.push_back(gfx::Point(x + offset.x(), y + offset.y()));
    }
    return points;
  };
  auto parse_points_and_offsets = [parse_points](const std::string& input) {
    auto pieces = base::SplitString(input, ":", base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_NONEMPTY);
    gfx::Point empty_offset;
    return make_pair(parse_points(pieces[0], empty_offset),
                     parse_points(pieces[1], empty_offset));
  };

  // Add two input fields to each of the three frames and retrieve click
  // coordinates.
  std::string result = EvalJs(main_frame, script).ExtractString();
  auto parsed = parse_points_and_offsets(result);
  auto main_frame_input_coords = parsed.first;
  auto iframe1_offset = parsed.second[0];
  auto iframe2_offset = parsed.second[1];

  result = EvalJs(child1, script).ExtractString();
  auto child1_input_coords = parse_points(result, iframe1_offset);
  result = EvalJs(child2, script).ExtractString();
  auto child2_input_coords = parse_points(result, iframe2_offset);

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message =
      [web_contents](content::RenderFrameHost* receiver, bool reverse) {
        SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                         ui::VKEY_TAB, false, reverse /* shift */, false,
                         false);
        LOG(INFO) << "Press tab";
        return EvalJs(receiver, "waitForFocusEvent()");
      };

  auto click_element_and_wait_for_message =
      [web_contents](content::RenderFrameHost* receiver,
                     const gfx::Point& point) {
        auto content_bounds = web_contents->GetContainerBounds();
        ui_controls::SendMouseMove(point.x() + content_bounds.x(),
                                   point.y() + content_bounds.y());
        ui_controls::SendMouseClick(ui_controls::LEFT);

        LOG(INFO) << "Click element";
        return EvalJs(receiver, "waitForFocusEvent()");
      };

  // Tab from child1 back to root.
  EXPECT_EQ("root-focused-input1", click_element_and_wait_for_message(
                                       main_frame, main_frame_input_coords[0]));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());
  auto frame_focused = std::make_unique<content::FrameFocusedObserver>(child1);
  EXPECT_EQ("child1-focused-input1",
            click_element_and_wait_for_message(child1, child1_input_coords[0]));
  frame_focused->Wait();
  frame_focused = std::make_unique<content::FrameFocusedObserver>(main_frame);
  EXPECT_EQ("root-focused-input1",
            press_tab_and_wait_for_message(main_frame, true));
  frame_focused->Wait();

  // Tab from child2 forward to root.
  EXPECT_EQ("root-focused-input2", click_element_and_wait_for_message(
                                       main_frame, main_frame_input_coords[1]));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());
  frame_focused = std::make_unique<content::FrameFocusedObserver>(child2);
  EXPECT_EQ("child2-focused-input2",
            click_element_and_wait_for_message(child2, child2_input_coords[1]));
  frame_focused->Wait();
  frame_focused = std::make_unique<content::FrameFocusedObserver>(main_frame);
  EXPECT_EQ("root-focused-input2",
            press_tab_and_wait_for_message(main_frame, false));
  frame_focused->Wait();

  // Tab forward from child1 to child2.
  frame_focused = std::make_unique<content::FrameFocusedObserver>(child2);
  EXPECT_EQ("child2-focused-input1",
            click_element_and_wait_for_message(child2, child2_input_coords[0]));
  frame_focused->Wait();
  frame_focused = std::make_unique<content::FrameFocusedObserver>(child1);
  EXPECT_EQ("child1-focused-input2",
            click_element_and_wait_for_message(child1, child1_input_coords[1]));
  frame_focused->Wait();
  frame_focused = std::make_unique<content::FrameFocusedObserver>(child2);
  EXPECT_EQ("child2-focused-input1",
            press_tab_and_wait_for_message(child2, false));
  frame_focused->Wait();

  // Tab backward from child2 to child1.
  frame_focused = std::make_unique<content::FrameFocusedObserver>(child1);
  EXPECT_EQ("child1-focused-input2",
            click_element_and_wait_for_message(child1, child1_input_coords[1]));
  frame_focused->Wait();
  frame_focused = std::make_unique<content::FrameFocusedObserver>(child2);
  EXPECT_EQ("child2-focused-input1",
            click_element_and_wait_for_message(child2, child2_input_coords[0]));
  frame_focused->Wait();
  frame_focused = std::make_unique<content::FrameFocusedObserver>(child1);
  EXPECT_EQ("child1-focused-input2",
            press_tab_and_wait_for_message(child1, true));
  // EXPECT_EQ(child1, web_contents->GetFocusedFrame());
  frame_focused->Wait();

  // Ensure there are no pending focus events after tabbing.
  EXPECT_EQ("root-focused-input1", click_element_and_wait_for_message(
                                       main_frame, main_frame_input_coords[0]))
      << "Unexpected extra focus events.";
}
#endif

namespace {

// Helper to retrieve the frame's (window.innerWidth, window.innerHeight).
gfx::Size GetFrameSize(content::RenderFrameHost* frame) {
  int width = EvalJs(frame, "window.innerWidth;").ExtractInt();

  int height = EvalJs(frame, "window.innerHeight;").ExtractInt();

  return gfx::Size(width, height);
}

// Helper to check |frame|'s document.webkitFullscreenElement and return its ID
// if it's defined (which is the case when |frame| is in fullscreen mode), or
// "none" otherwise.
std::string GetFullscreenElementId(content::RenderFrameHost* frame) {
  return EvalJs(frame,
                "document.webkitFullscreenElement ? "
                "    document.webkitFullscreenElement.id : 'none'")
      .ExtractString();
}

// Helper to check if an element with ID |element_id| has the
// :-webkit-full-screen style.
bool ElementHasFullscreenStyle(content::RenderFrameHost* frame,
                               const std::string& element_id) {
  std::string script = base::StringPrintf(
      "document.querySelectorAll('#%s:-webkit-full-screen').length == 1",
      element_id.c_str());
  return EvalJs(frame, script).ExtractBool();
}

// Helper to check if an element with ID |element_id| has the
// :-webkit-full-screen-ancestor style.
bool ElementHasFullscreenAncestorStyle(content::RenderFrameHost* host,
                                       const std::string& element_id) {
  std::string script = base::StringPrintf(
      "document.querySelectorAll("
      "    '#%s:-webkit-full-screen-ancestor').length == 1",
      element_id.c_str());
  return EvalJs(host, script).ExtractBool();
}

// Add a listener that will send back a message whenever the (prefixed)
// fullscreenchange event fires.  The message will be "fullscreenchange",
// followed by a space and the provided |id|.
void AddFullscreenChangeListener(content::RenderFrameHost* frame,
                                 const std::string& id) {
  std::string script = base::StringPrintf(
      "document.addEventListener('webkitfullscreenchange', function() {"
      "    domAutomationController.send('fullscreenchange %s');});",
      id.c_str());
  EXPECT_TRUE(ExecJs(frame, script));
}

// Helper to add a listener that will send back a "resize" message when the
// target |frame| is resized to |expected_size|.
void AddResizeListener(content::RenderFrameHost* frame,
                       const gfx::Size& expected_size) {
  std::string script =
      base::StringPrintf("addResizeListener(%d, %d);", expected_size.width(),
                         expected_size.height());
  EXPECT_TRUE(ExecJs(frame, script));
}

// Helper to wait for a toggle fullscreen operation to complete in all affected
// frames.  This means waiting for:
// 1. All fullscreenchange events with id's matching the list in
//    |expected_fullscreen_event_ids|.  Typically the list will correspond to
//    events from the actual fullscreen element and all of its ancestor
//    <iframe> elements.
// 2. A resize event.  This will verify that the frame containing the
//    fullscreen element is properly resized.  This assumes that the expected
//    size is already registered via AddResizeListener().
void WaitForMultipleFullscreenEvents(
    const std::set<std::string>& expected_fullscreen_event_ids,
    content::DOMMessageQueue& queue) {
  std::set<std::string> remaining_events(expected_fullscreen_event_ids);
  bool resize_validated = false;
  std::string response;
  while (queue.WaitForMessage(&response)) {
    base::TrimString(response, "\"", &response);
    std::vector<std::string> response_params = base::SplitString(
        response, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (response_params[0] == "fullscreenchange") {
      EXPECT_TRUE(base::Contains(remaining_events, response_params[1]));
      remaining_events.erase(response_params[1]);
    } else if (response_params[0] == "resize") {
      resize_validated = true;
    }
    if (remaining_events.empty() && resize_validated) {
      break;
    }
  }
}

}  // namespace

// Check that an element in a cross-process subframe can enter and exit
// fullscreen.  The test will verify that:
// - the subframe is properly resized
// - the WebContents properly enters/exits fullscreen.
// - document.webkitFullscreenElement is correctly updated in both frames.
// - fullscreenchange events fire in both frames.
// - fullscreen CSS is applied correctly in both frames.
//
#if BUILDFLAG(IS_MAC)
// https://crbug.com/850594
#define MAYBE_FullscreenElementInSubframe DISABLED_FullscreenElementInSubframe
#else
#define MAYBE_FullscreenElementInSubframe FullscreenElementInSubframe
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       MAYBE_FullscreenElementInSubframe) {
  // Start on a page with one subframe (id "child-0") that has
  // "allowfullscreen" enabled.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_allowfullscreen_frame.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate the subframe cross-site to a page with a fullscreenable <div>.
  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/fullscreen_frame.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "child-0", frame_url));

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child = ChildFrameAt(main_frame, 0);
  gfx::Size original_child_size = GetFrameSize(child);

  // Fullscreen the <div> inside the cross-site child frame.  Wait until:
  // (1) the fullscreenchange events in main frame and child send a response,
  // (2) the child frame is resized to fill the whole screen.
  // (3) the browser has finished the fullscreen transition.
  AddFullscreenChangeListener(main_frame, "main_frame");
  AddFullscreenChangeListener(child, "child");
  std::set<std::string> expected_events = {"main_frame", "child"};
  AddResizeListener(child, GetScreenSize());
  {
    content::DOMMessageQueue queue(web_contents);
    ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
    EXPECT_TRUE(ExecJs(child, "activateFullscreen()"));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    waiter.Wait();
  }

  // Verify that the browser has entered fullscreen for the current tab.
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(web_contents->IsFullscreen());

  // Verify that the <div> has fullscreen style (:-webkit-full-screen) in the
  // subframe.
  EXPECT_TRUE(ElementHasFullscreenStyle(child, "fullscreen-div"));

  // Verify that the main frame has applied proper fullscreen styles to the
  // <iframe> element (:-webkit-full-screen and :-webkit-full-screen-ancestor).
  // This is what causes the <iframe> to stretch and fill the whole viewport.
  EXPECT_TRUE(ElementHasFullscreenStyle(main_frame, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenAncestorStyle(main_frame, "child-0"));

  // Check document.webkitFullscreenElement.  For main frame, it should point
  // to the subframe, and for subframe, it should point to the fullscreened
  // <div>.
  EXPECT_EQ("child-0", GetFullscreenElementId(main_frame));
  EXPECT_EQ("fullscreen-div", GetFullscreenElementId(child));

  // Now exit fullscreen from the subframe.  Wait for two fullscreenchange
  // events from both frames, and also for the child to be resized to its
  // original size.
  AddResizeListener(child, original_child_size);
  {
    content::DOMMessageQueue queue(web_contents);
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.tab_fullscreen = false});
    EXPECT_TRUE(ExecJs(child, "exitFullscreen()"));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    waiter.Wait();
  }

  EXPECT_FALSE(browser()->window()->IsFullscreen());

  // Verify that the fullscreen styles were removed from the <div> and its
  // container <iframe>.
  EXPECT_FALSE(ElementHasFullscreenStyle(child, "fullscreen-div"));
  EXPECT_FALSE(ElementHasFullscreenStyle(main_frame, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenAncestorStyle(main_frame, "child-0"));

  // Check that both frames cleared their document.webkitFullscreenElement.
  EXPECT_EQ("none", GetFullscreenElementId(main_frame));
  EXPECT_EQ("none", GetFullscreenElementId(child));
}

// Check that on a page with A-embed-B-embed-A frame hierarchy, an element in
// the bottom frame can enter and exit fullscreen.  |exit_method| specifies
// whether to use browser-initiated vs. renderer-initiated fullscreen exit
// (i.e., pressing escape vs. a JS call), since they trigger different code
// paths on the Blink side.
void SitePerProcessInteractiveBrowserTest::FullscreenElementInABA(
    FullscreenExitMethod exit_method) {
  GURL main_url(embedded_test_server()->GetURL("a.com",
                                               "/cross_site_iframe_factory."
                                               "html?a(b{allowfullscreen}(a{"
                                               "allowfullscreen}))"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child = ChildFrameAt(main_frame, 0);
  content::RenderFrameHost* grandchild = ChildFrameAt(child, 0);

  // Navigate the bottom frame to a page that has a fullscreenable <div>.
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(ExecJs(grandchild, "location.href = '/fullscreen_frame.html'"));
  observer.Wait();
  grandchild = ChildFrameAt(child, 0);
  EXPECT_EQ(embedded_test_server()->GetURL("a.com", "/fullscreen_frame.html"),
            grandchild->GetLastCommittedURL());

  // Make fullscreenchange events in all three frames send a message.
  AddFullscreenChangeListener(main_frame, "main_frame");
  AddFullscreenChangeListener(child, "child");
  AddFullscreenChangeListener(grandchild, "grandchild");

  // Add a resize event handler that will send a message when the grandchild
  // frame is resized to the screen size.  Also save its original size.
  AddResizeListener(grandchild, GetScreenSize());
  gfx::Size original_grandchild_size = GetFrameSize(grandchild);

  // Fullscreen a <div> inside the bottom subframe.  This will block until
  // (1) the fullscreenchange events in all frames send a response, and
  // (2) the frame is resized to fill the whole screen.
  // (3) the browser has finished the fullscreen transition.
  std::set<std::string> expected_events = {"main_frame", "child", "grandchild"};
  {
    content::DOMMessageQueue queue(web_contents);
    ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
    EXPECT_TRUE(ExecJs(grandchild, "activateFullscreen()"));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    waiter.Wait();
  }

  // Verify that the browser has entered fullscreen for the current tab.
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(web_contents->IsFullscreen());

  // Verify that the <div> has fullscreen style in the bottom frame, and that
  // the proper <iframe> elements have fullscreen style in its ancestor frames.
  EXPECT_TRUE(ElementHasFullscreenStyle(grandchild, "fullscreen-div"));
  EXPECT_TRUE(ElementHasFullscreenStyle(child, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenAncestorStyle(child, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenStyle(main_frame, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenAncestorStyle(main_frame, "child-0"));

  // Check document.webkitFullscreenElement in all frames.
  EXPECT_EQ("child-0", GetFullscreenElementId(main_frame));
  EXPECT_EQ("child-0", GetFullscreenElementId(child));
  EXPECT_EQ("fullscreen-div", GetFullscreenElementId(grandchild));

  // Now exit fullscreen from the subframe.
  AddResizeListener(grandchild, original_grandchild_size);
  {
    content::DOMMessageQueue queue(web_contents);
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.tab_fullscreen = false});
    switch (exit_method) {
      case FullscreenExitMethod::JS_CALL:
        EXPECT_TRUE(ExecJs(grandchild, "exitFullscreen()"));
        break;
      case FullscreenExitMethod::ESC_PRESS:
        ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
            browser(), ui::VKEY_ESCAPE, false, false, false, false));
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    WaitForMultipleFullscreenEvents(expected_events, queue);
    waiter.Wait();
  }

  EXPECT_FALSE(browser()->window()->IsFullscreen());

  // Verify that the fullscreen styles were removed from the <div> and its
  // container <iframe>'s.
  EXPECT_FALSE(ElementHasFullscreenStyle(grandchild, "fullscreen-div"));
  EXPECT_FALSE(ElementHasFullscreenStyle(child, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenAncestorStyle(child, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(main_frame, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenAncestorStyle(main_frame, "child-0"));

  // Check that document.webkitFullscreenElement was cleared in all three
  // frames.
  EXPECT_EQ("none", GetFullscreenElementId(main_frame));
  EXPECT_EQ("none", GetFullscreenElementId(child));
  EXPECT_EQ("none", GetFullscreenElementId(grandchild));
}

// https://crbug.com/1087392: Flaky for ASAN and TSAN
#if BUILDFLAG(IS_MAC) || defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
#define MAYBE_FullscreenElementInABAAndExitViaEscapeKey \
  DISABLED_FullscreenElementInABAAndExitViaEscapeKey
#else
#define MAYBE_FullscreenElementInABAAndExitViaEscapeKey \
  FullscreenElementInABAAndExitViaEscapeKey
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       MAYBE_FullscreenElementInABAAndExitViaEscapeKey) {
  FullscreenElementInABA(FullscreenExitMethod::ESC_PRESS);
}

// This test is flaky on Linux (crbug.com/851236) and also not working
// on Mac (crbug.com/850594).
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       DISABLED_FullscreenElementInABAAndExitViaJS) {
  FullscreenElementInABA(FullscreenExitMethod::JS_CALL);
}

// Check that fullscreen works on a more complex page hierarchy with multiple
// local and remote ancestors.  The test uses this frame tree:
//
//             A (a_top)
//             |
//             A (a_bottom)
//            / \   .
// (b_first) B   B (b_second)
//               |
//               C (c_top)
//               |
//               C (c_middle) <- fullscreen target
//               |
//               C (c_bottom)
//
// The c_middle frame will trigger fullscreen for its <div> element.  The test
// verifies that its ancestor chain is properly updated for fullscreen, and
// that the b_first node that's not on the chain is not affected.
//
// The test also exits fullscreen by simulating pressing ESC rather than using
// document.webkitExitFullscreen(), which tests the browser-initiated
// fullscreen exit path.
// TODO(crbug.com/40535621): flaky on all platforms.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       DISABLED_FullscreenElementInMultipleSubframes) {
  // Allow fullscreen in all iframes descending to |c_middle|.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(a{allowfullscreen}(b,b{"
      "allowfullscreen}(c{allowfullscreen}(c{allowfullscreen}))))"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* a_top = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* a_bottom = ChildFrameAt(a_top, 0);
  content::RenderFrameHost* b_first = ChildFrameAt(a_bottom, 0);
  content::RenderFrameHost* b_second = ChildFrameAt(a_bottom, 1);
  content::RenderFrameHost* c_top = ChildFrameAt(b_second, 0);
  content::RenderFrameHost* c_middle = ChildFrameAt(c_top, 0);

  // Navigate |c_middle| to a page that has a fullscreenable <div> and another
  // frame.
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(ExecJs(c_middle, "location.href = '/fullscreen_frame.html'"));
  observer.Wait();
  c_middle = ChildFrameAt(c_top, 0);
  EXPECT_EQ(embedded_test_server()->GetURL("c.com", "/fullscreen_frame.html"),
            c_middle->GetLastCommittedURL());
  content::RenderFrameHost* c_bottom = ChildFrameAt(c_middle, 0);

  // Save the size of the frame to be fullscreened.
  gfx::Size c_middle_original_size = GetFrameSize(c_middle);

  // Add fullscreenchange and resize event handlers to all frames.
  AddFullscreenChangeListener(a_top, "a_top");
  AddFullscreenChangeListener(a_bottom, "a_bottom");
  AddFullscreenChangeListener(b_first, "b_first");
  AddFullscreenChangeListener(b_second, "b_second");
  AddFullscreenChangeListener(c_top, "c_top");
  AddFullscreenChangeListener(c_middle, "c_middle");
  AddFullscreenChangeListener(c_bottom, "c_bottom");
  AddResizeListener(c_middle, GetScreenSize());

  // Note that expected fullscreenchange events do NOT include |b_first| and
  // |c_bottom|, which aren't on the ancestor chain of |c_middle|.
  // WaitForMultipleFullscreenEvents() below will fail if it hears an
  // unexpected fullscreenchange from one of these frames.
  std::set<std::string> expected_events = {"a_top", "a_bottom", "b_second",
                                           "c_top", "c_middle"};

  // Fullscreen a <div> inside |c_middle|.  Block until (1) the
  // fullscreenchange events in |c_middle| and all its ancestors send a
  // response, (2) |c_middle| is resized to fill the whole screen, and (3) the
  // browser finishes the fullscreen transition.
  {
    content::DOMMessageQueue queue(web_contents);
    ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
    EXPECT_TRUE(ExecJs(c_middle, "activateFullscreen()"));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    waiter.Wait();
  }

  // Verify that the browser has entered fullscreen for the current tab.
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(web_contents->IsFullscreen());

  // Check document.webkitFullscreenElement.  It should point to corresponding
  // <iframe> element IDs on |c_middle|'s ancestor chain, and it should be null
  // in b_first and c_bottom.
  EXPECT_EQ("child-0", GetFullscreenElementId(a_top));
  EXPECT_EQ("child-1", GetFullscreenElementId(a_bottom));
  EXPECT_EQ("child-0", GetFullscreenElementId(b_second));
  EXPECT_EQ("child-0", GetFullscreenElementId(c_top));
  EXPECT_EQ("fullscreen-div", GetFullscreenElementId(c_middle));
  EXPECT_EQ("none", GetFullscreenElementId(b_first));
  EXPECT_EQ("none", GetFullscreenElementId(c_bottom));

  // Verify that the fullscreen element and all <iframe> elements on its
  // ancestor chain have fullscreen style, but other frames do not.
  EXPECT_TRUE(ElementHasFullscreenStyle(a_top, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(a_bottom, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenStyle(a_bottom, "child-1"));
  EXPECT_TRUE(ElementHasFullscreenStyle(b_second, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenStyle(c_top, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenStyle(c_middle, "fullscreen-div"));
  EXPECT_FALSE(ElementHasFullscreenStyle(c_middle, "child-0"));

  // Now exit fullscreen by pressing escape.  Wait for all fullscreenchange
  // events fired for fullscreen exit and verify that the bottom frame was
  // resized back to its original size.
  AddResizeListener(c_middle, c_middle_original_size);
  {
    content::DOMMessageQueue queue(web_contents);
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.tab_fullscreen = false});
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE,
                                                false, false, false, false));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    waiter.Wait();
  }

  EXPECT_FALSE(browser()->window()->IsFullscreen());

  // Check that document.webkitFullscreenElement has been cleared in all
  // frames.
  EXPECT_EQ("none", GetFullscreenElementId(a_top));
  EXPECT_EQ("none", GetFullscreenElementId(a_bottom));
  EXPECT_EQ("none", GetFullscreenElementId(b_first));
  EXPECT_EQ("none", GetFullscreenElementId(b_second));
  EXPECT_EQ("none", GetFullscreenElementId(c_top));
  EXPECT_EQ("none", GetFullscreenElementId(c_middle));
  EXPECT_EQ("none", GetFullscreenElementId(c_bottom));

  // Verify that all fullscreen styles have been cleared.
  EXPECT_FALSE(ElementHasFullscreenStyle(a_top, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(a_bottom, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(a_bottom, "child-1"));
  EXPECT_FALSE(ElementHasFullscreenStyle(b_second, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(c_top, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(c_middle, "fullscreen-div"));
  EXPECT_FALSE(ElementHasFullscreenStyle(c_middle, "child-0"));
}

// Test that deleting a RenderWidgetHost that holds the mouse lock won't cause a
// crash. https://crbug.com/619571.

// Flaky on multiple builders. https://crbug.com/1059632
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       DISABLED_RenderWidgetHostDeletedWhileMouseLocked) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child = ChildFrameAt(main_frame, 0);

  EXPECT_TRUE(ExecJs(child, "document.body.requestPointerLock()",
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  EXPECT_EQ(true,
            EvalJs(child, "document.body == document.pointerLockElement"));
  EXPECT_TRUE(main_frame->GetView()->IsPointerLocked());

  EXPECT_TRUE(ExecJs(main_frame,
                     "document.querySelector('iframe').parentNode."
                     "removeChild(document.querySelector('iframe'))"));
  EXPECT_FALSE(main_frame->GetView()->IsPointerLocked());
}

#if BUILDFLAG(ENABLE_PDF)
// Base test class for interactive tests which load and test PDF files.
class SitePerProcessInteractivePDFTest
    : public base::test::WithFeatureOverride,
      public SitePerProcessInteractiveBrowserTest {
 public:
  SitePerProcessInteractivePDFTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  SitePerProcessInteractivePDFTest(const SitePerProcessInteractivePDFTest&) =
      delete;
  SitePerProcessInteractivePDFTest& operator=(
      const SitePerProcessInteractivePDFTest&) = delete;

  ~SitePerProcessInteractivePDFTest() override {}

  void SetUpOnMainThread() override {
    SitePerProcessInteractiveBrowserTest::SetUpOnMainThread();
    if (UseOopif()) {
      factory_ = std::make_unique<pdf::TestPdfViewerStreamManagerFactory>();
    } else {
      auto factory =
          std::make_unique<guest_view::TestGuestViewManagerFactory>();
      test_guest_view_manager_ = factory->GetOrCreateTestGuestViewManager(
          browser()->profile(), extensions::ExtensionsAPIClient::Get()
                                    ->CreateGuestViewManagerDelegate());
      factory_ = std::move(factory);
    }
  }

  void TearDownOnMainThread() override {
    test_guest_view_manager_ = nullptr;
    factory_ = absl::monostate();
    SitePerProcessInteractiveBrowserTest::TearDownOnMainThread();
  }

  bool UseOopif() const { return GetParam(); }

 protected:
  guest_view::TestGuestViewManager* GetTestGuestViewManager() const {
    return test_guest_view_manager_;
  }

  pdf::TestPdfViewerStreamManager* GetTestPdfViewerStreamManager() const {
    return absl::get<std::unique_ptr<pdf::TestPdfViewerStreamManagerFactory>>(
               factory_)
        ->GetTestPdfViewerStreamManager(
            browser()->tab_strip_model()->GetActiveWebContents());
  }

  void CreateTestPdfViewerStreamManager() const {
    absl::get<std::unique_ptr<pdf::TestPdfViewerStreamManagerFactory>>(factory_)
        ->CreatePdfViewerStreamManager(
            browser()->tab_strip_model()->GetActiveWebContents());
  }

  void WaitUntilPdfLoaded(content::RenderFrameHost* embedder_host) {
    if (UseOopif()) {
      ASSERT_TRUE(
          GetTestPdfViewerStreamManager()->WaitUntilPdfLoaded(embedder_host));
    } else {
      auto* guest_view =
          GetTestGuestViewManager()->WaitForSingleGuestViewCreated();
      ASSERT_TRUE(guest_view);
      auto* embedder_web_contents =
          browser()->tab_strip_model()->GetActiveWebContents();
      EXPECT_NE(embedder_web_contents->GetPrimaryMainFrame(),
                guest_view->GetGuestMainFrame());

      extensions::TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(
          guest_view);
    }
  }

 private:
  absl::variant<absl::monostate,
                std::unique_ptr<guest_view::TestGuestViewManagerFactory>,
                std::unique_ptr<pdf::TestPdfViewerStreamManagerFactory>>
      factory_;
  raw_ptr<guest_view::TestGuestViewManager> test_guest_view_manager_;
};

// This test loads a PDF inside an OOPIF and then verifies that context menu
// shows up at the correct position.
// TODO(crbug.com/1423184, crbug.com/327338993): Fix flaky test.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER))
#define MAYBE_ContextMenuPositionForEmbeddedPDFInCrossOriginFrame \
  DISABLED_ContextMenuPositionForEmbeddedPDFInCrossOriginFrame
#else
#define MAYBE_ContextMenuPositionForEmbeddedPDFInCrossOriginFrame \
  ContextMenuPositionForEmbeddedPDFInCrossOriginFrame
#endif  // BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) &&
        // defined(ADDRESS_SANITIZER))
IN_PROC_BROWSER_TEST_P(
    SitePerProcessInteractivePDFTest,
    MAYBE_ContextMenuPositionForEmbeddedPDFInCrossOriginFrame) {
  // Navigate to a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  if (!UseOopif()) {
    // Initially, no guests are created.
    EXPECT_EQ(0U, GetTestGuestViewManager()->num_guests_created());
  }

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Change the position of the <iframe> inside the page.
  EXPECT_TRUE(ExecJs(active_web_contents,
                     "document.querySelector('iframe').style ="
                     " 'margin-left: 100px; margin-top: 100px;';"));

  // Navigate subframe to a cross-site page with an embedded PDF.
  GURL frame_url =
      embedded_test_server()->GetURL("b.com", "/page_with_embedded_pdf.html");

  // Ensure the page finishes loading without crashing.
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));
  content::RenderFrameHost* iframe_host =
      ChildFrameAt(active_web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe_host);
  content::RenderFrameHost* embedder_host = ChildFrameAt(iframe_host, 0);
  ASSERT_TRUE(embedder_host);
  WaitUntilPdfLoaded(embedder_host);

  content::RenderWidgetHostView* child_view = iframe_host->GetView();

  ContextMenuWaiter menu_waiter;

  // Declaring a lambda to send a right-button mouse event to the embedder
  // frame.
  auto send_right_mouse_event = [](content::RenderWidgetHost* host, int x,
                                   int y, blink::WebInputEvent::Type type) {
    blink::WebMouseEvent event;
    event.SetTimeStamp(blink::WebInputEvent::GetStaticTimeStampForTests());
    event.SetPositionInWidget(x, y);
    event.button = blink::WebMouseEvent::Button::kRight;
    event.SetType(type);
    host->ForwardMouseEvent(event);
  };

  send_right_mouse_event(child_view->GetRenderWidgetHost(), 10, 20,
                         blink::WebInputEvent::Type::kMouseDown);
  send_right_mouse_event(child_view->GetRenderWidgetHost(), 10, 20,
                         blink::WebInputEvent::Type::kMouseUp);
  menu_waiter.WaitForMenuOpenAndClose();

  gfx::Point point_in_root_window =
      child_view->TransformPointToRootCoordSpace(gfx::Point(10, 20));

  EXPECT_EQ(point_in_root_window.x(), menu_waiter.params().x);
  EXPECT_EQ(point_in_root_window.y(), menu_waiter.params().y);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessInteractivePDFTest,
                       LoadingPdfDoesNotStealFocus) {
  // Load test HTML, and verify the text area has focus.
  GURL main_url(embedded_test_server()->GetURL("/pdf/two_iframes.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  auto* embedder_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Make sure we can see the iframe's document.
  ASSERT_TRUE(
      content::EvalJs(embedder_web_contents,
                      "new Promise((resolve) => {"
                      "  var iframe1 = document.getElementById('iframe1');"
                      "  var iframe1doc = iframe1.contentDocument;"
                      "  resolve(iframe1doc != null);"
                      "});")
          .ExtractBool());

  // Make sure the text area is focused. First, we must explicitly focus the
  // child iframe containing the text area.
  content::RenderFrameHost* main_frame =
      embedder_web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_text_area = ChildFrameAt(main_frame, 0);
  content::RenderFrameHost* iframe_pdf = ChildFrameAt(main_frame, 1);
  ASSERT_TRUE(iframe_pdf);
  ASSERT_TRUE(content::ExecJs(child_text_area, "window.focus();"));
  bool starts_focused =
      content::EvalJs(
          embedder_web_contents,
          "new Promise((resolve) => {"
          "  iframe1doc = "
          "      document.getElementById('iframe1').contentDocument;"
          "  function timeoutFcn(n) {"
          "    if (n == 0 || iframe1doc.hasFocus()) {"
          "      resolve(iframe1doc.hasFocus());"
          "      return;"
          "    }"
          "    window.console.log('Recursing: n = ' + n);"
          "    setTimeout(() => { timeoutFcn(n-1); }, 1000);"
          "  };"
          "  timeoutFcn(5);"
          "});")
          .ExtractBool();
  if (!starts_focused) {
    LOG(ERROR) << "Embedder focused frame = "
               << embedder_web_contents->GetFocusedFrame()
               << ", main frame = " << main_frame
               << ", embedder_contents_focused = "
               << IsRenderWidgetHostFocused(
                      embedder_web_contents->GetRenderWidgetHostView()
                          ->GetRenderWidgetHost())
               << ", iframe_text = " << child_text_area
               << ", iframe_pdf = " << iframe_pdf;
  }
  ASSERT_TRUE(starts_focused);

  if (UseOopif()) {
    // Create the manager first, since the following script doesn't block until
    // navigation is complete.
    CreateTestPdfViewerStreamManager();
  }

  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(content::ExecJs(
      embedder_web_contents,
      content::JsReplace("document.getElementById('iframe2').src = $1;",
                         pdf_url.spec())));

  WaitUntilPdfLoaded(iframe_pdf);

  // Make sure the text area still has focus.
  ASSERT_TRUE(
      content::EvalJs(
          embedder_web_contents,
          "new Promise((resolve) => {"
          "  iframe1doc = "
          "      document.getElementById('iframe1').contentDocument;"
          "  text_area = iframe1doc.getElementById('text_area');"
          "  text_area_is_active = iframe1doc.activeElement == text_area;"
          "  resolve(iframe1doc.hasFocus() && text_area_is_active);"
          "});")
          .ExtractBool());
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SitePerProcessInteractivePDFTest);
#endif  // BUILDFLAG(ENABLE_PDF)

class SitePerProcessAutofillTest : public SitePerProcessInteractiveBrowserTest {
 public:
  SitePerProcessAutofillTest() : SitePerProcessInteractiveBrowserTest() {}

  SitePerProcessAutofillTest(const SitePerProcessAutofillTest&) = delete;
  SitePerProcessAutofillTest& operator=(const SitePerProcessAutofillTest&) =
      delete;

  ~SitePerProcessAutofillTest() override = default;

  void SetupMainTab() {
    // Add a fresh new WebContents for which we add our own version of the
    // ChromePasswordManagerClient that uses a custom TestAutofillClient.
    std::unique_ptr<content::WebContents> new_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetBrowserContext()));
    ASSERT_TRUE(new_contents);
    ASSERT_FALSE(
        ChromePasswordManagerClient::FromWebContents(new_contents.get()));

    // Create ChromePasswordManagerClient and verify it exists for the new
    // WebContents.
    ChromePasswordManagerClient::CreateForWebContents(new_contents.get());
    ASSERT_TRUE(
        ChromePasswordManagerClient::FromWebContents(new_contents.get()));

    browser()->tab_strip_model()->AppendWebContents(std::move(new_contents),
                                                    true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Waits until transforming |sample_point| from |render_frame_host| coordinates
// to its root frame's view's coordinates matches |transformed_point| within a
// reasonable error margin less than or equal to |bound|. This method is used to
// verify CSS changes on OOPIFs have been applied properly and the corresponding
// compositor frame is updated as well. This way we can rest assured that the
// future transformed and reported bounds for the elements inside
// |render_frame_host| are correct.
void WaitForFramePositionUpdated(content::RenderFrameHost* render_frame_host,
                                 const gfx::Point& sample_point,
                                 const gfx::Point& transformed_point,
                                 float bound) {
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return (transformed_point -
            render_frame_host->GetView()->TransformPointToRootCoordSpace(
                sample_point))
               .Length() <= bound;
  }));
}

// This test verifies that when clicking outside the bounds of a date picker
// associated with an <input> inside an OOPIF, the RenderWidgetHostImpl
// corresponding to the WebPagePopup is destroyed (see
// https://crbug.com/671732).
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       ShowAndHideDatePopupInOOPIFMultipleTimes) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::RenderFrameHost* child_frame =
      ChildFrameAt(browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetPrimaryMainFrame(),
                   0);

  // Add <input type='date'> to the child frame. Adjust the positions that we
  // know where to click to dismiss the popup.
  ASSERT_TRUE(ExecJs(
      child_frame,
      "var input = document.createElement('input');"
      "input.type = 'date';"
      "input.value = '2008-09-02';"
      "document.body.appendChild(input);"
      "input.style = 'position: fixed; left: 0px; top: 10px; border: none;' +"
      "              'width: 120px; height: 20px;';"));

  // Cache current date value for a sanity check later.
  std::string cached_date = EvalJs(child_frame, "input.value;").ExtractString();

  // We use this to determine whether a new RenderWidgetHost is created or an
  // old one is removed.
  size_t default_widget_count = GetNumberOfRenderWidgetHosts();

  // Repeatedly invoke the date picker and then click outside the bounds of the
  // widget to dismiss it and each time verify that a new RenderWidgetHost is
  // added when showing the date picker and a RenderWidgetHost is destroyed when
  // it is dismissed.
  for (size_t tries = 0; tries < 3U; tries++) {
    // Focus the <input>.
    ASSERT_TRUE(
        ExecJs(child_frame, "document.querySelector('input').focus();"));

    // Alt + Down seems to be working fine on all platforms.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                                false, true, false));

    // We should get one more widget on the screen.
    WaitForRenderWidgetHostCount(default_widget_count + 1U);

    content::RenderWidgetHost* child_widget_host =
        child_frame->GetView()->GetRenderWidgetHost();

    // Now simulate a click outside the bounds of the popup.
    blink::WebMouseEvent event;
    event.SetTimeStamp(blink::WebInputEvent::GetStaticTimeStampForTests());
    // Click a little bit to the right and top of the <input>.
    event.SetPositionInWidget(130, 10);
    event.button = blink::WebMouseEvent::Button::kLeft;

    // Send a mouse down event.
    event.SetType(blink::WebInputEvent::Type::kMouseDown);
    child_widget_host->ForwardMouseEvent(event);

    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();

    // Now send a mouse up event.
    event.SetType(blink::WebMouseEvent::Type::kMouseUp);
    child_widget_host->ForwardMouseEvent(event);

    // Wait until the popup disappears and we go back to the normal
    // RenderWidgetHost count.
    WaitForRenderWidgetHostCount(default_widget_count);
  }

  // To make sure we never clicked into the date picker, get current date value
  // and make sure it matches the cached value.
  std::string date = EvalJs(child_frame, "input.value;").ExtractString();
  EXPECT_EQ(cached_date, date) << "Cached date was '" << cached_date
                               << "' but current date is '" << date << "'.";
}

// There is a problem of missing keyup events with the command key after
// the NSEvent is sent to NSApplication in ui/base/test/ui_controls_mac.mm .
// This test is disabled on only the Mac until the problem is resolved.
// See http://crbug.com/425859 for more information.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SubframeAnchorOpenedInBackgroundTab \
  DISABLED_SubframeAnchorOpenedInBackgroundTab
#else
#define MAYBE_SubframeAnchorOpenedInBackgroundTab \
  SubframeAnchorOpenedInBackgroundTab
#endif
// Tests that ctrl-click in a subframe results in a background, not a foreground
// tab - see https://crbug.com/804838.  This test is somewhat similar to
// CtrlClickShouldEndUpIn*ProcessTest tests, but this test has to simulate an
// actual mouse click.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       MAYBE_SubframeAnchorOpenedInBackgroundTab) {
  // Setup the test page - the ctrl-clicked link should be in a subframe.
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  GURL subframe_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/anchor_to_same_site_location.html"));
  content::WebContents* old_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(NavigateIframeToURL(old_contents, "test", subframe_url));
  content::RenderFrameHost* subframe = ChildFrameAt(old_contents, 0);
  ASSERT_TRUE(subframe);
  EXPECT_EQ(subframe_url, subframe->GetLastCommittedURL());

  // Simulate the ctrl-return to open the anchor's link in a new background tab.
  EXPECT_TRUE(ExecJs(
      subframe, "document.getElementById('test-anchor-no-target').focus();"));
  content::WebContents* new_contents = nullptr;
  {
    content::WebContentsAddedObserver new_tab_observer;
#if BUILDFLAG(IS_MAC)
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        old_contents->GetTopLevelNativeWindow(), ui::VKEY_RETURN, false, false,
        false, true /* cmd */));
#else
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        old_contents->GetTopLevelNativeWindow(), ui::VKEY_RETURN,
        true /* ctrl */, false, false, false));
#endif
    new_contents = new_tab_observer.GetWebContents();
  }

  // Verify that the new content has loaded the expected contents.
  GURL target_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  EXPECT_EQ(target_url,
            new_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the anchor opened in a new background tab.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(0,
            browser()->tab_strip_model()->GetIndexOfWebContents(old_contents));
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetIndexOfWebContents(new_contents));
}

// Check that window.focus works for cross-process popups.
// Flaky on ChromeOS debug and ASAN builds. https://crbug.com/1326293
// Flaky on Linux https://crbug.com/1336109.
// Flaky on Win https://crbug.com/1337725.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || \
    (BUILDFLAG(IS_CHROMEOS) &&                  \
     (!defined(NDEBUG) || defined(ADDRESS_SANITIZER)))
#define MAYBE_PopupWindowFocus DISABLED_PopupWindowFocus
#else
#define MAYBE_PopupWindowFocus PopupWindowFocus
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       MAYBE_PopupWindowFocus) {
  GURL main_url(embedded_test_server()->GetURL("/page_with_focus_events.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Set window.name on main page.  This will be used to identify the page
  // later when it sends messages from its focus/blur events.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ExecJs(web_contents, "window.name = 'main'",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Open a popup for a cross-site page.
  GURL popup_url =
      embedded_test_server()->GetURL("foo.com", "/page_with_focus_events.html");
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(
      ExecJs(web_contents, "openPopup('" + popup_url.spec() + "','popup')"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(popup_url, popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Switch focus to the original tab, since opening a popup also focused it.
  web_contents->GetDelegate()->ActivateContents(web_contents);
  EXPECT_EQ(web_contents, browser()->tab_strip_model()->GetActiveWebContents());

  // Focus the popup via window.focus(), this needs user gesture.
  content::DOMMessageQueue main_queue(web_contents);
  content::DOMMessageQueue popup_queue(popup);
  ExecuteScriptAsync(web_contents, "focusPopup()");

  // Wait for main page to lose focus and for popup to gain focus.  Each event
  // will send a message, and the two messages can arrive in any order.
  std::string status;
  while (main_queue.WaitForMessage(&status)) {
    if (status == "\"main-lost-focus\"") {
      break;
    }
  }
  while (popup_queue.WaitForMessage(&status)) {
    if (status == "\"popup-got-focus\"") {
      break;
    }
  }

  // The popup should be focused now.
  EXPECT_EQ(popup, browser()->tab_strip_model()->GetActiveWebContents());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SitePerProcessInteractiveFencedFrameBrowserTest,
    ::testing::Values("fencedframe", "iframe"),
    &SitePerProcessInteractiveFencedFrameBrowserTest::DescribeParams);
