// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/text_input_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/base/ime/linux/text_edit_command_auralinux.h"
#include "ui/linux/fake_linux_ui.h"
#include "ui/linux/linux_ui.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// TextInputManager and IME Tests
//
// The following tests verify the correctness of TextInputState tracking on the
// browser side. They also make sure the IME logic works correctly. The baseline
// for comparison is the default functionality in the non-OOPIF case (i.e., the
// legacy implementation in RWHV's other than RWHVCF).
// These tests live outside content/ because they rely on being part of the
// interactive UI test framework (to avoid flakiness).

namespace {
using IndexVector = std::vector<size_t>;

// This class observes TextInputManager for the first change in TextInputState.
class TextInputManagerChangeObserver
    : public content::TextInputManagerObserverBase {
 public:
  explicit TextInputManagerChangeObserver(content::WebContents* web_contents)
      : TextInputManagerObserverBase(web_contents) {
    tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
        &TextInputManagerChangeObserver::VerifyChange, base::Unretained(this)));
  }

  TextInputManagerChangeObserver(const TextInputManagerChangeObserver&) =
      delete;
  TextInputManagerChangeObserver& operator=(
      const TextInputManagerChangeObserver&) = delete;

 private:
  void VerifyChange() {
    if (tester()->IsTextInputStateChanged())
      OnSuccess();
  }
};

// This class observes |TextInputState.type| for a specific RWHV.
class ViewTextInputTypeObserver : public content::TextInputManagerObserverBase {
 public:
  explicit ViewTextInputTypeObserver(content::WebContents* web_contents,
                                     content::RenderWidgetHostView* rwhv,
                                     ui::TextInputType expected_type)
      : TextInputManagerObserverBase(web_contents),
        web_contents_(web_contents),
        view_(rwhv),
        expected_type_(expected_type) {
    tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
        &ViewTextInputTypeObserver::VerifyType, base::Unretained(this)));
  }

  ViewTextInputTypeObserver(const ViewTextInputTypeObserver&) = delete;
  ViewTextInputTypeObserver& operator=(const ViewTextInputTypeObserver&) =
      delete;

 private:
  void VerifyType() {
    ui::TextInputType type;
    if (!content::GetTextInputTypeForView(web_contents_, view_, &type))
      return;
    if (expected_type_ == type)
      OnSuccess();
  }

  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  raw_ptr<content::RenderWidgetHostView, AcrossTasksDanglingUntriaged> view_;
  const ui::TextInputType expected_type_;
};

// This class observes the |expected_view| for the first change in its
// selection bounds.
class ViewSelectionBoundsChangedObserver
    : public content::TextInputManagerObserverBase {
 public:
  ViewSelectionBoundsChangedObserver(
      content::WebContents* web_contents,
      content::RenderWidgetHostView* expected_view)
      : TextInputManagerObserverBase(web_contents),
        expected_view_(expected_view) {
    tester()->SetOnSelectionBoundsChangedCallback(
        base::BindRepeating(&ViewSelectionBoundsChangedObserver::VerifyChange,
                            base::Unretained(this)));
  }

  ViewSelectionBoundsChangedObserver(
      const ViewSelectionBoundsChangedObserver&) = delete;
  ViewSelectionBoundsChangedObserver& operator=(
      const ViewSelectionBoundsChangedObserver&) = delete;

 private:
  void VerifyChange() {
    if (expected_view_ == tester()->GetUpdatedView())
      OnSuccess();
  }

  const raw_ptr<const content::RenderWidgetHostView> expected_view_;
};

// This class observes the |expected_view| for the first change in its
// composition range information.
class ViewCompositionRangeChangedObserver
    : public content::TextInputManagerObserverBase {
 public:
  ViewCompositionRangeChangedObserver(
      content::WebContents* web_contents,
      content::RenderWidgetHostView* expected_view)
      : TextInputManagerObserverBase(web_contents),
        expected_view_(expected_view) {
    tester()->SetOnImeCompositionRangeChangedCallback(
        base::BindRepeating(&ViewCompositionRangeChangedObserver::VerifyChange,
                            base::Unretained(this)));
  }

  ViewCompositionRangeChangedObserver(
      const ViewCompositionRangeChangedObserver&) = delete;
  ViewCompositionRangeChangedObserver& operator=(
      const ViewCompositionRangeChangedObserver&) = delete;

 private:
  void VerifyChange() {
    if (expected_view_ == tester()->GetUpdatedView())
      OnSuccess();
  }

  const raw_ptr<const content::RenderWidgetHostView> expected_view_;
};

// This class observes the |expected_view| for a change in the text selection.
class ViewTextSelectionObserver : public content::TextInputManagerObserverBase {
 public:
  ViewTextSelectionObserver(content::WebContents* web_contents,
                            content::RenderWidgetHostView* expected_view,
                            size_t expected_length)
      : TextInputManagerObserverBase(web_contents),
        expected_view_(expected_view),
        expected_length_(expected_length) {
    tester()->SetOnTextSelectionChangedCallback(base::BindRepeating(
        &ViewTextSelectionObserver::VerifyChange, base::Unretained(this)));
  }

  ViewTextSelectionObserver(const ViewTextSelectionObserver&) = delete;
  ViewTextSelectionObserver& operator=(const ViewTextSelectionObserver&) =
      delete;

 private:
  void VerifyChange() {
    if (expected_view_ == tester()->GetUpdatedView()) {
      size_t length;
      EXPECT_TRUE(tester()->GetCurrentTextSelectionLength(&length));
      if (length == expected_length_)
        OnSuccess();
    }
  }

  const raw_ptr<const content::RenderWidgetHostView> expected_view_;
  const size_t expected_length_;
};

// This class observes all the text selection updates within a WebContents.
class TextSelectionObserver : public content::TextInputManagerObserverBase {
 public:
  explicit TextSelectionObserver(content::WebContents* web_contents)
      : TextInputManagerObserverBase(web_contents) {
    tester()->SetOnTextSelectionChangedCallback(base::BindRepeating(
        &TextSelectionObserver::VerifyChange, base::Unretained(this)));
  }

  TextSelectionObserver(const TextSelectionObserver&) = delete;
  TextSelectionObserver& operator=(const TextSelectionObserver&) = delete;

  void WaitForSelectedText(const std::string& text) {
    selected_text_ = text;
    Wait();
  }

