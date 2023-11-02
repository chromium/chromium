// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_TEST_PRINT_PREVIEW_DIALOG_CLONED_OBSERVER_H_
#define CHROME_BROWSER_PRINTING_TEST_PRINT_PREVIEW_DIALOG_CLONED_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace printing {

// A supplementary helper class for
// printing::TestPrintViewManagerForRequestPreview.
// It enables to set up TestPrintViewManagerForRequestPreview in browser tests
// instead of a real PrintViewManager.
class TestPrintPreviewDialogClonedObserver
    : public content::WebContentsObserver {
 public:
  explicit TestPrintPreviewDialogClonedObserver(content::WebContents* dialog);
  TestPrintPreviewDialogClonedObserver(
      const TestPrintPreviewDialogClonedObserver&) = delete;
  TestPrintPreviewDialogClonedObserver& operator=(
      const TestPrintPreviewDialogClonedObserver&) = delete;
  ~TestPrintPreviewDialogClonedObserver() override;

 private:
  // content::WebContentsObserver:
  void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) override;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_TEST_PRINT_PREVIEW_DIALOG_CLONED_OBSERVER_H_
