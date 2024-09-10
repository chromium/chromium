// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/document_picture_in_picture_window_controller.h"

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#endif

using content::EvalJs;
using content::ExecJs;
using ::testing::_;

namespace {

const base::FilePath::CharType kPictureInPictureDocumentPipPage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");

// Observes a views::Widget and waits for it to be active or inactive.
class WidgetActivationWaiter : public views::WidgetObserver {
 public:
  explicit WidgetActivationWaiter(views::Widget* widget) : widget_(widget) {
    CHECK(widget_);
    widget_->AddObserver(this);
  }
  WidgetActivationWaiter(const WidgetActivationWaiter&) = delete;
  WidgetActivationWaiter& operator=(const WidgetActivationWaiter&) = delete;
  ~WidgetActivationWaiter() override {
    if (widget_) {
      widget_->RemoveObserver(this);
      widget_ = nullptr;
    }
  }

  // Eventually returns true if the actual activation state matches `activated`.
  // Returns false if the Widget is destroyed before that activation state ever
  // matches `activated`.
  bool WaitForActivationState(bool activated) {
    if (!widget_) {
      return false;
    }

    if (widget_->IsActive() == activated) {
      return true;
    }

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();

    if (!widget_) {
      return false;
    }
    return widget_->IsActive() == activated;
  }

  // views::WidgetObserver:

  void OnWidgetDestroying(views::Widget*) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
    run_loop_->Quit();
  }

  void OnWidgetActivationChanged(views::Widget*, bool active) override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

 private:
  raw_ptr<views::Widget> widget_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class DocumentPictureInPictureWindowControllerBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<gfx::Size> {
 public:
  DocumentPictureInPictureWindowControllerBrowserTest() = default;

  DocumentPictureInPictureWindowControllerBrowserTest(
      const DocumentPictureInPictureWindowControllerBrowserTest&) = delete;
  DocumentPictureInPictureWindowControllerBrowserTest& operator=(
      const DocumentPictureInPictureWindowControllerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "DocumentPictureInPictureAPI");
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kDocumentPictureInPictureAPI,
         blink::features::kDocumentPictureInPicturePreferInitialPlacement},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
  }

  void SetUpWindowController(content::WebContents* web_contents) {
    pip_window_controller_ = content::PictureInPictureWindowController::
        GetOrCreateDocumentPictureInPictureController(web_contents);
  }

  content::DocumentPictureInPictureWindowController* window_controller() {
    return pip_window_controller_;
  }

  content::RenderWidgetHostView* GetRenderWidgetHostView() {
    if (!window_controller())
      return nullptr;

    if (auto* web_contents = window_controller()->GetChildWebContents())
      return web_contents->GetRenderWidgetHostView();

    return nullptr;
  }

  void LoadTabAndEnterPictureInPicture(
      Browser* browser,
      const gfx::Size& window_size = gfx::Size(500, 500),
      bool prefer_initial_window_placement = false) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kPictureInPictureDocumentPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));

    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    SetUpWindowController(active_web_contents);

    std::string script =
        base::StrCat({"createDocumentPipWindow({width:",
                      base::NumberToString(window_size.width()),
                      ",height:", base::NumberToString(window_size.height()),
                      ",preferInitialWindowPlacement:",
                      prefer_initial_window_placement ? "true" : "false"});
    script = base::StrCat({script, "})"});
    ASSERT_EQ(true, EvalJs(active_web_contents, script));
    ASSERT_TRUE(window_controller() != nullptr);
    // Especially on Linux, this isn't synchronous.
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&content::RenderWidgetHostView::IsShowing,
                            base::Unretained(GetRenderWidgetHostView())),
        true, base::Seconds(30))
        .Wait();
    ASSERT_TRUE(GetRenderWidgetHostView()->IsShowing());
  }

  void ClickButton(views::Button* button) {
    const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

  void WaitForPageLoad(content::WebContents* contents) {
    EXPECT_TRUE(WaitForLoadStop(contents));
    EXPECT_TRUE(WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
  }

  bool IsOriginSet(BrowserView* browser_view) {
    // Document Picture In Picture windows are always positioned relative to the
    // bottom-right corner. Therefore we can assert that the origin is set
    // whenever it is not located at the top-left corner.
    return browser_view->GetBounds().origin() != gfx::Point(0, 0);
  }

  void CheckOriginSet(BrowserView* browser_view) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(
            &DocumentPictureInPictureWindowControllerBrowserTest::IsOriginSet,
            base::Unretained(this), browser_view),
        true, base::Minutes(1))
        .Wait();
    EXPECT_NE(browser_view->GetBounds().origin(), gfx::Point(0, 0));
  }

  // Watch for destruction of a WebContents. `is_destroyed()` will report if the
  // WebContents has been destroyed yet.
  class DestructionObserver : public content::WebContentsObserver {
   public:
    explicit DestructionObserver(content::WebContents* web_contents)
        : content::WebContentsObserver(web_contents) {}

    void WebContentsDestroyed() override { Observe(/*web_contents=*/nullptr); }

    // If we've stopped observing, it's because the WebContents was destroyed.
    bool is_destroyed() const { return !web_contents(); }
  };

 private:
  raw_ptr<content::DocumentPictureInPictureWindowController,
          AcrossTasksDanglingUntriaged>
      pip_window_controller_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Helper class that waits without polling a run loop until a condition is met.