 private:
  void VerifyChange() {
    if (base::UTF16ToUTF8(tester()->GetUpdatedView()->GetSelectedText()) ==
        selected_text_) {
      OnSuccess();
    }
  }

  std::string selected_text_;
};

// This class monitors all the changes in TextInputState and keeps a record of
// the active views. There is no waiting and the recording process is
// continuous.
class RecordActiveViewsObserver {
 public:
  explicit RecordActiveViewsObserver(content::WebContents* web_contents)
      : tester_(new content::TextInputManagerTester(web_contents)) {
    tester_->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
        &RecordActiveViewsObserver::RecordActiveView, base::Unretained(this)));
  }

  RecordActiveViewsObserver(const RecordActiveViewsObserver&) = delete;
  RecordActiveViewsObserver& operator=(const RecordActiveViewsObserver&) =
      delete;

  const std::vector<
      raw_ptr<const content::RenderWidgetHostView, VectorExperimental>>*
  active_views() const {
    return &active_views_;
  }

 private:
  void RecordActiveView() {
    if (!tester_->IsTextInputStateChanged())
      return;
    active_views_.push_back(tester_->GetActiveView());
  }

  std::unique_ptr<content::TextInputManagerTester> tester_;
  std::vector<raw_ptr<const content::RenderWidgetHostView, VectorExperimental>>
      active_views_;
};

}  // namespace

// Main class for all TextInputState and IME related tests.
class SitePerProcessTextInputManagerTest : public InProcessBrowserTest {
 public:
  SitePerProcessTextInputManagerTest() {}

  SitePerProcessTextInputManagerTest(
      const SitePerProcessTextInputManagerTest&) = delete;
  SitePerProcessTextInputManagerTest& operator=(
      const SitePerProcessTextInputManagerTest&) = delete;

  ~SitePerProcessTextInputManagerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
    // Some builders are flaky due to slower loading interacting with
    // deferred commits, and this test suite sometimes uses content
    // that paints nothing upon load.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data 'cross_site_iframe_factory.html'.
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* active_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // static
  // Adds an <input> field to a given frame by executing javascript code.
  // The input can be added as the first element or the last element of
  // |document.body|. The text range defined by |selection_range| will be
  // marked.
  static void AddInputFieldToFrame(content::RenderFrameHost* rfh,
                                   const std::string& type,
                                   const std::string& value,
                                   bool append_as_first_child) {
    std::string script = base::StringPrintf(
        "var input = document.createElement('input');"
        "input.setAttribute('type', '%s');"
        "input.setAttribute('value', '%s');"
        "document.body.%s;",
        type.c_str(), value.c_str(),
        append_as_first_child ? "insertBefore(input, document.body.firstChild)"
                              : "appendChild(input)");
    EXPECT_TRUE(ExecJs(rfh, script));
  }

  // static
  // Appends an <input> field with various attribues to a given frame by
  // executing javascript code.
  static void AppendInputFieldToFrame(content::RenderFrameHost* rfh,
                                      const std::string& type,
                                      const std::string& id,
                                      const std::string& value,
                                      const std::string& placeholder) {
    std::string script = base::StringPrintf(
        "var input = document.createElement('input');"
        "input.setAttribute('type', '%s');"
        "input.setAttribute('id', '%s');"
        "input.setAttribute('value', '%s');"
        "input.setAttribute('placeholder', '%s');"
        "document.body.appendChild(input);",
        type.c_str(), id.c_str(), value.c_str(), placeholder.c_str());
    EXPECT_TRUE(ExecJs(rfh, script));
  }

  // static
  // Focus a form field by its Id.
  static void FocusFormField(content::RenderFrameHost* rfh,
                             const std::string& id) {
    std::string script = base::StringPrintf(
        "document.getElementById('%s').focus();", id.c_str());

    EXPECT_TRUE(ExecJs(rfh, script));
  }

  // Uses 'cross_site_iframe_factory.html'. The main frame's domain is
  // 'a.com'.
  void CreateIframePage(const std::string& structure) {
    std::string path = base::StringPrintf("/cross_site_iframe_factory.html?%s",
                                          structure.c_str());
    GURL main_url(embedded_test_server()->GetURL("a.com", path));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  // Iteratively uses ChildFrameAt(frame, i) to get the i-th child frame
  // inside frame. For example, for 'a(b(c, d(e)))', [0] returns b, and
  // [0, 1, 0] returns e;
  content::RenderFrameHost* GetFrame(const IndexVector& indices) {
    content::RenderFrameHost* current =
        active_contents()->GetPrimaryMainFrame();
    for (size_t index : indices)
      current = ChildFrameAt(current, index);
    return current;
  }
};

// The following test loads a page with multiple nested <iframe> elements which
// are in or out of process with the main frame. Then an <input> field with
// unique value is added to every single frame on the frame tree. The test then
// creates a sequence of tab presses and verifies that after each key press, the
// TextInputState.value reflects that of the focused input, i.e., the
// TextInputManager is correctly tracking TextInputState across frames.
// Flaky on ChromeOS, Linux, Mac, and Windows; https://crbug.com/704994.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       DISABLED_TrackStateWhenSwitchingFocusedFrames) {
  CreateIframePage("a(a,b,c(a,b,d(e, f)),g)");
  std::vector<std::string> values{
      "main",     "node_a",   "node_b",     "node_c",     "node_c_a",
      "node_c_b", "node_c_d", "node_c_d_e", "node_c_d_f", "node_g"};

  // TODO(ekaramad): The use for explicitly constructing the IndexVector from
  // initializer list should not be necessary. However, some chromeos bots throw
  //  errors if we do not do it like this.
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}),        GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}),       GetFrame(IndexVector{2}),
      GetFrame(IndexVector{2, 0}),    GetFrame(IndexVector{2, 1}),
      GetFrame(IndexVector{2, 2}),    GetFrame(IndexVector{2, 2, 0}),
      GetFrame(IndexVector{2, 2, 1}), GetFrame(IndexVector{3})};

  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", values[i], true);

  for (size_t i = 0; i < frames.size(); ++i) {
    content::TextInputManagerValueObserver observer(active_contents(),
                                                    values[i]);
    SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    observer.Wait();
  }
}

