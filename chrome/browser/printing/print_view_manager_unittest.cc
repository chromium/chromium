// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/printing/test_print_job.h"
#include "chrome/browser/printing/test_printer_query.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace printing {

using PrintViewManagerTest = BrowserWithTestWindowTest;

class TestPrintViewManager : public PrintViewManagerBase {
 public:
  explicit TestPrintViewManager(content::WebContents* web_contents)
      : PrintViewManagerBase(web_contents) {}

  ~TestPrintViewManager() override {
    // Set this null here. Otherwise, the PrintViewManagerBase destructor will
    // try to de-register for notifications that were not registered for in
    // CreateNewPrintJob().
    print_job_ = nullptr;
  }

  // Mostly copied from PrintViewManager::PrintPreviewNow(). We can't override
  // PrintViewManager since it is a user data class.
  bool PrintPreviewNow(content::RenderFrameHost* rfh, bool has_selection) {
    // Don't print / print preview crashed tabs.
    if (IsCrashed())
      return false;

    mojo::AssociatedRemote<mojom::PrintRenderFrame> print_render_frame;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&print_render_frame);
    print_render_frame->InitiatePrintPreview(mojo::NullAssociatedRemote(),
                                             has_selection);
    return true;
  }

  // Getters for validating arguments to StartPdf...Conversion functions
  const gfx::Size& page_size() { return test_job()->page_size(); }

  const gfx::Rect& content_area() { return test_job()->content_area(); }

  const gfx::Point& physical_offsets() {
    return test_job()->physical_offsets();
  }

#if defined(OS_WIN)
  PrintSettings::PrinterType type() { return test_job()->type(); }
#endif

  // Ends the run loop.
  void FakePrintCallback(const base::Value& error) {
    DCHECK(run_loop_);
    run_loop_->Quit();
  }

  // Starts a run loop that quits when the print callback is called to indicate
  // printing is complete.
  void WaitForCallback() {
    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
    run_loop.Run();
  }

 protected:
  // Override to create a TestPrintJob instead of a real one.
  bool CreateNewPrintJob(std::unique_ptr<PrinterQuery> query) override {
    print_job_ = base::MakeRefCounted<TestPrintJob>();
    print_job_->Initialize(std::move(query), RenderSourceName(), number_pages_);
#if defined(OS_CHROMEOS)
    print_job_->SetSource(PrintJob::Source::PRINT_PREVIEW, /*source_id=*/"");
#endif  // defined(OS_CHROMEOS)
    return true;
  }

 private:
  TestPrintJob* test_job() {
    return static_cast<TestPrintJob*>(print_job_.get());
  }

  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestPrintViewManager);
};

TEST_F(PrintViewManagerTest, PrintSubFrameAndDestroy) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RenderFrameHost* sub_frame =
      content::RenderFrameHostTester::For(web_contents->GetMainFrame())
          ->AppendChild("child");

  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(web_contents);
  ASSERT_TRUE(print_view_manager);
  EXPECT_FALSE(print_view_manager->print_preview_rfh());

  print_view_manager->PrintPreviewNow(sub_frame, false);
  EXPECT_TRUE(print_view_manager->print_preview_rfh());

  content::RenderFrameHostTester::For(sub_frame)->Detach();
  EXPECT_FALSE(print_view_manager->print_preview_rfh());
}

#if defined(OS_WIN)
// Verifies that StartPdfToPostScriptConversion is called with the correct
// printable area offsets. See crbug.com/821485.
TEST_F(PrintViewManagerTest, PostScriptHasCorrectOffsets) {
  scoped_refptr<TestPrintQueriesQueue> queue =
      base::MakeRefCounted<TestPrintQueriesQueue>();

  // Setup PostScript printer with printable area offsets of 0.1in.
  queue->SetupPrinterType(PrintSettings::PrinterType::TYPE_POSTSCRIPT_LEVEL2);
  int offset_in_pixels = static_cast<int>(kTestPrinterDpi * 0.1f);
  queue->SetupPrinterOffsets(offset_in_pixels, offset_in_pixels);
  g_browser_process->print_job_manager()->SetQueueForTest(queue);

  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RemoveWebContentsReceiverSet(web_contents,
                                        mojom::PrintManagerHost::Name_);

  std::unique_ptr<TestPrintViewManager> print_view_manager =
      std::make_unique<TestPrintViewManager>(web_contents);

  print_view_manager->PrintPreviewNow(web_contents->GetMainFrame(), false);

  base::Value print_ticket = GetPrintTicket(PrinterType::kLocal);
  const char kTestData[] = "abc";
  auto print_data = base::MakeRefCounted<base::RefCountedStaticMemory>(
      kTestData, sizeof(kTestData));
  PrinterHandler::PrintCallback callback =
      base::BindOnce(&TestPrintViewManager::FakePrintCallback,
                     base::Unretained(print_view_manager.get()));
  print_view_manager->PrintForPrintPreview(std::move(print_ticket), print_data,
                                           web_contents->GetMainFrame(),
                                           std::move(callback));
  print_view_manager->WaitForCallback();

  EXPECT_EQ(gfx::Point(60, 60), print_view_manager->physical_offsets());
  EXPECT_EQ(gfx::Rect(0, 0, 5100, 6600), print_view_manager->content_area());
  EXPECT_EQ(PrintSettings::PrinterType::TYPE_POSTSCRIPT_LEVEL2,
            print_view_manager->type());
}
#endif

}  // namespace printing