// Note that it does not ever check the condition itself; some other thing, like
// an observer (see below), must notice that the condition is set as part of
// running the RunLoop.  One must derive a subclass to do whatever specific type
// of checks are required.
class TestConditionWaiter {
 public:
  // `check_cb` should return true if the condition is satisfied, and false if
  // it is not.  Will return once the condition is satisfied.  Because browser
  // tests have a timeout, we don't bother with one here.
  //
  // Will return immediately if `check_cb` is initially true.  Otherwise, it's
  // up to the subclass to call `CheckCondition()` to check the condition.
  // Probably, these calls are the result of work being run on the RunLoop.
  void Wait(base::RepeatingCallback<bool()> check_cb) {
    check_cb_ = std::move(check_cb);
    base::RunLoop run_loop_;
    quit_closure_ = run_loop_.QuitClosure();
    if (!check_cb_.Run()) {
      run_loop_.Run();
    }
  }

 protected:
  // Check the condition callback, and stop the run loop if it's happy.  It's up
  // to the subclass to call this when the state changes.
  void CheckCondition() {
    if (check_cb_.Run()) {
      quit_closure_.Run();
    }
  }

 private:
  base::RepeatingClosure quit_closure_;
  base::RepeatingCallback<bool()> check_cb_;
};

// Specialization of `TestConditionWaiter` that's useful for many types of
// observers.  The template argument is the observer type (e.g.,
// views::WidgetObserver).  Derive from this class, and implement whatever
// methods on `ObserverType` you need.  The subclass should call
// `CheckCondition()` to see if the condition is met.
template <typename ObserverType>
class TestObserverWaiter : public TestConditionWaiter, public ObserverType {
 public:
  // Same as `TestConditionWaiter::Wait()`, except it registers and unregisters
  // the observer on `y`.  It also guarantees that all calls to `check_cb` will
  // be made while `this` is registered to observe `y`.
  template <typename ObservedType>
  void Wait(ObservedType* y, base::RepeatingCallback<bool()> check_cb) {
    y->AddObserver(this);
    TestConditionWaiter::Wait(std::move(check_cb));
    y->RemoveObserver(this);
  }
};

}  // namespace

// Checks the creation of the window controller, as well as basic window
// creation, visibility and activation.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CreationAndVisibilityAndActivation) {
  LoadTabAndEnterPictureInPicture(browser());

  ASSERT_TRUE(GetRenderWidgetHostView());
  EXPECT_TRUE(GetRenderWidgetHostView()->IsShowing());
  EXPECT_TRUE(GetRenderWidgetHostView()->HasFocus());

  // Also verify that the window manager agrees about which WebContents is
  // which; the opener should not be the child web contents, but the child
  // contents should be(!).
  EXPECT_FALSE(PictureInPictureWindowManager::IsChildWebContents(
      window_controller()->GetWebContents()));
  EXPECT_TRUE(PictureInPictureWindowManager::IsChildWebContents(
      window_controller()->GetChildWebContents()));
}