// The following test loads a page with two OOPIFs. An <input> is added to both
// frames and tab key is pressed until the one in the second OOPIF is focused.
// Then, the renderer processes for both frames are crashed. The test verifies
// that the TextInputManager stops tracking the RWHVs as well as properly
// resets the TextInputState after the second (active) RWHV goes away.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       StopTrackingCrashedChildFrame) {
  CreateIframePage("a(b, c)");
  std::vector<std::string> values{"node_b", "node_c"};
  std::vector<content::RenderFrameHost*> frames{GetFrame(IndexVector{0}),
                                                GetFrame(IndexVector{1})};

  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", values[i], true);

  // Tab into both inputs and make sure we correctly receive their
  // TextInputState. For the second tab two IPCs arrive: one from the first
  // frame to set the state to none, and another one from the second frame to
  // set it to TEXT. To avoid the race between them, we shall also observe the
  // first frame setting its state to NONE after the second tab.
  ViewTextInputTypeObserver view_type_observer(
      active_contents(), frames[0]->GetView(), ui::TEXT_INPUT_TYPE_NONE);

  for (size_t i = 0; i < frames.size(); ++i) {
    content::TextInputManagerValueObserver observer(active_contents(),
                                                    values[i]);
    SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    observer.Wait();
  }

  // Make sure that the first view has set its TextInputState.type to NONE.
  view_type_observer.Wait();

  // Verify that we are tracking the TextInputState from the first frame.
  content::RenderWidgetHostView* first_view = frames[0]->GetView();
  ui::TextInputType first_view_type;
  EXPECT_TRUE(content::GetTextInputTypeForView(active_contents(), first_view,
                                               &first_view_type));
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, first_view_type);

  size_t registered_views_count =
      content::GetRegisteredViewsCountFromTextInputManager(active_contents());

  // We expect at least two views for the two child frames.
  EXPECT_GT(registered_views_count, 2U);

  // Now that the second frame's <input> is focused, we crash the first frame
  // and observe that text input state is updated for the view.
  std::unique_ptr<content::TestRenderWidgetHostViewDestructionObserver>
      destruction_observer(
          new content::TestRenderWidgetHostViewDestructionObserver(first_view));
  {
    content::ScopedAllowRendererCrashes allow_renderer_crashes(frames[0]);
    frames[0]->GetProcess()->Shutdown(0);
    destruction_observer->Wait();
  }

  // Verify that the TextInputManager is no longer tracking TextInputState for
  // |first_view|.
  EXPECT_EQ(
      registered_views_count - 1U,
      content::GetRegisteredViewsCountFromTextInputManager(active_contents()));

  // Now crash the second <iframe> which has an active view.
  destruction_observer =
      std::make_unique<content::TestRenderWidgetHostViewDestructionObserver>(

          frames[1]->GetView());
  {
    content::ScopedAllowRendererCrashes allow_renderer_crashes(frames[1]);
    frames[1]->GetProcess()->Shutdown(0);
    destruction_observer->Wait();
  }

  EXPECT_EQ(
      registered_views_count - 2U,
      content::GetRegisteredViewsCountFromTextInputManager(active_contents()));
}

// The following test loads a page with two child frames: one in process and one
// out of process with main frame. The test inserts an <input> inside each frame
// and focuses the first frame and observes the TextInputManager setting the
// state to ui::TEXT_INPUT_TYPE_TEXT. Then, the frame is detached and the test
// observes that the state type is reset to ui::TEXT_INPUT_TYPE_NONE. The same
// sequence of actions is then performed on the out of process frame.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       ResetStateAfterFrameDetached) {
  CreateIframePage("a(a, b)");
  std::vector<content::RenderFrameHost*> frames{GetFrame(IndexVector{0}),
                                                GetFrame(IndexVector{1})};

  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", "", true);

  // Press tab key to focus the <input> in the first frame.
  content::TextInputManagerTypeObserver type_observer_text_a(
      active_contents(), ui::TEXT_INPUT_TYPE_TEXT);
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  type_observer_text_a.Wait();

  std::string remove_first_iframe_script =
      "var frame = document.querySelector('iframe');"
      "frame.parentNode.removeChild(frame);";
  // Detach first frame and observe |TextInputState.type| resetting to
  // ui::TEXT_INPUT_TYPE_NONE.
  content::TextInputManagerTypeObserver type_observer_none_a(
      active_contents(), ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_TRUE(ExecJs(active_contents(), remove_first_iframe_script));
  type_observer_none_a.Wait();

  // Press tab to focus the <input> in the second frame.
  content::TextInputManagerTypeObserver type_observer_text_b(
      active_contents(), ui::TEXT_INPUT_TYPE_TEXT);
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  type_observer_text_b.Wait();

  // Detach first frame and observe |TextInputState.type| resetting to
  // ui::TEXT_INPUT_TYPE_NONE.
  content::TextInputManagerTypeObserver type_observer_none_b(
      active_contents(), ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_TRUE(ExecJs(active_contents(), remove_first_iframe_script));
  type_observer_none_b.Wait();
}

// This test creates a page with one OOPIF and adds an <input> to it. Then, the
// <input> is focused and the test verfies that the |TextInputState.type| is set
// to ui::TEXT_INPUT_TYPE_TEXT. Next, the child frame is navigated away and the
// test verifies that |TextInputState.type| resets to ui::TEXT_INPUT_TYPE_NONE.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       ResetStateAfterChildNavigation) {
  CreateIframePage("a(b)");
  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  content::RenderFrameHost* child_frame = GetFrame(IndexVector{0});

  AddInputFieldToFrame(child_frame, "text", "child", false);

  // Focus <input> in child frame and verify the |TextInputState.value|.
  content::TextInputManagerValueObserver child_set_state_observer(
      active_contents(), "child");
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  child_set_state_observer.Wait();

  // Navigate the child frame to about:blank and verify that TextInputManager
  // correctly sets its |TextInputState.type| to ui::TEXT_INPUT_TYPE_NONE.
  content::TextInputManagerTypeObserver child_reset_state_observer(
      active_contents(), ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_TRUE(ExecJs(main_frame,
                     "document.querySelector('iframe').src = 'about:blank'"));
  child_reset_state_observer.Wait();
}

// This test creates a blank page and adds an <input> to it. Then, the <input>
// is focused and the test verfies that the |TextInputState.type| is set to
// ui::TEXT_INPUT_TYPE_TEXT. Next, the browser is navigated away and the test
// verifies that |TextInputState.type| resets to ui::TEXT_INPUT_TYPE_NONE.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       ResetStateAfterBrowserNavigation) {
  CreateIframePage("a()");
  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  AddInputFieldToFrame(main_frame, "text", "", false);

  content::TextInputManagerTypeObserver set_state_observer(
      active_contents(), ui::TEXT_INPUT_TYPE_TEXT);
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  set_state_observer.Wait();

  content::TextInputManagerTypeObserver reset_state_observer(
      active_contents(), ui::TEXT_INPUT_TYPE_NONE);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  reset_state_observer.Wait();
}

