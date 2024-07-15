// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_MIXIN_TEST_BASE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_MIXIN_TEST_BASE_H_

#include "base/files/file_path.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class DocumentPictureInPictureWindowController;
class RenderWidgetHostView;
class WebContents;
}  // namespace content

class GURL;
class Browser;

// DocumentPictureInPictureMixinTestBase is test base harness that can inherit
// any InProcessBrowserTest to be used for writing basic automated tests for
// document based picture in picture windows. This includes utilities that
// trigger and wait for a picture in picture window to show up or close,
// encapsulating asynchronous workflows.
class DocumentPictureInPictureMixinTestBase : public InProcessBrowserTestMixin {
 public:
  inline static constexpr base::FilePath::CharType
      kPictureInPictureDocumentPipPage[] =
          FILE_PATH_LITERAL("media/picture-in-picture/document-pip.html");

  explicit DocumentPictureInPictureMixinTestBase(
      InProcessBrowserTestMixinHost* mixin_host);
  ~DocumentPictureInPictureMixinTestBase() override;

  void PostRunTestOnMainThread() override;

  void NavigateToURLAndEnterPictureInPicture(
      Browser* browser,
      const gfx::Size& window_size = gfx::Size(500, 500));

  void WaitForPageLoad(content::WebContents* contents);

  GURL GetPictureInPictureURL() const;
  bool AwaitPipWindowClosedSuccessfully();
  content::DocumentPictureInPictureWindowController* window_controller() {
    return pip_window_controller_;
  }

 private:
  content::RenderWidgetHostView* GetRenderWidgetHostView();
  void SetUpWindowController(content::WebContents* web_contents);

  raw_ptr<content::DocumentPictureInPictureWindowController>
      pip_window_controller_ = nullptr;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_MIXIN_TEST_BASE_H_
