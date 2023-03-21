// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BROWSERTEST_H_
#define CHROME_BROWSER_PRINTING_PRINT_BROWSERTEST_H_

#include "chrome/test/base/in_process_browser_test.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/printing/browser_printing_context_factory_for_test.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/backend/test_print_backend.h"
#include "printing/print_settings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace printing {

class TestPrintRenderFrame;

class PrintBrowserTest : public InProcessBrowserTest {
 public:
  struct PrintParams {
    bool print_only_selection = false;
    int pages_per_sheet = 1;
  };

  PrintBrowserTest();
  ~PrintBrowserTest() override;

  // InProcessBrowserTest overrides:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void TearDown() override;

  void AddPrinter(const std::string& printer_name);
  void SetPrinterNameForSubsequentContexts(const std::string& printer_name);
  void PrintAndWaitUntilPreviewIsReady();
  void PrintAndWaitUntilPreviewIsReady(const PrintParams& params);
  void PrintAndWaitUntilPreviewIsReadyAndLoaded();
  void PrintAndWaitUntilPreviewIsReadyAndLoaded(const PrintParams& params);

  void SetNumExpectedMessages(unsigned int num);
  void WaitUntilCallbackReceived();
  void CheckForQuit();

  void CreateTestPrintRenderFrame(content::RenderFrameHost* frame_host,
                                  content::WebContents* web_contents);

  static mojom::PrintFrameContentParamsPtr GetDefaultPrintFrameParams();

  const mojo::AssociatedRemote<mojom::PrintRenderFrame>& GetPrintRenderFrame(
      content::RenderFrameHost* rfh);

  uint32_t rendered_page_count() const { return rendered_page_count_; }

  uint32_t error_dialog_shown_count() const {
    return error_dialog_shown_count_;
  }

 protected:
  TestPrintBackend* test_print_backend() { return test_print_backend_.get(); }

  BrowserPrintingContextFactoryForTest* test_printing_context_factory() {
    return &test_printing_context_factory_;
  }

  void set_rendered_page_count(uint32_t page_count) {
    rendered_page_count_ = page_count;
  }

  const absl::optional<PrintSettings>& document_print_settings() const {
    return test_printing_context_factory_.document_print_settings();
  }

 private:
  TestPrintRenderFrame* GetFrameContent(content::RenderFrameHost* host) const;
  void OverrideBinderForTesting(content::RenderFrameHost* render_frame_host);
  void ShowPrintErrorDialog();

  uint32_t error_dialog_shown_count_ = 0;
  uint32_t rendered_page_count_ = 0;
  unsigned int num_expected_messages_;
  unsigned int num_received_messages_;
  base::OnceClosure quit_callback_;
  mojo::AssociatedRemote<mojom::PrintRenderFrame> remote_;
  std::map<content::RenderFrameHost*, std::unique_ptr<TestPrintRenderFrame>>
      frame_content_;
  scoped_refptr<TestPrintBackend> test_print_backend_;
  BrowserPrintingContextFactoryForTest test_printing_context_factory_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BROWSERTEST_H_