#if defined(USE_AURA)
// This test creates a blank page and adds an <input> to it. Then, the <input>
// is focused, UI is focused, then the input is refocused. The test verifies
// that selection bounds change with the refocus (see https://crbug.com/864563).
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       SelectionBoundsChangeAfterRefocusInput) {
  CreateIframePage("a()");
  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  content::RenderWidgetHostView* view = main_frame->GetView();
  content::WebContents* web_contents = active_contents();
  AddInputFieldToFrame(main_frame, "text", "", false);

  auto focus_input_and_wait_for_selection_bounds_change =
      [&main_frame, &web_contents, &view]() {
        ViewSelectionBoundsChangedObserver bounds_observer(web_contents, view);
        // SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
        //               ui::VKEY_TAB, false, true, false, false);
        EXPECT_TRUE(
            ExecJs(main_frame, "document.querySelector('input').focus();"));
        bounds_observer.Wait();
      };

  focus_input_and_wait_for_selection_bounds_change();

  // Focus location bar.
  BrowserWindow* window = browser()->window();
  ASSERT_TRUE(window);
  LocationBar* location_bar = window->GetLocationBar();
  ASSERT_TRUE(location_bar);
  location_bar->FocusLocation(true);

  focus_input_and_wait_for_selection_bounds_change();
}
#endif

// This test verifies that if we have a focused <input> in the main frame and
// the tab is closed, TextInputManager handles unregistering itself and
// notifying the observers properly (see https://crbug.com/669375).
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       ClosingTabWillNotCrash) {
  CreateIframePage("a()");
  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  AddInputFieldToFrame(main_frame, "text", "", false);

  // Focus the input and wait for state update.
  content::TextInputManagerTypeObserver observer(active_contents(),
                                                 ui::TEXT_INPUT_TYPE_TEXT);
  SimulateKeyPress(active_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, false, false, false);
  observer.Wait();

  // Now destroy the tab. We should exit without crashing.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
}

// The following test verifies that when the active widget changes value, it is
// always from nullptr to non-null or vice versa.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       ResetTextInputStateOnActiveWidgetChange) {
  CreateIframePage("a(b,c(a,b),d)");
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}),     GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}),    GetFrame(IndexVector{1, 0}),
      GetFrame(IndexVector{1, 1}), GetFrame(IndexVector{2})};
  std::vector<content::RenderWidgetHostView*> views;
  for (auto* frame : frames)
    views.push_back(frame->GetView());
  std::vector<std::string> values{"a", "ab", "ac", "aca", "acb", "acd"};
  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", values[i], true);

  content::WebContents* web_contents = active_contents();

  auto send_tab_and_wait_for_value =
      [&web_contents](const std::string& expected_value) {
        content::TextInputManagerValueObserver observer(web_contents,
                                                        expected_value);
        SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                         ui::VKEY_TAB, false, false, false, false);
        observer.Wait();
      };

  // Record all active view changes.
  RecordActiveViewsObserver recorder(web_contents);
  for (auto value : values)
    send_tab_and_wait_for_value(value);

  // We have covered a total of 6 views, so there should at least be 11 entries
  // recorded (at least one null between two views).
  size_t record_count = recorder.active_views()->size();
  EXPECT_GT(record_count, 10U);

  // Verify we do not have subsequent nullptr or non-nullptrs.
  for (size_t i = 0; i < record_count - 1U; ++i) {
    const content::RenderWidgetHostView* current =
        recorder.active_views()->at(i);
    const content::RenderWidgetHostView* next =
        recorder.active_views()->at(i + 1U);
    EXPECT_TRUE((current != nullptr && next == nullptr) ||
                (current == nullptr && next != nullptr));
  }
}

// This test creates a page with multiple child frames and adds an <input> to
// each frame. Then, sequentially, each <input> is focused by sending a tab key.
// Then, after |TextInputState.type| for a view is changed to text, the test
// sends a set composition IPC to the active widget and waits until the widget
// updates its composition range.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       TrackCompositionRangeForAllFrames) {
  CreateIframePage("a(b,c(a,b),d)");
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}),     GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}),    GetFrame(IndexVector{1, 0}),
      GetFrame(IndexVector{1, 1}), GetFrame(IndexVector{2})};
  std::vector<content::RenderWidgetHostView*> views;
  for (auto* frame : frames)
    views.push_back(frame->GetView());
  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", "text", true);

  content::WebContents* web_contents = active_contents();

  auto send_tab_set_composition_wait_for_bounds_change =
      [&web_contents](content::RenderWidgetHostView* view) {
        ViewTextInputTypeObserver type_observer(web_contents, view,
                                                ui::TEXT_INPUT_TYPE_TEXT);
        SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                         ui::VKEY_TAB, false, false, false, false);
        type_observer.Wait();

        ViewCompositionRangeChangedObserver range_observer(web_contents, view);
        EXPECT_TRUE(
            content::RequestCompositionInfoFromActiveWidget(web_contents));
        range_observer.Wait();
      };

  for (auto* view : views)
    send_tab_set_composition_wait_for_bounds_change(view);
}

// Failing on Mac - http://crbug.com/852452
#if BUILDFLAG(IS_MAC)
#define MAYBE_TrackTextSelectionForAllFrames \
  DISABLED_TrackTextSelectionForAllFrames
#else
#define MAYBE_TrackTextSelectionForAllFrames TrackTextSelectionForAllFrames
#endif

