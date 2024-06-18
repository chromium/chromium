// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_TEST_PRINT_PREVIEW_OBSERVER_H_
#define CHROME_BROWSER_PRINTING_TEST_PRINT_PREVIEW_OBSERVER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_WIN)
#include "printing/printing_utils.h"
#endif

namespace content {
class WebContents;
}

namespace printing {

class TestPrintPreviewObserver : PrintPreviewUI::TestDelegate {
 public:
  explicit TestPrintPreviewObserver(bool wait_for_loaded);
  TestPrintPreviewObserver(bool wait_for_loaded, int pages_per_sheet);
  TestPrintPreviewObserver(const TestPrintPreviewObserver&) = delete;
  TestPrintPreviewObserver& operator=(const TestPrintPreviewObserver&) = delete;
  ~TestPrintPreviewObserver() override;

  // Tests that use PrintPreviewObserver must call
  // WaitUntilPreviewIsReady*() exactly once.
  [[nodiscard]] content::WebContents*
  WaitUntilPreviewIsReadyAndReturnPreviewDialog();

  // Wrapper for WaitUntilPreviewIsReadyAndReturnPreviewDialog() provided for
  // convenience for callers that do not need the returned result.
  void WaitUntilPreviewIsReady();

  uint32_t rendered_page_count() const { return rendered_page_count_; }

#if BUILDFLAG(IS_WIN)
  std::optional<DocumentDataType> last_document_composite_data_type() const {
    return last_document_composite_data_type_;
  }
#endif

 private:
  void EnsureWaitForLoaded();

  // PrintPreviewUI::TestDelegate:
  void DidGetPreviewPageCount(uint32_t page_count) override;
  void DidRenderPreviewPage(content::WebContents* preview_dialog) override;
  void PreviewDocumentReady(content::WebContents* preview_dialog,
                            base::span<const uint8_t> data) override;

  std::optional<content::DOMMessageQueue> queue_;

  // Rendered pages are provided after N-up processing, which will be different
  // from the count provided to `DidGetPreviewPageCount()` when
  // `pages_per_sheet_` is larger than one.
  const int pages_per_sheet_;
  uint32_t expected_rendered_page_count_ = 1;
  uint32_t rendered_page_count_ = 0;

#if BUILDFLAG(IS_WIN)
  std::optional<DocumentDataType> last_document_composite_data_type_;
#endif

  const bool wait_for_loaded_;
  raw_ptr<content::WebContents, FlakyDanglingUntriaged> preview_dialog_ =
      nullptr;
  base::OnceClosure quit_closure_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_TEST_PRINT_PREVIEW_OBSERVER_H_
