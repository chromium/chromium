// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_TEST_PRINT_VIEW_MANAGER_FOR_REQUEST_PREVIEW_H_
#define CHROME_BROWSER_PRINTING_TEST_PRINT_VIEW_MANAGER_FOR_REQUEST_PREVIEW_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "components/printing/common/print.mojom.h"

namespace content {
class WebContents;
}

namespace printing {

// A helper class for testing PrintViewManager::RequestPrintPreview().
// When printing::StartPrint is called in a browser test, the browser process
// communicates with a renderer process using mojo API. The rendered should call
// PrintViewManager::RequestPrintPreview(). However, due to asynchronous
// execution, it is possible that the test fixture will end before this call
// happens. TestPrintViewManagerForRequestPreview helps to set up
// synchronisation using base::RunLoop.
class TestPrintViewManagerForRequestPreview : public PrintViewManager {
 public:
  // Create TestPrintViewManagerForRequestPreview with
  // PrintViewManager::UserDataKey() so that PrintViewManager::FromWebContents()
  // in printing path returns TestPrintViewManagerForRequestPreview*.
  static void CreateForWebContents(content::WebContents* web_contents);

  explicit TestPrintViewManagerForRequestPreview(
      content::WebContents* web_contents);
  TestPrintViewManagerForRequestPreview(
      const TestPrintViewManagerForRequestPreview&) = delete;
  TestPrintViewManagerForRequestPreview& operator=(
      const TestPrintViewManagerForRequestPreview&) = delete;
  ~TestPrintViewManagerForRequestPreview() override;

  static TestPrintViewManagerForRequestPreview* FromWebContents(
      content::WebContents* web_contents);

  void set_quit_closure(base::OnceClosure quit_closure);

 private:
  // printing::mojom::PrintManagerHost:
  void RequestPrintPreview(mojom::RequestPrintPreviewParamsPtr params) override;

  base::OnceClosure quit_closure_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_TEST_PRINT_VIEW_MANAGER_FOR_REQUEST_PREVIEW_H_