// This test creates a page with multiple child frames and adds an <input> to
// each frame. Then, sequentially, each <input> is focused by sending a tab key.
// After focusing each input, a sequence of key presses (character 'E') are sent
// to the focused widget. The test then verifies that the selection length
// equals the length of the sequence of 'E's.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       MAYBE_TrackTextSelectionForAllFrames) {
  CreateIframePage("a(b,c(a,b),d)");
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}),     GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}),    GetFrame(IndexVector{1, 0}),
      GetFrame(IndexVector{1, 1}), GetFrame(IndexVector{2})};
  std::vector<std::string> values{"main", "b", "c", "ca", "cb", "d"};
  std::vector<content::RenderWidgetHostView*> views;
  for (auto* frame : frames)
    views.push_back(frame->GetView());
  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", values[i], true);

  content::WebContents* web_contents = active_contents();

  auto send_tab_and_wait_for_value = [&web_contents](const std::string& value) {
    content::TextInputManagerValueObserver observer(web_contents, value);
    SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    observer.Wait();
  };

  auto send_keys_select_all_wait_for_selection_change =
      [&web_contents](content::RenderWidgetHostView* view, size_t count) {
        ViewTextSelectionObserver observer(web_contents, view, count);
        for (size_t i = 0; i < count; ++i) {
          SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('E'),
                           ui::DomCode::US_E, ui::VKEY_E, false, false, false,
                           false);
        }
        observer.Wait();
      };

  size_t count = 2;
  for (size_t i = 0; i < views.size(); ++i) {
    // First focus the <input>.
    send_tab_and_wait_for_value(values[i]);

    // Send a sequence of |count| 'E' keys and wait until the view receives a
    // selection change update for a text of the corresponding size, |count|.
    send_keys_select_all_wait_for_selection_change(views[i], count++);
  }
}

// This test verifies that committing text works as expected for all the frames
// on the page. Specifically, the test sends an IPC to the RenderWidget
// corresponding to a focused frame with a focused <input> to commit some text.
// Then, it verifies that the <input>'s value matches the committed text
// (https://crbug.com/688842).
// Flaky on Android and Linux http://crbug.com/852274
#if BUILDFLAG(IS_MAC)
#define MAYBE_ImeCommitTextForAllFrames DISABLED_ImeCommitTextForAllFrames
#else
#define MAYBE_ImeCommitTextForAllFrames ImeCommitTextForAllFrames
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       MAYBE_ImeCommitTextForAllFrames) {
  CreateIframePage("a(b,c(a))");
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}), GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}), GetFrame(IndexVector{1, 0})};
  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", "", true);

  std::vector<std::string> sample_text{"main", "child_b", "child_c", "child_a"};
  ASSERT_EQ(frames.size(), sample_text.size());

  // An observer of all text selection updates within a WebContents.
  TextSelectionObserver observer(active_contents());
  for (size_t index = 0; index < frames.size(); ++index) {
    // Focus the input and listen to 'input' event inside the frame. When the
    // event fires, select all the text inside the input. This will trigger a
    // selection update on the browser side.
    ASSERT_TRUE(ExecJs(frames[index],
                       "window.focus();"
                       "var input = document.querySelector('input');"
                       "input.focus();"
                       "window.addEventListener('input', function(e) {"
                       "  input.select();"
                       "});"))
        << "Could not run script in frame with index:" << index;

    // Commit some text for this frame.
    content::SendImeCommitTextToWidget(
        frames[index]->GetView()->GetRenderWidgetHost(),
        base::UTF8ToUTF16(sample_text[index]), std::vector<ui::ImeTextSpan>(),
        gfx::Range(), 0);

    // Verify that the text we committed is now selected by listening to a
    // selection update from a RenderWidgetHostView which has the expected
    // selected text.
    observer.WaitForSelectedText(sample_text[index]);
  }
}

// TODO(ekaramad): Some of the following tests should be active on Android as
// well. Enable them when the corresponding feature is implemented for Android
// (https://crbug.com/602723).
#if !BUILDFLAG(IS_ANDROID)
// This test creates a page with multiple child frames and adds an <input> to
// each frame. Then, sequentially, each <input> is focused by sending a tab key.
// Then, after |TextInputState.type| for a view is changed to text, another key
// is pressed (a character) and then the test verifies that TextInputManager
// receives the corresponding update on the change in selection bounds on the
// browser side.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       TrackSelectionBoundsForAllFrames) {
  CreateIframePage("a(b,c(a,b),d)");
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}),     GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}),    GetFrame(IndexVector{1, 0}),
      GetFrame(IndexVector{1, 1}), GetFrame(IndexVector{2})};
  std::vector<content::RenderWidgetHostView*> views;
  for (auto* frame : frames)
    views.push_back(frame->GetView());
  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", "", true);

  content::WebContents* web_contents = active_contents();

  auto send_tab_insert_text_wait_for_bounds_change =
      [&web_contents](content::RenderWidgetHostView* view) {
        ViewTextInputTypeObserver type_observer(web_contents, view,
                                                ui::TEXT_INPUT_TYPE_TEXT);
        SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                         ui::VKEY_TAB, false, false, false, false);
        type_observer.Wait();
        ViewSelectionBoundsChangedObserver bounds_observer(web_contents, view);
        SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('E'),
                         ui::DomCode::US_E, ui::VKEY_E, false, false, false,
                         false);
        bounds_observer.Wait();
      };

  for (auto* view : views)
    send_tab_insert_text_wait_for_bounds_change(view);
}

// This test makes sure browser correctly tracks focused editable element inside
// each RenderFrameHost.
// Test is flaky on chromeOS; https://crbug.com/705203.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_TrackingFocusedElementForAllFrames \
  DISABLED_TrackingFocusedElementForAllFrames
#else
#define MAYBE_TrackingFocusedElementForAllFrames \
  TrackingFocusedElementForAllFrames
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       MAYBE_TrackingFocusedElementForAllFrames) {
  CreateIframePage("a(a, b(a))");
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}), GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}), GetFrame(IndexVector{1, 0})};
  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", "some text", true);

  // Focus the <input> in |frame| and return if RenderFrameHost thinks there is
  // a focused editable element in it.
  auto focus_input_and_return_editable_element_state =
      [](content::RenderFrameHost* frame) {
        EXPECT_TRUE(ExecJs(frame, "document.querySelector('input').focus();"));
        return content::DoesFrameHaveFocusedEditableElement(frame);
      };

  // When focusing an <input> we should receive an update.
  for (auto* frame : frames)
    EXPECT_TRUE(focus_input_and_return_editable_element_state(frame));

  // Blur the <input> in |frame| and return if RenderFrameHost thinks there is a
  // focused editable element in it.
  auto blur_input_and_return_editable_element_state =
      [](content::RenderFrameHost* frame) {
        EXPECT_TRUE(ExecJs(frame, "document.querySelector('input').blur();"));
        return content::DoesFrameHaveFocusedEditableElement(frame);
      };

  // Similarly, we should receive updates when losing focus.
  for (auto* frame : frames)
    EXPECT_FALSE(blur_input_and_return_editable_element_state(frame));
}

