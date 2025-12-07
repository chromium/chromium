// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/widget/widget.h"

namespace {

const base::FilePath::CharType kPictureInPictureDocumentPipPage[] =
    FILE_PATH_LITERAL(
        "media/picture-in-picture/documen_picture_in_picture_pixel_test.html");

class DocumentPictureInPicturePixelTest : public UiBrowserTest,
                                          public content::WebContentsObserver {
 public:
  DocumentPictureInPicturePixelTest() = default;

  DocumentPictureInPicturePixelTest(const DocumentPictureInPicturePixelTest&) =
      delete;
  DocumentPictureInPicturePixelTest& operator=(
      const DocumentPictureInPicturePixelTest&) = delete;

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
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kDocumentPictureInPictureAPI);

    // Disable animation for stability.
    animation_duration_ =
        std::make_unique<gfx::ScopedAnimationDurationScaleMode>(
            gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    InProcessBrowserTest::SetUp();
  }

  void SetUpWindowController(content::WebContents* web_contents) {
    pip_window_controller_ = content::PictureInPictureWindowController::
        GetOrCreateDocumentPictureInPictureController(web_contents);

    content::WebContentsObserver::Observe(web_contents);
  }

  content::DocumentPictureInPictureWindowController* window_controller() {
    return pip_window_controller_;
  }

  content::RenderWidgetHostView* GetRenderWidgetHostView() {
    if (!window_controller()) {
      return nullptr;
    }

    if (auto* web_contents = window_controller()->GetChildWebContents()) {
      return web_contents->GetRenderWidgetHostView();
    }

    return nullptr;
  }

  void LoadTabAndEnterPictureInPicture(
      Browser* browser,
      const gfx::Size& window_size = gfx::Size(300, 300)) {
    GURL test_page_url = chrome_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kPictureInPictureDocumentPipPage));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_page_url));

    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    SetUpWindowController(active_web_contents);

    const std::string script = base::StrCat(
        {"createDocumentPipWindow({width:",
         base::NumberToString(window_size.width()),
         ",height:", base::NumberToString(window_size.height()), "})"});
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

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    if (pip_window_controller_) {
      pip_window_controller_ = nullptr;
    }
  }

  void WaitForPageLoad(content::WebContents* contents) {
    EXPECT_TRUE(WaitForLoadStop(contents));
    EXPECT_TRUE(WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
  }

  void WaitForPipWidgetActivation(content::WebContents* pip_web_contents) {
    auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(
        pip_web_contents->GetTopLevelNativeWindow());
    ASSERT_TRUE(browser_view);
    auto* pip_widget = browser_view->GetWidget();
    ASSERT_TRUE(pip_widget);
    views::test::WaitForWidgetActive(pip_widget, /*active=*/true);
  }

  void ShowUi(const std::string& name) override {
    LoadTabAndEnterPictureInPicture(browser());

    auto* pip_web_contents = window_controller()->GetChildWebContents();
    ASSERT_NE(nullptr, pip_web_contents);
    WaitForPageLoad(pip_web_contents);

    ASSERT_TRUE(GetRenderWidgetHostView());
    EXPECT_TRUE(GetRenderWidgetHostView()->IsShowing());

    WaitForPipWidgetActivation(pip_web_contents);
    EXPECT_TRUE(GetRenderWidgetHostView()->HasFocus());
  }

  bool VerifyUi() override {
    auto* browser_view = static_cast<BrowserView*>(
        BrowserWindow::FindBrowserWindowWithWebContents(
            window_controller()->GetChildWebContents()));
    auto* pip_frame_view = static_cast<PictureInPictureBrowserFrameView*>(
        browser_view->browser_widget()->GetFrameView());

    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(pip_frame_view, test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    ui_test_utils::WaitForBrowserToClose();
  }

 private:
 // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  raw_ptr<content::DocumentPictureInPictureWindowController>
      pip_window_controller_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Used to force a zero duration animation.
  std::unique_ptr<gfx::ScopedAnimationDurationScaleMode> animation_duration_;
};

}  // namespace

// Shows a blank document Picture-in-Picture window and verifies the UI.
IN_PROC_BROWSER_TEST_F(DocumentPictureInPicturePixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
