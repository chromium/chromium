// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/printing/test_print_job.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#include "printing/units.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#if BUILDFLAG(IS_WIN)
#include "printing/mojom/print.mojom.h"
#endif

namespace printing {

using PrintViewManagerTest = BrowserWithTestWindowTest;

namespace {

class TestPrintQueriesQueue : public PrintQueriesQueue {
 public:
  TestPrintQueriesQueue() = default;
  TestPrintQueriesQueue(const TestPrintQueriesQueue&) = delete;
  TestPrintQueriesQueue& operator=(const TestPrintQueriesQueue&) = delete;

  // Creates a `TestPrinterQuery`. Sets up the printer query with the printer
  // settings indicated by `printable_offset_x_`, `printable_offset_y_`, and
  // `print_driver_type_`.
  std::unique_ptr<PrinterQuery> CreatePrinterQuery(
      content::GlobalRenderFrameHostId rfh_id) override;

  // Sets the printer's printable area offsets to `offset_x` and `offset_y`,
  // which should be in pixels. Used to fill in printer settings that would
  // normally be filled in by the backend `PrintingContext`.
  void SetupPrinterOffsets(int offset_x, int offset_y);

#if BUILDFLAG(IS_WIN)
  // Sets the printer type to `type`. Used to fill in printer settings that
  // would normally be filled in by the backend `PrintingContext`.
  void SetupPrinterLanguageType(mojom::PrinterLanguageType type);
#endif

 private:
  ~TestPrintQueriesQueue() override = default;

#if BUILDFLAG(IS_WIN)
  mojom::PrinterLanguageType printer_language_type_;
#endif
  int printable_offset_x_;
  int printable_offset_y_;
};

class TestPrinterQuery : public PrinterQuery {
 public:
  // Can only be called on the IO thread, since this inherits from
  // `PrinterQuery`.
  explicit TestPrinterQuery(content::GlobalRenderFrameHostId rfh_id);
  TestPrinterQuery(const TestPrinterQuery&) = delete;
  TestPrinterQuery& operator=(const TestPrinterQuery&) = delete;
  ~TestPrinterQuery() override;

  // Updates the current settings with `new_settings` dictionary values. Also
  // fills in the settings with values from `offsets_` and `printer_type_` that
  // would normally be filled in by the `PrintingContext`.
  void SetSettings(base::Value::Dict new_settings,
                   base::OnceClosure callback) override;

#if BUILDFLAG(IS_WIN)
  // Sets `printer_language_type_` to `type`. Should be called before
  // `SetSettings()`.
  void SetPrinterLanguageType(mojom::PrinterLanguageType type);
#endif

  // Sets printer offsets to `offset_x` and `offset_y`, which should be in DPI.
  // Should be called before `SetSettings()`.
  void SetPrintableAreaOffsets(int offset_x, int offset_y);