// This test tracks page level focused editable element tracking using
// WebContents. In a page with multiple frames, a frame is selected and
// focused. Then the <input> inside frame is both focused and blurred and  and
// in both cases the test verifies that WebContents is aware whether or not a
// focused editable element exists on the page.
// Test is flaky on ChromeOS. crbug.com/705289
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_TrackPageFocusEditableElement \
  DISABLED_TrackPageFocusEditableElement
#else
#define MAYBE_TrackPageFocusEditableElement TrackPageFocusEditableElement
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       MAYBE_TrackPageFocusEditableElement) {
  CreateIframePage("a(a, b(a))");
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}), GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}), GetFrame(IndexVector{1, 0})};
  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", "some text", true);

  auto focus_frame = [](content::RenderFrameHost* frame) {
    EXPECT_TRUE(ExecJs(frame, "window.focus();"));
  };

  auto set_input_focus = [](content::RenderFrameHost* frame, bool focus) {
    EXPECT_TRUE(ExecJs(
        frame, base::StringPrintf("document.querySelector('input').%s();",
                                  (focus ? "focus" : "blur"))));
  };

  for (auto* frame : frames) {
    focus_frame(frame);
    // Focus the <input>.
    set_input_focus(frame, true);
    EXPECT_TRUE(active_contents()->IsFocusedElementEditable());
    // No blur <input>.
    set_input_focus(frame, false);
    EXPECT_FALSE(active_contents()->IsFocusedElementEditable());
  }
}

// TODO(ekaramad): Could this become a unit test instead?
// This test focuses <input> elements on the page and verifies that
// WebContents knows about the focused editable element. Then it asks the
// WebContents to clear focused element and verifies that there is no longer
// a focused editable element on the page.
// Test is flaky on ChromeOS; https://crbug.com/705203.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ClearFocusedElementOnPage DISABLED_ClearFocusedElementOnPage
#else
#define MAYBE_ClearFocusedElementOnPage ClearFocusedElementOnPage
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       MAYBE_ClearFocusedElementOnPage) {
  CreateIframePage("a(a, b(a))");
  std::vector<content::RenderFrameHost*> frames{
      GetFrame(IndexVector{}), GetFrame(IndexVector{0}),
      GetFrame(IndexVector{1}), GetFrame(IndexVector{1, 0})};
  for (size_t i = 0; i < frames.size(); ++i)
    AddInputFieldToFrame(frames[i], "text", "some text", true);

  auto focus_frame_and_input = [](content::RenderFrameHost* frame) {
    EXPECT_TRUE(ExecJs(frame,
                       "window.focus();"
                       "document.querySelector('input').focus();"));
  };

  for (auto* frame : frames) {
    focus_frame_and_input(frame);
    EXPECT_TRUE(active_contents()->IsFocusedElementEditable());
    active_contents()->ClearFocusedElement();
    EXPECT_FALSE(active_contents()->IsFocusedElementEditable());
  }
}

// TODO(ekaramad): The following tests are specifically written for Aura and are
// based on InputMethodObserver. Write similar tests for Mac/Android/Mus
// (crbug.com/602723).
#if defined(USE_AURA)
// -----------------------------------------------------------------------------
// Input Method Observer Tests
//
// The following tests will make use of the InputMethodObserver to verify that
// OOPIF pages interact properly with the InputMethod through the tab's view.

// TODO(ekaramad): We only have coverage for some aura tests as the whole idea
// of ui::TextInputClient/ui::InputMethod/ui::InputMethodObserver seems to be
// only fit to aura (specifically, OS_CHROMEOS). Can we add more tests here for
// aura as well as other platforms (https://crbug.com/602723)?

// Observes current input method for state changes.
class InputMethodObserverBase {
 public:
  explicit InputMethodObserverBase(content::WebContents* web_contents)
      : success_(false),
        test_observer_(content::TestInputMethodObserver::Create(web_contents)) {
  }

  InputMethodObserverBase(const InputMethodObserverBase&) = delete;
  InputMethodObserverBase& operator=(const InputMethodObserverBase&) = delete;

