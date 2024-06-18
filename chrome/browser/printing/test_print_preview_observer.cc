// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/test_print_preview_observer.h"

#include <tuple>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace printing {

TestPrintPreviewObserver::TestPrintPreviewObserver(bool wait_for_loaded)
    : TestPrintPreviewObserver(wait_for_loaded, /*pages_per_sheet=*/1) {}

TestPrintPreviewObserver::TestPrintPreviewObserver(bool wait_for_loaded,
                                                   int pages_per_sheet)
    : pages_per_sheet_(pages_per_sheet), wait_for_loaded_(wait_for_loaded) {
  PrintPreviewUI::SetDelegateForTesting(this);
}

TestPrintPreviewObserver::~TestPrintPreviewObserver() {
  PrintPreviewUI::SetDelegateForTesting(nullptr);
}

[[nodiscard]] content::WebContents*
TestPrintPreviewObserver::WaitUntilPreviewIsReadyAndReturnPreviewDialog() {
  if (rendered_page_count_ < expected_rendered_page_count_) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();

    if (queue_.has_value()) {
      std::string message;
      EXPECT_TRUE(queue_->WaitForMessage(&message));
      EXPECT_EQ("\"success\"", message);
    }
  }

  // Grab and reset `preview_dialog_` to avoid potential dangling pointers.
  content::WebContents* dialog = preview_dialog_;
  preview_dialog_ = nullptr;
  return dialog;
}

// Wrapper for WaitUntilPreviewIsReadyAndReturnPreviewDialog() provided for
// convenience for callers that do not need the returned result.
void TestPrintPreviewObserver::WaitUntilPreviewIsReady() {
  std::ignore = WaitUntilPreviewIsReadyAndReturnPreviewDialog();
}

void TestPrintPreviewObserver::EnsureWaitForLoaded() {
  if (queue_.has_value()) {
    // Have already added event listener.
    return;
  }
  // Instantiate `queue_` to listen for messages in `preview_dialog_`.
  queue_.emplace(preview_dialog_);
  content::ExecuteScriptAsync(
      preview_dialog_.get(),
      "window.addEventListener('message', event => {"
      "  if (event.data.type === 'documentLoaded') {"
      "    domAutomationController.send(event.data.load_state);"
      "  }"
      "});");
}

void TestPrintPreviewObserver::DidGetPreviewPageCount(uint32_t page_count) {
  rendered_page_count_ = 0;

  // `page_count` is the number of pages to be generated but doesn't take
  // N-up into consideration.  Since `DidRenderPreviewPage()` is called after
  // any N-up processing is performed, determine the number of times that
  // function is expected to be called.
  expected_rendered_page_count_ =
      (page_count + pages_per_sheet_ - 1) / pages_per_sheet_;
}

void TestPrintPreviewObserver::DidRenderPreviewPage(
    content::WebContents* preview_dialog) {
  ++rendered_page_count_;
  DVLOG(2) << "Rendered preview page " << rendered_page_count_
           << " of a total expected " << expected_rendered_page_count_;
  CHECK_LE(rendered_page_count_, expected_rendered_page_count_);
  if (rendered_page_count_ == expected_rendered_page_count_ && quit_closure_) {
    std::move(quit_closure_).Run();
    preview_dialog_ = preview_dialog;

    if (wait_for_loaded_) {
      EnsureWaitForLoaded();
    }
  }
}

void TestPrintPreviewObserver::PreviewDocumentReady(
    content::WebContents* preview_dialog,
    base::span<const uint8_t> data) {
#if BUILDFLAG(IS_WIN)
  last_document_composite_data_type_ = DetermineDocumentDataType(data);
#endif

  // This runs after `DidGetPreviewPageCount()` for modifiable content, but is
  // otherwise the only notification.
  if (quit_closure_) {
    std::move(quit_closure_).Run();
    preview_dialog_ = preview_dialog;

    if (wait_for_loaded_) {
      EnsureWaitForLoaded();
    }
  }
}

}  // namespace printing