 private:
  absl::optional<gfx::Point> offsets_;
#if BUILDFLAG(IS_WIN)
  absl::optional<mojom::PrinterLanguageType> printer_language_type_;
#endif
};

std::unique_ptr<PrinterQuery> TestPrintQueriesQueue::CreatePrinterQuery(
    content::GlobalRenderFrameHostId rfh_id) {
  auto test_query = std::make_unique<TestPrinterQuery>(rfh_id);
#if BUILDFLAG(IS_WIN)
  test_query->SetPrinterLanguageType(printer_language_type_);
#endif
  test_query->SetPrintableAreaOffsets(printable_offset_x_, printable_offset_y_);

  return test_query;
}

void TestPrintQueriesQueue::SetupPrinterOffsets(int offset_x, int offset_y) {
  printable_offset_x_ = offset_x;
  printable_offset_y_ = offset_y;
}

#if BUILDFLAG(IS_WIN)
void TestPrintQueriesQueue::SetupPrinterLanguageType(
    mojom::PrinterLanguageType type) {
  printer_language_type_ = type;
}
#endif

TestPrinterQuery::TestPrinterQuery(content::GlobalRenderFrameHostId rfh_id)
    : PrinterQuery(rfh_id) {}

TestPrinterQuery::~TestPrinterQuery() = default;

void TestPrinterQuery::SetSettings(base::Value::Dict new_settings,
                                   base::OnceClosure callback) {
  DCHECK(offsets_);
#if BUILDFLAG(IS_WIN)
  DCHECK(printer_language_type_);
#endif
  std::unique_ptr<PrintSettings> settings =
      PrintSettingsFromJobSettings(new_settings);
  mojom::ResultCode result = mojom::ResultCode::kSuccess;
  if (!settings) {
    settings = std::make_unique<PrintSettings>();
    result = mojom::ResultCode::kFailed;
  }

  float device_microns_per_device_unit =
      static_cast<float>(kMicronsPerInch) / settings->device_units_per_inch();
  gfx::Size paper_size =
      gfx::Size(settings->requested_media().size_microns.width() /
                    device_microns_per_device_unit,
                settings->requested_media().size_microns.height() /
                    device_microns_per_device_unit);
  gfx::Rect paper_rect(0, 0, paper_size.width(), paper_size.height());
  paper_rect.Inset(gfx::Insets::VH(offsets_->y(), offsets_->x()));
  settings->SetPrinterPrintableArea(paper_size, paper_rect, true);
#if BUILDFLAG(IS_WIN)
  settings->set_printer_language_type(*printer_language_type_);
#endif

  GetSettingsDone(std::move(callback), /*maybe_is_modifiable=*/absl::nullopt,
                  std::move(settings), result);
}

#if BUILDFLAG(IS_WIN)
void TestPrinterQuery::SetPrinterLanguageType(mojom::PrinterLanguageType type) {
  printer_language_type_ = type;
}
#endif

void TestPrinterQuery::SetPrintableAreaOffsets(int offset_x, int offset_y) {
  offsets_ = gfx::Point(offset_x, offset_y);
}

class TestPrintViewManagerForSystemDialogPrint : public PrintViewManager {
 public:
  explicit TestPrintViewManagerForSystemDialogPrint(
      content::WebContents* web_contents)
      : PrintViewManager(web_contents) {}
  ~TestPrintViewManagerForSystemDialogPrint() override = default;

  // PrintViewManager:
  void PrintForSystemDialogImpl() override {
    // There has to be a target frame so DidShowPrintDialog() does not crash.
    // Manually set it, as there is no IPC in progress.
    print_manager_host_receivers_for_testing().SetCurrentTargetFrameForTesting(
        web_contents()->GetPrimaryMainFrame());
    DidShowPrintDialog();
    print_manager_host_receivers_for_testing().SetCurrentTargetFrameForTesting(
        nullptr);
  }
};

}  // namespace

class TestPrintViewManager : public PrintViewManagerBase {
 public:
  explicit TestPrintViewManager(content::WebContents* web_contents)
      : PrintViewManagerBase(web_contents) {}
  TestPrintViewManager(const TestPrintViewManager&) = delete;
  TestPrintViewManager& operator=(const TestPrintViewManager&) = delete;

  ~TestPrintViewManager() override {
    // Set this null here. Otherwise, the `PrintViewManagerBase` destructor
    // will try to de-register for notifications that were not registered for
    // in `CreateNewPrintJob()`.
    print_job_ = nullptr;
  }

  // Mostly copied from `PrintViewManager::PrintPreviewNow()`. We can't
  // override `PrintViewManager` since it is a user data class.
  bool PrintPreviewNow(content::RenderFrameHost* rfh, bool has_selection) {
    // Don't print / print preview crashed tabs.
    if (IsCrashed())
      return false;

    mojo::AssociatedRemote<mojom::PrintRenderFrame> print_render_frame;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&print_render_frame);
    print_render_frame->InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        mojo::NullAssociatedRemote(),
#endif
        has_selection);
    return true;
  }

  // Getters for validating arguments to StartPdf...Conversion functions
  const gfx::Size& page_size() { return test_job()->page_size(); }

  const gfx::Rect& content_area() { return test_job()->content_area(); }

  const gfx::Point& physical_offsets() {
    return test_job()->physical_offsets();
  }