// Regression test for https://crbug.com/1296780 - opening a picture-in-picture
// window twice in a row should work, closing the old window before opening the
// new one.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CreateTwice) {
  LoadTabAndEnterPictureInPicture(browser());

  ASSERT_TRUE(window_controller()->GetWebContents());
  ASSERT_TRUE(window_controller()->GetChildWebContents());
  DestructionObserver w(window_controller()->GetChildWebContents());

  // Now open the window a second time, without previously closing the original
  // window.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "documentPictureInPicture.requestWindow()"
                         ".then(w => true)"));
  base::RunLoop().RunUntilIdle();

  // The first WebContents should be destroyed.
  EXPECT_TRUE(w.is_destroyed());

  // The new WebContents should be visible and focused.
  ASSERT_TRUE(GetRenderWidgetHostView());
  EXPECT_TRUE(GetRenderWidgetHostView()->IsShowing());
  EXPECT_TRUE(GetRenderWidgetHostView()->HasFocus());
}

// Tests closing the document picture-in-picture window.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CloseWindow) {
  LoadTabAndEnterPictureInPicture(browser());

  window_controller()->Close(/*should_pause_video=*/true);

  ASSERT_FALSE(window_controller()->GetChildWebContents());
}

// Tests navigating the opener closes the picture in picture window.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       ClosePictureInPictureWhenOpenerNavigates) {
  LoadTabAndEnterPictureInPicture(browser());
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureDocumentPipPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));
  ASSERT_FALSE(window_controller()->GetChildWebContents());
}

// Navigation by the pip window to a new document should close the pip window.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CloseOnPictureInPictureNavigationToNewDocument) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "navigateInDocumentPipWindow('http://media/"
                         "picture_in_picture/blank.html');"));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_controller()->GetChildWebContents());
}

// Navigation within the pip window's document should not close the pip window.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       DoNotCloseOnPictureInPictureNavigationInsideDocument) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "navigateInDocumentPipWindow('#top');"));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_controller()->GetChildWebContents());
}

// Refreshing the pip window's document should close the pip window.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CloseOnPictureInPictureRefresh) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents, "refreshInDocumentPipWindow();"));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_controller()->GetChildWebContents());
}

// Explicitly navigating to about:blank should close the pip window.
// Regression test for https://crbug.com/1413919.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CloseOnPictureInPictureNavigatedToAboutBlank) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "navigateInDocumentPipWindow('about:blank');"));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_controller()->GetChildWebContents());
}

// Explicitly navigating to the empty string should close the pip window.
// Regression test for https://crbug.com/1413919.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CloseOnPictureInPictureNavigatedToEmptyString) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true,
            EvalJs(active_web_contents, "navigateInDocumentPipWindow('');"));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_controller()->GetChildWebContents());
}

// Adding a script to the popup window should not crash.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       AddScriptToPictureInPictureWindow) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(true, EvalJs(active_web_contents,
                         "addScriptToPictureInPictureWindow();"));
  base::RunLoop().RunUntilIdle();
}

// Window controller bounds should be same as the web content bounds.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CheckWindowBoundsSameAsWebContents) {
  LoadTabAndEnterPictureInPicture(browser());
  auto* web_contents = window_controller()->GetChildWebContents();
  ASSERT_TRUE(web_contents);

  EXPECT_EQ(web_contents->GetContainerBounds(),
            window_controller()->GetWindowBounds());
}

