// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BROWSERTEST_H_
#define CHROME_BROWSER_PRINTING_PRINT_BROWSERTEST_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/printing/browser_printing_context_factory_for_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/printing/common/print.mojom-test-utils.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/backend/test_print_backend.h"
#include "printing/print_settings.h"

namespace printing {

class TestPrintRenderFrame;

class PrintBrowserTest : public InProcessBrowserTest {
 public:
  enum class InvokePrintMethod {
    // Browser starts print (e.g., like CTRL+P keyboard shortcut).
    kStartPrint,
    // Initiate printing for from JavaScript.
    kWindowDotPrint,
  };

  struct PrintParams {
    bool print_only_selection = false;
    int pages_per_sheet = 1;
    InvokePrintMethod invoke_method = InvokePrintMethod::kStartPrint;
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
#if BUILDFLAG(IS_WIN)
  void SetPrinterLanguageTypeForSubsequentContexts(
      mojom::PrinterLanguageType printer_language_type);
#endif
  void SetUserSettingsPageRangesForSubsequentContext(
      const PageRanges& page_ranges);
  void SetNewDocumentJobId(int job_id);

  void PrintAndWaitUntilPreviewIsReady();
  // Returns the Print Preview dialog.
  content::WebContents* PrintAndWaitUntilPreviewIsReady(
      const PrintParams& params);
  content::WebContents* PrintAndWaitUntilPreviewIsReadyAndLoaded();
  content::WebContents* PrintAndWaitUntilPreviewIsReadyAndLoaded(
      const PrintParams& params);

  void SetNumExpectedMessages(unsigned int num);
  void ResetNumReceivedMessages();
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
  // Support class for making renderer process terminate during tests.
  class KillPrintRenderFrame
      : public mojom::PrintRenderFrameInterceptorForTesting {
   public:
    explicit KillPrintRenderFrame(content::RenderProcessHost* rph);
    KillPrintRenderFrame(content::RenderProcessHost* rph,
                         mojom::PrintRenderFrame* print_render_frame);
    ~KillPrintRenderFrame() override;

    void OverrideBinderForTesting(content::RenderFrameHost* render_frame_host);

    void KillRenderProcess(int document_cookie,
                           mojom::DidPrintContentParamsPtr param,
                           PrintFrameContentCallback callback) const;

    void Bind(mojo::ScopedInterfaceEndpointHandle handle);

    // mojom::PrintRenderFrameInterceptorForTesting
    mojom::PrintRenderFrame* GetForwardingInterface() override;
    void PrintFrameContent(mojom::PrintFrameContentParamsPtr params,
                           PrintFrameContentCallback callback) override;

   private:
    const raw_ptr<content::RenderProcessHost> rph_;
    const raw_ptr<mojom::PrintRenderFrame> print_render_frame_;
    mojo::AssociatedReceiver<mojom::PrintRenderFrame> receiver_{this};
  };

  // Initialize a run loop for a testing sequence.
  void PrepareRunloop();

  void OnNewDocument(
#if BUILDFLAG(IS_MAC)
      bool destination_is_preview,
#endif
      const PrintSettings& settings);

  TestPrintBackend* test_print_backend() { return test_print_backend_.get(); }

  BrowserPrintingContextFactoryForTest* test_printing_context_factory() {
    return &test_printing_context_factory_;
  }

  void set_rendered_page_count(uint32_t page_count) {
    rendered_page_count_ = page_count;
  }

  int new_document_called_count() const { return new_document_called_count_; }

  const std::optional<PrintSettings>& document_print_settings() const {
    return document_print_settings_;
  }

#if BUILDFLAG(IS_MAC)
  bool destination_is_preview() const { return destination_is_preview_; }
#endif

 private:
  // Helper to bounce worker thread callbacks onto PrintBrowserTest's callback
  // equivalent on the UI thread.
  class WorkerHelper : public base::RefCountedThreadSafe<WorkerHelper> {
   public:
    explicit WorkerHelper(base::WeakPtr<PrintBrowserTest> owner);

    void OnNewDocument(
#if BUILDFLAG(IS_MAC)
        bool destination_is_preview,
#endif
        const PrintSettings& settings);

   private:
    friend class base::RefCountedThreadSafe<WorkerHelper>;
    ~WorkerHelper();

    // Only accessed on the UI thread.
    const base::WeakPtr<PrintBrowserTest> owner_;
  };

  content::WebContents* PrintAndWaitUntilPreviewIsReadyAndMaybeLoaded(
      const PrintParams& params,
      bool wait_for_loaded);
  TestPrintRenderFrame* GetFrameContent(content::RenderFrameHost* host) const;
  void OverrideBinderForTesting(content::RenderFrameHost* render_frame_host);
  void ShowPrintErrorDialog();

  int new_document_called_count_ = 0;
  std::optional<PrintSettings> document_print_settings_;
#if BUILDFLAG(IS_MAC)
  bool destination_is_preview_ = false;
#endif
  uint32_t error_dialog_shown_count_ = 0;
  uint32_t rendered_page_count_ = 0;
  unsigned int num_expected_messages_;
  unsigned int num_received_messages_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure quit_callback_;
  mojo::AssociatedRemote<mojom::PrintRenderFrame> remote_;
  std::map<content::RenderFrameHost*, std::unique_ptr<TestPrintRenderFrame>>
      frame_content_;
  scoped_refptr<TestPrintBackend> test_print_backend_;
  BrowserPrintingContextFactoryForTest test_printing_context_factory_;
  scoped_refptr<WorkerHelper> worker_helper_;
  base::WeakPtrFactory<PrintBrowserTest> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BROWSERTEST_H_