#if BUILDFLAG(IS_WIN)
  mojom::PrinterLanguageType type() { return test_job()->type(); }
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
  // Override to create a `TestPrintJob` instead of a real one.
  bool CreateNewPrintJob(std::unique_ptr<PrinterQuery> query) override {
    print_job_ = base::MakeRefCounted<TestPrintJob>();
    print_job_->Initialize(std::move(query), RenderSourceName(),
                           number_pages());
#if BUILDFLAG(IS_CHROMEOS)
    print_job_->SetSource(PrintJob::Source::kPrintPreview, /*source_id=*/"");
#endif  // BUILDFLAG(IS_CHROMEOS)
    return true;
  }
  void SetupScriptedPrintPreview(
      SetupScriptedPrintPreviewCallback callback) override {
    NOTREACHED();
  }
  void ShowScriptedPrintPreview(bool is_modifiable) override { NOTREACHED(); }
  void RequestPrintPreview(
      mojom::RequestPrintPreviewParamsPtr params) override {
    NOTREACHED();
  }
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override {
    NOTREACHED();
  }

 private:
  TestPrintJob* test_job() {
    return static_cast<TestPrintJob*>(print_job_.get());
  }

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION base::RunLoop* run_loop_ = nullptr;
};

TEST_F(PrintViewManagerTest, PrintSubFrameAndDestroy) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RenderFrameHost* sub_frame =
      content::RenderFrameHostTester::For(web_contents->GetPrimaryMainFrame())
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

TEST_F(PrintViewManagerTest, PrintForSystemDialog) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  auto print_view_manager =
      std::make_unique<TestPrintViewManagerForSystemDialogPrint>(web_contents);

  ASSERT_TRUE(print_view_manager->PrintPreviewNow(
      web_contents->GetPrimaryMainFrame(), /*has_selection=*/false));

  base::RunLoop run_loop;
  bool dialog_shown = false;
  EXPECT_TRUE(print_view_manager->PrintForSystemDialogNow(
      base::BindLambdaForTesting([&]() {
        dialog_shown = true;
        run_loop.Quit();
      })));
  run_loop.Run();
  EXPECT_TRUE(dialog_shown);

  print_view_manager->PrintPreviewDone();
}

#if BUILDFLAG(IS_WIN)
// Verifies that `StartPdfToPostScriptConversion` is called with the correct
// printable area offsets. See crbug.com/821485.
TEST_F(PrintViewManagerTest, PostScriptHasCorrectOffsets) {
  scoped_refptr<TestPrintQueriesQueue> queue =
      base::MakeRefCounted<TestPrintQueriesQueue>();

  // Setup PostScript printer with printable area offsets of 0.1in.
  queue->SetupPrinterLanguageType(
      mojom::PrinterLanguageType::kPostscriptLevel2);
  int offset_in_pixels = static_cast<int>(test::kPrinterDpi * 0.1f);
  queue->SetupPrinterOffsets(offset_in_pixels, offset_in_pixels);
  g_browser_process->print_job_manager()->SetQueueForTest(queue);

  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  std::unique_ptr<TestPrintViewManager> print_view_manager =
      std::make_unique<TestPrintViewManager>(web_contents);
  PrintViewManager::SetReceiverImplForTesting(print_view_manager.get());

  print_view_manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(),
                                      false);

  base::Value::Dict print_ticket =
      test::GetPrintTicket(mojom::PrinterType::kLocal);
  const char kTestData[] = "abc";
  auto print_data = base::MakeRefCounted<base::RefCountedStaticMemory>(
      kTestData, sizeof(kTestData));
  PrinterHandler::PrintCallback callback =
      base::BindOnce(&TestPrintViewManager::FakePrintCallback,
                     base::Unretained(print_view_manager.get()));
  print_view_manager->PrintForPrintPreview(std::move(print_ticket), print_data,
                                           web_contents->GetPrimaryMainFrame(),
                                           std::move(callback));
  print_view_manager->WaitForCallback();

  EXPECT_EQ(gfx::Point(60, 60), print_view_manager->physical_offsets());
  EXPECT_EQ(gfx::Rect(0, 0, 5100, 6600), print_view_manager->content_area());
  EXPECT_EQ(mojom::PrinterLanguageType::kPostscriptLevel2,
            print_view_manager->type());

  PrintViewManager::SetReceiverImplForTesting(nullptr);
}
#endif

}  // namespace printing