#if BUILDFLAG(IS_WIN)
// Back to tab button (PictureInPictureBrowserFrameView) is not available
// in Windows yet.
#define MAYBE_FocusInitiatorWhenBackToTab DISABLED_FocusInitiatorWhenBackToTab
#else
#define MAYBE_FocusInitiatorWhenBackToTab FocusInitiatorWhenBackToTab
#endif
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MAYBE_FocusInitiatorWhenBackToTab) {
  LoadTabAndEnterPictureInPicture(browser());
  auto* opener_web_contents = window_controller()->GetWebContents();

  // Open a new tab.
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureDocumentPipPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_NE(browser()->tab_strip_model()->GetActiveWebContents(),
            opener_web_contents);

  auto* web_contents = window_controller()->GetChildWebContents();
  ASSERT_TRUE(web_contents);

  auto* browser_view = static_cast<BrowserView*>(
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents));
  ASSERT_TRUE(browser_view);

  auto* pip_frame_view = static_cast<PictureInPictureBrowserFrameView*>(
      browser_view->frame()->GetFrameView());
  ASSERT_TRUE(pip_frame_view);

  ClickButton(
      views::Button::AsButton(pip_frame_view->GetBackToTabButtonForTesting()));
  EXPECT_FALSE(window_controller()->GetChildWebContents());
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            opener_web_contents);
}

// Make sure that document PiP fails without a secure context.
// TODO(crbug.com/40842257): Consider replacing this with a web platform test.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       RequiresSecureContext) {
  GURL test_page_url("http://media/picture-in-picture/blank.html");
  ASSERT_FALSE(network::IsUrlPotentiallyTrustworthy(test_page_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_page_url));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  // In an insecure context, there should not be a method.
  EXPECT_EQ(false, EvalJs(active_web_contents,
                          "'documentPictureInPicture' in window"));
}

// Make sure that inner bounds of document PiP windows are not smaller than the
// allowed minimum size.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MinimumWindowInnerBounds) {
  LoadTabAndEnterPictureInPicture(browser(), gfx::Size(100, 20));

  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);

  auto* browser_view = static_cast<BrowserView*>(
      BrowserWindow::FindBrowserWindowWithWebContents(pip_web_contents));
  EXPECT_EQ(PictureInPictureWindowManager::GetMinimumInnerWindowSize(),
            browser_view->GetContentsSize());
}

// Make sure that outer bounds of document PiP windows do not exceed the allowed
// maximum size.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MaximumWindowOuterBounds) {
  const BrowserWindow* const browser_window = browser()->window();
  const gfx::NativeWindow native_window = browser_window->GetNativeWindow();
  const display::Screen* const screen = display::Screen::GetScreen();
  const display::Display display =
      screen->GetDisplayNearestWindow(native_window);
  const gfx::Size maximum_window_size =
      PictureInPictureWindowManager::GetMaximumWindowSize(display);

  // Attempt to create a Document PiP window with size greater than the maximum
  // window size.
  LoadTabAndEnterPictureInPicture(browser(),
                                  maximum_window_size + gfx::Size(1000, 2000));

  // Confirm that the size of the outer window bounds are equal to the maximum
  // window size.
  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);

  auto* browser_view = static_cast<BrowserView*>(
      BrowserWindow::FindBrowserWindowWithWebContents(pip_web_contents));
  ASSERT_EQ(maximum_window_size, browser_view->GetBounds().size());
}

// Context menu should not be shown when right clicking on a document picture in
// picture window title.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       ContextMenuIsDisabled) {
  LoadTabAndEnterPictureInPicture(browser());
  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);

  auto* browser_view = static_cast<BrowserView*>(
      BrowserWindow::FindBrowserWindowWithWebContents(pip_web_contents));
  auto* pip_frame_view = static_cast<PictureInPictureBrowserFrameView*>(
      browser_view->frame()->GetFrameView());

  // Get the document picture in picture window title and the location to be
  // clicked.
  views::Label* window_title = pip_frame_view->GetWindowTitleForTesting();
  const gfx::Point click_location =
      window_title->GetBoundsInScreen().CenterPoint();

  // Simulate a click on the document picture in picture window title, and
  // verify that the context menu is not shown.
  pip_frame_view->frame()->ShowContextMenuForViewImpl(
      window_title, click_location, ui::MenuSourceType::MENU_SOURCE_MOUSE);

  EXPECT_EQ(false, pip_frame_view->frame()->IsMenuRunnerRunningForTesting());
}

IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       WindowClosesEvenIfDisconnectedFromWindowManager) {
  // Rarely, `PictureInPictureWindowManager` fails to close the pip browser
  // window. It's unclear why this happens, but the pip browser frame should
  // fall back and close itself.
  LoadTabAndEnterPictureInPicture(browser());
  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);
  auto* browser_view = static_cast<BrowserView*>(
      BrowserWindow::FindBrowserWindowWithWebContents(pip_web_contents));
  auto* pip_frame_view = static_cast<PictureInPictureBrowserFrameView*>(
      browser_view->frame()->GetFrameView());
  // Make the window manager forget about the window controller, which will
  // cause it to fail to close the window when asked.
  PictureInPictureWindowManager::GetInstance()
      ->set_window_controller_for_testing(nullptr);
  ClickButton(
      views::Button::AsButton(pip_frame_view->GetCloseButtonForTesting()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_controller()->GetChildWebContents());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verify that it is possible to resize a document picture in picture window
// using the resize outside bound in ChromeOS ASH.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       CanResizeUsingOutsideBounds) {
  // Attempt to create a Document PiP window with the minimum window size.
  LoadTabAndEnterPictureInPicture(
      browser(), PictureInPictureWindowManager::GetMinimumInnerWindowSize());
  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);

  // Get a point within the resize outside bounds.
  auto* browser_view = static_cast<BrowserView*>(
      BrowserWindow::FindBrowserWindowWithWebContents(pip_web_contents));
  const auto left_center_point = browser_view->GetBounds().left_center();
  const auto resize_outside_bound_point =
      gfx::Point(left_center_point.x() - chromeos::kResizeInsideBoundsSize -
                     chromeos::kResizeOutsideBoundsSize / 2,
                 left_center_point.y());

  // Perform a click on the left resize outside bound, followed by a drag to the
  // left.
  aura::Window* window = browser_view->GetNativeWindow();
  const auto initial_window_size = window->GetBoundsInScreen().size();
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  event_generator.set_current_screen_location(resize_outside_bound_point);
  const int drag_distance = 10;
  event_generator.DragMouseBy(-drag_distance, 0);

  // Verify that the rezise took place.
  const auto expected_size = initial_window_size + gfx::Size(drag_distance, 0);
  ASSERT_EQ(expected_size, window->GetBoundsInScreen().size());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       WindowBoundsAreCached) {
  // Create a Document PiP window with any size.  We want to be sure that this
  // fits in the display comfortably.
  const gfx::Size size(400, 410);
  auto display = display::Display::GetDefaultDisplay();
  ASSERT_LE(size.width(), display.size().width() * 0.8);
  ASSERT_LE(size.height(), display.size().height() * 0.8);

  LoadTabAndEnterPictureInPicture(browser(), size);
  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);
  auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      pip_web_contents->GetTopLevelNativeWindow());
  ASSERT_TRUE(browser_view);

  // Wait for the window origin location to be set. This is needed to eliminate
  // test flakiness.
  CheckOriginSet(browser_view);

  // Get the bounds, which might not be the same size we asked for.
  const gfx::Rect original_window_bounds = browser_view->GetBounds();
  gfx::Rect window_bounds = original_window_bounds;

  // Move the window and change the size.  Make sure that it stays on-screen.
  // Also make sure it gets smaller, in case one of the bounds was clipped to
  // the display size.
  ASSERT_GE(window_bounds.x(), 10);
  ASSERT_GE(window_bounds.y(), 10);
  window_bounds -= gfx::Vector2d(10, 10);