  void Wait() {
    if (success_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner();
    message_loop_runner_->Run();
  }

  bool success() const { return success_; }

 protected:
  content::TestInputMethodObserver* test_observer() {
    return test_observer_.get();
  }

  const base::RepeatingCallback<void(bool)> success_closure() {
    return base::BindRepeating(
        [](InputMethodObserverBase* self, bool should_show) {
          if (should_show)
            self->OnSuccess();
        },
        base::Unretained(this));
  }

 private:
  void OnSuccess() {
    success_ = true;
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  bool success_;
  std::unique_ptr<content::TestInputMethodObserver> test_observer_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

class InputMethodObserverForShowIme : public InputMethodObserverBase {
 public:
  explicit InputMethodObserverForShowIme(content::WebContents* web_contents)
      : InputMethodObserverBase(web_contents) {
    test_observer()->SetOnVirtualKeyboardVisibilityChangedIfEnabledCallback(
        success_closure());
  }

  InputMethodObserverForShowIme(const InputMethodObserverForShowIme&) = delete;
  InputMethodObserverForShowIme& operator=(
      const InputMethodObserverForShowIme&) = delete;
};

// TODO(ekaramad): This test is actually a unit test and should be moved to
// somewhere more relevant (https://crbug.com/602723).
// This test verifies that the IME for Aura is shown if and only if the current
// client's |TextInputState.type| is not ui::TEXT_INPUT_TYPE_NONE and the flag
// |TextInputState.show_ime_if_needed| is true. This should happen even when
// the TextInputState has not changed (according to the platform), e.g., in
// aura when receiving two consecutive updates with same |TextInputState.type|.

// This test is disabled on Windows because we have removed TryShow/TryHide API
// calls and replaced it with TSF input pane policy which is a policy applied by
// text service framework on Windows based on whether TSF edit control has focus
// or not. On Windows we have implemented TSF1 on Chromium that takes care of
// IME compositions, handwriting panels, SIP visibility etc. Please see
// (https://crbug.com/1007958) for more details.
#if !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       CorrectlyShowVirtualKeyboardIfEnabled) {
  // We only need the <iframe> page to create RWHV.
  CreateIframePage("a()");
  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  content::RenderWidgetHostView* view = main_frame->GetView();
  content::WebContents* web_contents = active_contents();

  content::TextInputStateSender sender(view);

  auto send_and_check_show_ime = [&sender, &web_contents]() {
    InputMethodObserverForShowIme observer(web_contents);
    sender.Send();
    return observer.success();
  };

  // Sending an empty state should not trigger ime.
  EXPECT_FALSE(send_and_check_show_ime());

  // Set |TextInputState.type| to text. Expect no IME.
  sender.SetType(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_FALSE(send_and_check_show_ime());

  // Set |TextInputState.show_ime_if_needed| to true. Expect IME.
  sender.SetShowVirtualKeyboardIfEnabled(true);
  EXPECT_TRUE(send_and_check_show_ime());

  // Send the same message. Expect IME (no change).
  EXPECT_TRUE(send_and_check_show_ime());

  // Reset |TextInputState.show_ime_if_needed|. Expect no IME.
  sender.SetShowVirtualKeyboardIfEnabled(false);
  EXPECT_FALSE(send_and_check_show_ime());

  // Setting an irrelevant field. Expect no IME.
  sender.SetMode(ui::TEXT_INPUT_MODE_TEXT);
  EXPECT_FALSE(send_and_check_show_ime());

  // Set |TextInputState.show_ime_if_needed|. Expect IME.
  sender.SetShowVirtualKeyboardIfEnabled(true);
  EXPECT_TRUE(send_and_check_show_ime());

  // Set |TextInputState.type| to ui::TEXT_INPUT_TYPE_NONE. Expect no IME.
  sender.SetType(ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_FALSE(send_and_check_show_ime());
}
#endif  // !BUILDFLAG(IS_WIN)

#endif  // USE_AURA

// Ensure that a cross-process subframe can utilize keyboard edit commands.
// See https://crbug.com/640706.  This test is Linux-specific, as it relies on
// overriding ui::LinuxUi.
#if BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       SubframeKeyboardEditCommands) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents = active_contents();

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
  EXPECT_EQ(child, web_contents->GetFocusedFrame());

  // Generate a couple of keystrokes, which will be routed to the subframe.
  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('1'),
                   ui::DomCode::DIGIT1, ui::VKEY_1, false, false, false, false);
  EXPECT_TRUE(ExecJs(child, "waitForInput()"));
  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('2'),
                   ui::DomCode::DIGIT2, ui::VKEY_2, false, false, false, false);
  EXPECT_TRUE(ExecJs(child, "waitForInput()"));

  // Verify that the input field in the subframe received the keystrokes.
  EXPECT_EQ("12", EvalJs(child, "getInputFieldText();"));

  // Define and install a test delegate that translates any keystroke to a
  // command to delete all text from current cursor position to the beginning
  // of the line.
  class TextDeleteDelegate : public ui::FakeLinuxUi {
   public:
    TextDeleteDelegate() = default;

    TextDeleteDelegate(const TextDeleteDelegate&) = delete;
    TextDeleteDelegate& operator=(const TextDeleteDelegate&) = delete;

    ~TextDeleteDelegate() override = default;

    bool GetTextEditCommandsForEvent(
        const ui::Event& event,
        int text_flags,
        std::vector<ui::TextEditCommandAuraLinux>* commands) override {
      if (commands) {
        commands->emplace_back(ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE,
                               "");
      }
      return true;
    }
  };

  auto test_delete_delegate = std::make_unique<TextDeleteDelegate>();
  auto* old_linux_ui = ui::LinuxUi::SetInstance(test_delete_delegate.get());

  // Press ctrl-alt-shift-D.  The test's delegate will pretend that this
  // corresponds to the command to delete everyting to the beginning of the
  // line.  Note the use of SendKeyPressSync instead of SimulateKeyPress, as
  // the latter doesn't go through
  // RenderWidgetHostViewAura::ForwardKeyboardEvent, which contains the edit
  // commands logic that's tested here.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_D, true, true,
                                              true, false));
  ui::LinuxUi::SetInstance(old_linux_ui);

  // Verify that the input field in the subframe is erased.
  EXPECT_EQ("", EvalJs(child, "getInputFieldText();"));
}
#endif

