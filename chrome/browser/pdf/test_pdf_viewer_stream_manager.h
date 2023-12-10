// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_TEST_PDF_VIEWER_STREAM_MANAGER_H_
#define CHROME_BROWSER_PDF_TEST_PDF_VIEWER_STREAM_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace pdf {

class TestPdfViewerStreamManager : public PdfViewerStreamManager {
 public:
  // Prefer using this over the constructor so that this instance is used for
  // PDF loads.
  static TestPdfViewerStreamManager* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestPdfViewerStreamManager(content::WebContents* contents);
  TestPdfViewerStreamManager(const TestPdfViewerStreamManager&) = delete;
  TestPdfViewerStreamManager& operator=(const TestPdfViewerStreamManager&) =
      delete;
  ~TestPdfViewerStreamManager() override;

  // WebContentsObserver overrides.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Wait until the PDF has finished loading.
  void WaitUntilPdfLoaded();

 private:
  base::OnceClosure on_pdf_loaded_;
};

}  // namespace pdf

#endif  // CHROME_BROWSER_PDF_TEST_PDF_VIEWER_STREAM_MANAGER_H_