#if !BUILDFLAG(IS_LINUX)
  // During resize, aura on linux posts delayed work to update the size:
  //
  // PictureInPictureWindowManager::UpdateCachedBounds()
  // views::Widget::OnNativeWidgetSizeChanged()
  // views::DesktopNativeWidgetAura::OnHostResized()
  // aura::WindowTreeHost::OnHostResizedInPixels()
  // aura::WindowTreeHostPlatform::OnBoundsChanged()
  // BrowserDesktopWindowTreeHostLinux::OnBoundsChanged()
  // ui::X11Window::NotifyBoundsChanged()
  // ui::X11Window::DelayedResize()
  // base::internal::CancelableCallbackImpl<>::ForwardOnce<>()
  //
  // This starts when the window is opened, so the posted work tries to set the
  // size to what we requested above.
  //
  // If the test is fast enough to update the bounds before the original posted
  // work completes, then the posted work will cause a cache update back to the
  // original size.  Luckily, the position isn't updated so we can at least make
  // sure that the cache is doing something, even on linux.
  //
  // On other platforms, we change the size here to test both.
  window_bounds.set_size(
      {window_bounds.width() - 10, window_bounds.height() - 10});
#endif  // !BUILDFLAG(IS_LINUX)

  browser_view->SetBounds(window_bounds);

  // Wait for the bounds to change.  It would be nice if we didn't need to
  // explicitly create a variable.  A temporary would do it, but it seems like
  // temporaries and anonymous types don't work well together.
  struct : TestObserverWaiter<views::WidgetObserver> {
    void OnWidgetBoundsChanged(views::Widget*,
                               const gfx::Rect& bounds) override {
      CheckCondition();
    }
  } waiter;
  waiter.Wait(browser_view->GetWidget(),
              base::BindRepeating(
                  [](const gfx::Rect& expected, BrowserView* browser_view) {
                    return expected == browser_view->GetBounds();
                  },
                  window_bounds, browser_view));

  // Open a new pip window, and make sure that it's not in the same place it was
  // last time.
  LoadTabAndEnterPictureInPicture(browser(), size);
  auto* pip_web_contents_2 = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents_2);
  WaitForPageLoad(pip_web_contents_2);
  auto* browser_view_2 = BrowserView::GetBrowserViewForNativeWindow(
      pip_web_contents_2->GetTopLevelNativeWindow());

  // The new window should match the bounds we set for the old one, which differ
  // from the default.
  EXPECT_EQ(browser_view_2->GetBounds(), window_bounds);

  // Close the window and re-open it, but request no cache this time.  This
  // should revert it to its original bounds.
  LoadTabAndEnterPictureInPicture(browser(), size,
                                  /*prefer_initial_window_placement=*/true);
  auto* pip_web_contents_3 = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents_3);
  WaitForPageLoad(pip_web_contents_3);
  auto* browser_view_3 = BrowserView::GetBrowserViewForNativeWindow(
      pip_web_contents_3->GetTopLevelNativeWindow());
  EXPECT_EQ(browser_view_3->GetBounds(), original_window_bounds);
}

INSTANTIATE_TEST_SUITE_P(WindowSizes,
                         DocumentPictureInPictureWindowControllerBrowserTest,
                         testing::Values(gfx::Size(1, 1),
                                         gfx::Size(22, 22),
                                         gfx::Size(300, 300),
                                         gfx::Size(500, 500),
                                         gfx::Size(250, 670)));

