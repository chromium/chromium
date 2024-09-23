// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/size.h"

DocumentPictureInPictureMixinTestBase::DocumentPictureInPictureMixinTestBase(
    InProcessBrowserTestMixinHost* mixin_host)
    : InProcessBrowserTestMixin(mixin_host) {}

DocumentPictureInPictureMixinTestBase::
    ~DocumentPictureInPictureMixinTestBase() = default;

void DocumentPictureInPictureMixinTestBase::PostRunTestOnMainThread() {
  pip_window_controller_ = nullptr;
}

void DocumentPictureInPictureMixinTestBase::
    NavigateToURLAndEnterPictureInPicture(Browser* browser,
                                          const gfx::Size& window_size) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, GetPictureInPictureURL()));

  content::WebContents* active_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  SetUpWindowController(active_web_contents);

  const std::string script = base::StrCat(
      {"createDocumentPipWindow({width:",
       base::NumberToString(window_size.width()),
       ",height:", base::NumberToString(window_size.height()), "})"});
  ASSERT_EQ(true, content::EvalJs(active_web_contents, script));
  ASSERT_TRUE(window_controller() != nullptr);
  // Especially on Linux, this isn't synchronous.
  ui_test_utils::CheckWaiter(
      base::BindRepeating(&content::RenderWidgetHostView::IsShowing,
                          base::Unretained(GetRenderWidgetHostView())),
      true, base::Seconds(30))
      .Wait();
  ASSERT_TRUE(GetRenderWidgetHostView()->IsShowing());
}

void DocumentPictureInPictureMixinTestBase::WaitForPageLoad(
    content::WebContents* contents) {
  EXPECT_TRUE(WaitForLoadStop(contents));
  EXPECT_TRUE(WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
}

GURL DocumentPictureInPictureMixinTestBase::GetPictureInPictureURL() const {
  return ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureDocumentPipPage));
}

bool DocumentPictureInPictureMixinTestBase::AwaitPipWindowClosedSuccessfully() {
  auto* render_widget_host_view = GetRenderWidgetHostView();
  if (!render_widget_host_view) {
    return true;
  }
  ui_test_utils::CheckWaiter(
      base::BindRepeating(&content::RenderWidgetHostView::IsShowing,
                          base::Unretained(render_widget_host_view)),
      false, base::Seconds(30))
      .Wait();

  return (GetRenderWidgetHostView() == nullptr);
}

content::RenderWidgetHostView*
DocumentPictureInPictureMixinTestBase::GetRenderWidgetHostView() {
  if (!window_controller()) {
    return nullptr;
  }

  if (auto* web_contents = window_controller()->GetChildWebContents()) {
    return web_contents->GetRenderWidgetHostView();
  }

  return nullptr;
}

void DocumentPictureInPictureMixinTestBase::SetUpWindowController(
    content::WebContents* web_contents) {
  pip_window_controller_ = content::PictureInPictureWindowController::
      GetOrCreateDocumentPictureInPictureController(web_contents);
}