// Ideally, the following code + test should be live in
// 'site_per_process_mac_browsertest.mm'. However, the test
// 'LookUpStringForRangeRoutesToFocusedWidget' relies on an override in
// ContentBrowserClient to register its filters in time. In content shell, we
// cannot have two instances of ShellContentBrowserClient (due to a DCHECK in
// the ctor). Therefore, we put the test here to use ChromeContentBrowserClient
// which does not have the same singleton constraint.
#if BUILDFLAG(IS_MAC)
class ShowDefinitionForWordObserver
    : content::RenderWidgetHostViewCocoaObserver {
 public:
  explicit ShowDefinitionForWordObserver(content::WebContents* web_contents)
      : content::RenderWidgetHostViewCocoaObserver(web_contents) {}

  ShowDefinitionForWordObserver(const ShowDefinitionForWordObserver&) = delete;
  ShowDefinitionForWordObserver& operator=(
      const ShowDefinitionForWordObserver&) = delete;

  ~ShowDefinitionForWordObserver() override {}

  const std::string& WaitForWordLookUp() {
    if (did_receive_string_)
      return word_;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    return word_;
  }

 private:
  void OnShowDefinitionForAttributedString(
      const std::string& for_word) override {
    did_receive_string_ = true;
    word_ = for_word;
    if (run_loop_)
      run_loop_->Quit();
  }

  bool did_receive_string_ = false;
  std::string word_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Flakey (https:://crbug.com/874417).
// This test verifies that requests for dictionary lookup based on selection
// range are routed to the focused RenderWidgetHost.
IN_PROC_BROWSER_TEST_F(SitePerProcessTextInputManagerTest,
                       DISABLED_LookUpStringForRangeRoutesToFocusedWidget) {
  CreateIframePage("a(b)");
  std::vector<content::RenderFrameHost*> frames{GetFrame(IndexVector{}),
                                                GetFrame(IndexVector{0})};
  std::string expected_words[] = {"word1", "word2"};

  // For each frame, add <input>, set its value to expected word, select it, ask
  // for dictionary and verify the word returned from renderer matches.
  for (size_t i = 0; i < frames.size(); ++i) {
    AddInputFieldToFrame(frames[i], "text", expected_words[i].c_str(), true);
    // Focusing the <input> automatically selects the text.
    ASSERT_TRUE(ExecJs(frames[i], "document.querySelector('input').focus();"));
    ShowDefinitionForWordObserver word_lookup_observer(active_contents());
    // Request for the dictionary lookup and intercept the word on its way back.
    // The request is always on the tab's view which is a
    // RenderWidgetHostViewMac.
    content::AskForLookUpDictionaryForRange(
        active_contents()->GetRenderWidgetHostView(),
        gfx::Range(0, expected_words[i].size()));
    EXPECT_EQ(expected_words[i], word_lookup_observer.WaitForWordLookUp());
  }
}

// This test verifies that when a word lookup result comes from the renderer
// after the target RenderWidgetHost has been deleted, the browser will not
// crash. This test covers the case where the target RenderWidgetHost is that of
// an OOPIF.
IN_PROC_BROWSER_TEST_F(
    SitePerProcessTextInputManagerTest,
    DoNotCrashBrowserInWordLookUpForDestroyedWidget_ChildFrame) {
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          active_contents()->GetBrowserContext(), nullptr));
  content::WebContents* raw_new_contents = new_contents.get();
  browser()->tab_strip_model()->InsertWebContentsAt(1, std::move(new_contents),
                                                    AddTabTypes::ADD_ACTIVE);
  EXPECT_EQ(active_contents(), raw_new_contents);

  // Simple page with 1 cross origin (out-of-process) <iframe>.
  CreateIframePage("a(b)");

  content::RenderFrameHost* child_frame = GetFrame(IndexVector{0});
  // Now add an <input> field and select its text.
  AddInputFieldToFrame(child_frame, "text", "four", true);
  EXPECT_TRUE(ExecJs(child_frame,
                     "window.focus();"
                     "document.querySelector('input').focus();"
                     "document.querySelector('input').select();"));

  content::TextInputTestLocalFrame text_input_local_frame;
  text_input_local_frame.SetUp(child_frame);

  // We need to wait for test scenario to complete before leaving this block.
  base::RunLoop test_complete_waiter;

  // Destroy the RenderWidgetHost from the browser side right after the
  // dictionary message is received. The destruction is post tasked to UI
  // thread.
  int32_t child_process_id = child_frame->GetProcess()->GetID();
  int32_t child_frame_routing_id = child_frame->GetRoutingID();

  text_input_local_frame.SetStringForRangeCallback(base::BindRepeating(
      [](int32_t process_id, int32_t routing_id,
         const base::RepeatingClosure& callback_on_io) {
        // This runs before TextInputClientMac gets to handle the mojo message.
        // Then, by the time TextInputClientMac calls back into UI to show the
        // dictionary, the target RWH is already destroyed which will be a
        // close enough repro for the crash in https://crbug.com/737032.
        ASSERT_TRUE(content::DestroyRenderWidgetHost(process_id, routing_id));

        // Quit the run loop on IO to make sure the message handler of
        // TextInputClientMac has successfully run on UI thread.
        content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE, callback_on_io);
      },
      child_process_id, child_frame_routing_id,
      test_complete_waiter.QuitClosure()));

  content::RenderWidgetHostView* page_rwhv =
      content::WebContents::FromRenderFrameHost(child_frame)
          ->GetRenderWidgetHostView();

  // The dictionary request to be made will be routed to the focused frame.
  ASSERT_EQ(child_frame, raw_new_contents->GetFocusedFrame());

  // Request for the dictionary lookup and intercept the word on its way back.
  // The request is always on the tab's view which is a RenderWidgetHostViewMac.
  content::AskForLookUpDictionaryForRange(page_rwhv, gfx::Range(0, 4));

  test_complete_waiter.Run();
}

// This test verifies that when a word lookup result comes from the renderer
// after the target RenderWidgetHost has been deleted, the browser will not
// crash. This test covers the case where the target RenderWidgetHost is that of
// the main frame (no OOPIFs on page).
IN_PROC_BROWSER_TEST_F(
    SitePerProcessTextInputManagerTest,
    DoNotCrashBrowserInWordLookUpForDestroyedWidget_MainFrame) {
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          active_contents()->GetBrowserContext(), nullptr));
  content::WebContents* raw_new_contents = new_contents.get();
  browser()->tab_strip_model()->InsertWebContentsAt(1, std::move(new_contents),
                                                    AddTabTypes::ADD_ACTIVE);
  EXPECT_EQ(active_contents(), raw_new_contents);

  // Simple page with no <iframe>s.
  CreateIframePage("a()");

  content::RenderFrameHost* main_frame = GetFrame(IndexVector{});
  // Now add an <input> field and select its text.
  AddInputFieldToFrame(main_frame, "text", "four", true);
  EXPECT_TRUE(ExecJs(main_frame,
                     "document.querySelector('input').focus();"
                     "document.querySelector('input').select();"));

  content::TextInputTestLocalFrame text_input_local_frame;
  text_input_local_frame.SetUp(main_frame);

  content::RenderWidgetHostView* page_rwhv = main_frame->GetView();

  // We need to wait for test scenario to complete before leaving this block.
  base::RunLoop test_complete_waiter;

  // Destroy the RenderWidgetHost from the browser side right after the
  // dictionary message is received. The destruction is post tasked to UI
  // thread.
  int32_t main_frame_process_id = main_frame->GetProcess()->GetID();
  int32_t main_frame_routing_id = main_frame->GetRoutingID();
  text_input_local_frame.SetStringForRangeCallback(base::BindRepeating(
      [](int32_t process_id, int32_t routing_id,
         const base::RepeatingClosure& callback_on_io) {
        // This runs before TextInputClientMac gets to handle the mojo message.
        // Then, by the time TextInputClientMac calls back into UI to show the
        // dictionary, the target RWH is already destroyed which will be a
        // close enough repro for the crash in https://crbug.com/737032.
        ASSERT_TRUE(content::DestroyRenderWidgetHost(process_id, routing_id));

        // Quit the run loop on IO to make sure the message handler of
        // TextInputClientMac has successfully run on UI thread.
        content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE, callback_on_io);
      },
      main_frame_process_id, main_frame_routing_id,
      test_complete_waiter.QuitClosure()));

  // Request for the dictionary lookup and intercept the word on its way back.
  // The request is always on the tab's view which is a RenderWidgetHostViewMac.
  content::AskForLookUpDictionaryForRange(page_rwhv, gfx::Range(0, 4));

  test_complete_waiter.Run();
}
#endif  //  defined(MAC_OSX)

#endif  // !BUILDFLAG(IS_ANDROID)