#if BUILDFLAG(IS_LINUX)
// TODO(crbug.com/40923223): Fix and re-enable this test for Linux.
// This test is flaky on Linux, sometimes the window origin is not updated
// before the test harness timeout.
#define MAYBE_VerifyWindowMargins DISABLED_VerifyWindowMargins
#else
#define MAYBE_VerifyWindowMargins VerifyWindowMargins
#endif
// Test that the document PiP window margins are correct.
IN_PROC_BROWSER_TEST_P(DocumentPictureInPictureWindowControllerBrowserTest,
                       MAYBE_VerifyWindowMargins) {
  const BrowserWindow* const browser_window = browser()->window();
  const gfx::NativeWindow native_window = browser_window->GetNativeWindow();
  const display::Screen* const screen = display::Screen::GetScreen();
  const display::Display display =
      screen->GetDisplayNearestWindow(native_window);

  // Create a Document PiP window with the given size.
  LoadTabAndEnterPictureInPicture(browser(), GetParam());
  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);
  auto* browser_view = static_cast<BrowserView*>(
      BrowserWindow::FindBrowserWindowWithWebContents(pip_web_contents));

  // Wait for the window origin location to be set. This is needed to eliminate
  // test flakiness.
  CheckOriginSet(browser_view);

  // Make sure that the right and bottom window margins are equal.
  gfx::Rect window_bounds = browser_view->GetBounds();
  gfx::Rect work_area = display.work_area();
  int window_diff_width = work_area.right() - window_bounds.width();
  int window_diff_height = work_area.bottom() - window_bounds.height();
  ASSERT_EQ(work_area.right() - window_bounds.right(),
            work_area.bottom() - window_bounds.bottom());

  // Make sure that the right and bottom window margins have distance of 2% the
  // average of the two window size differences.
  int buffer = (window_diff_width + window_diff_height) / 2 * 0.02;
  gfx::Point expected_origin =
      gfx::Point(window_diff_width - buffer, window_diff_height - buffer);
  ASSERT_EQ(window_bounds.origin(), expected_origin);
}

IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       DoNotDeferMediaLoadIfWindowOpened) {
  LoadTabAndEnterPictureInPicture(browser());
  auto* opener_web_contents = window_controller()->GetWebContents();

  // Open a new foreground tab.
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureDocumentPipPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_NE(browser()->tab_strip_model()->GetActiveWebContents(),
            opener_web_contents);

  ASSERT_EQ(true, EvalJs(opener_web_contents, "loadAndPlayVideo();"));
}

IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       MatchMediaQuery) {
  LoadTabAndEnterPictureInPicture(browser());
  auto* opener_web_contents = window_controller()->GetWebContents();
  auto* web_contents = window_controller()->GetChildWebContents();
  ASSERT_TRUE(opener_web_contents);
  ASSERT_TRUE(web_contents);

  std::string match_media_picture_in_picture =
      "window.matchMedia('(display-mode: picture-in-picture)').matches;";
  ASSERT_FALSE(EvalJs(opener_web_contents, match_media_picture_in_picture)
                   .ExtractBool());
  ASSERT_TRUE(
      EvalJs(web_contents, match_media_picture_in_picture).ExtractBool());
}

// Make sure that inner bounds of document PiP windows match the requested size.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       InnerBoundsMatchRequest) {
  constexpr auto size = gfx::Size(400, 450);
  LoadTabAndEnterPictureInPicture(browser(), size);

  auto* pip_web_contents = window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  WaitForPageLoad(pip_web_contents);

  auto* pip_browser = chrome::FindBrowserWithTab(pip_web_contents);
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(pip_browser);
  EXPECT_EQ(size, browser_view->GetContentsSize());
}

// When `window.open()` is called from a picture-in-picture window, it must lose
// focus to the newly opened window to prevent multiple popunders from opening
// when a user types multiple keys in a picture-in-picture window.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPictureWindowControllerBrowserTest,
                       WindowOpenLosesFocus) {
  LoadTabAndEnterPictureInPicture(browser());
  auto* web_contents = window_controller()->GetChildWebContents();
  ASSERT_TRUE(web_contents);
  views::Widget* pip_widget = views::Widget::GetWidgetForNativeWindow(
      web_contents->GetTopLevelNativeWindow());
  ASSERT_TRUE(pip_widget);
  WidgetActivationWaiter widget_activation_waiter(pip_widget);

  // Ensure that the picture-in-picture window has system focus.
  pip_widget->Activate();
  ASSERT_TRUE(widget_activation_waiter.WaitForActivationState(true));

  // Call `window.open()` to open a popup window.
  EXPECT_TRUE(
      ExecJs(web_contents,
             "window.open('about:blank', '_blank', 'width=300,height=300');"));

  // The picture-in-picture window should no longer have system focus.
  EXPECT_TRUE(widget_activation_waiter.WaitForActivationState(false));
}
