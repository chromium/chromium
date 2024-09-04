// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#include "printing/printed_document.h"
#include "printing/units.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#endif  // BUILDFLAG(IS_WIN)

namespace printing {

using PrintViewManagerTest = BrowserWithTestWindowTest;

namespace {

#if BUILDFLAG(IS_WIN)
class TestPrintQueriesQueueWin : public PrintQueriesQueue {
 public:
  TestPrintQueriesQueueWin() = default;
  TestPrintQueriesQueueWin(const TestPrintQueriesQueueWin&) = delete;
  TestPrintQueriesQueueWin& operator=(const TestPrintQueriesQueueWin&) = delete;

  // Creates a `TestPrinterQueryWin`. Sets up the printer query with the printer
  // settings indicated by `printable_offset_x_`, `printable_offset_y_`, and
  // `print_driver_type_`.
  std::unique_ptr<PrinterQuery> CreatePrinterQuery(
      content::GlobalRenderFrameHostId rfh_id) override;

  // Sets the printer's printable area offsets to `offset_x` and `offset_y`,
  // which should be in pixels. Used to fill in printer settings that would
  // normally be filled in by the backend `PrintingContext`.
  void SetupPrinterOffsets(int offset_x, int offset_y);

  // Sets the printer type to `type`. Used to fill in printer settings that
  // would normally be filled in by the backend `PrintingContext`.
  void SetupPrinterLanguageType(mojom::PrinterLanguageType type);

 private:
  ~TestPrintQueriesQueueWin() override = default;

  mojom::PrinterLanguageType printer_language_type_;
  int printable_offset_x_;
  int printable_offset_y_;
};

class TestPrinterQueryWin : public PrinterQuery {
 public:
  // Can only be called on the IO thread, since this inherits from
  // `PrinterQuery`.
  explicit TestPrinterQueryWin(content::GlobalRenderFrameHostId rfh_id);
  TestPrinterQueryWin(const TestPrinterQueryWin&) = delete;
  TestPrinterQueryWin& operator=(const TestPrinterQueryWin&) = delete;
  ~TestPrinterQueryWin() override;

  // Updates the current settings with `new_settings` dictionary values. Also
  // fills in the settings with values from `offsets_` and `printer_type_` that
  // would normally be filled in by the `PrintingContext`.
  void SetSettings(base::Value::Dict new_settings,
                   base::OnceClosure callback) override;

  // Sets `printer_language_type_` to `type`. Should be called before
  // `SetSettings()`.
  void SetPrinterLanguageType(mojom::PrinterLanguageType type);

  // Sets printer offsets to `offset_x` and `offset_y`, which should be in DPI.
  // Should be called before `SetSettings()`.
  void SetPrintableAreaOffsets(int offset_x, int offset_y);

 private:
  std::optional<gfx::Point> offsets_;
  std::optional<mojom::PrinterLanguageType> printer_language_type_;
};

std::unique_ptr<PrinterQuery> TestPrintQueriesQueueWin::CreatePrinterQuery(
    content::GlobalRenderFrameHostId rfh_id) {
  auto test_query = std::make_unique<TestPrinterQueryWin>(rfh_id);
  test_query->SetPrinterLanguageType(printer_language_type_);
  test_query->SetPrintableAreaOffsets(printable_offset_x_, printable_offset_y_);
  return test_query;
}

void TestPrintQueriesQueueWin::SetupPrinterOffsets(int offset_x, int offset_y) {
  printable_offset_x_ = offset_x;
  printable_offset_y_ = offset_y;
}

void TestPrintQueriesQueueWin::SetupPrinterLanguageType(
    mojom::PrinterLanguageType type) {
  printer_language_type_ = type;
}

TestPrinterQueryWin::TestPrinterQueryWin(
    content::GlobalRenderFrameHostId rfh_id)
    : PrinterQuery(rfh_id) {}

TestPrinterQueryWin::~TestPrinterQueryWin() = default;

void TestPrinterQueryWin::SetSettings(base::Value::Dict new_settings,
                                      base::OnceClosure callback) {
  DCHECK(offsets_);
  DCHECK(printer_language_type_);
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
  settings->set_printer_language_type(*printer_language_type_);

  GetSettingsDone(std::move(callback), /*maybe_is_modifiable=*/std::nullopt,
                  std::move(settings), result);
}

void TestPrinterQueryWin::SetPrinterLanguageType(
    mojom::PrinterLanguageType type) {
  printer_language_type_ = type;
}

void TestPrinterQueryWin::SetPrintableAreaOffsets(int offset_x, int offset_y) {
  offsets_ = gfx::Point(offset_x, offset_y);
}

class TestPrintJobWin : public PrintJob {
 public:
  // Create an empty `PrintJob`. When initializing with this constructor,
  // post-constructor initialization must be done with `Initialize()`.
  TestPrintJobWin() = default;

  // Getters for values stored by `TestPrintJobWin` in Start...Converter
  // functions.
  const gfx::Size& page_size() const { return page_size_; }
  const gfx::Rect& content_area() const { return content_area_; }
  const gfx::Point& physical_offsets() const { return physical_offsets_; }
  mojom::PrinterLanguageType type() const { return type_; }

  // All remaining functions are `PrintJob` implementation.
  void Initialize(std::unique_ptr<PrinterQuery> query,
                  const std::u16string& name,
                  uint32_t page_count) override {
    // Since we do not actually print in these tests, just let this get
    // destroyed when this function exits.
    std::unique_ptr<PrintJobWorker> worker =
        query->TransferContextToNewWorker(nullptr);

    scoped_refptr<PrintedDocument> new_doc =
        base::MakeRefCounted<PrintedDocument>(query->ExtractSettings(), name,
                                              query->cookie());

    new_doc->set_page_count(page_count);
    UpdatePrintedDocument(new_doc.get());
  }

  // Sets `job_pending_` to true.
  void StartPrinting() override { set_job_pending_for_testing(true); }

  // Sets `job_pending_` to false and deletes the worker.
  void Stop() override { set_job_pending_for_testing(false); }

  // Sets `job_pending_` to false and deletes the worker.
  void Cancel() override { set_job_pending_for_testing(false); }

  void OnFailed() override {}

  void OnDocDone(int job_id, PrintedDocument* document) override {}

  // Intentional no-op, returns true.
  bool FlushJob(base::TimeDelta timeout) override { return true; }

  // These functions fill in the corresponding member variables based on the
  // arguments passed in.
  void StartPdfToEmfConversion(scoped_refptr<base::RefCountedMemory> bytes,
                               const gfx::Size& page_size,
                               const gfx::Rect& content_area,
                               const GURL& url) override {
    page_size_ = page_size;
    content_area_ = content_area;
    type_ = mojom::PrinterLanguageType::kNone;
  }

  void StartPdfToPostScriptConversion(
      scoped_refptr<base::RefCountedMemory> bytes,
      const gfx::Rect& content_area,
      const gfx::Point& physical_offsets,
      bool ps_level2,
      const GURL& url) override {
    content_area_ = content_area;
    physical_offsets_ = physical_offsets;
    type_ = ps_level2 ? mojom::PrinterLanguageType::kPostscriptLevel2
                      : mojom::PrinterLanguageType::kPostscriptLevel3;
  }

  void StartPdfToTextConversion(scoped_refptr<base::RefCountedMemory> bytes,
                                const gfx::Size& page_size,
                                const GURL& url) override {
    page_size_ = page_size;
    type_ = mojom::PrinterLanguageType::kTextOnly;
  }

 private:
  ~TestPrintJobWin() override { set_job_pending_for_testing(false); }

  gfx::Size page_size_;
  gfx::Rect content_area_;
  gfx::Point physical_offsets_;
  mojom::PrinterLanguageType type_;
};
#endif  // BUILDFLAG(IS_WIN)

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

#if BUILDFLAG(IS_WIN)
class TestPrintViewManagerWin : public PrintViewManagerBase {
 public:
  explicit TestPrintViewManagerWin(content::WebContents* web_contents)
      : PrintViewManagerBase(web_contents) {}
  TestPrintViewManagerWin(const TestPrintViewManagerWin&) = delete;
  TestPrintViewManagerWin& operator=(const TestPrintViewManagerWin&) = delete;

  ~TestPrintViewManagerWin() override {
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
        has_selection);
    return true;
  }

  // Getters for validating arguments to StartPdf...Conversion functions
  const gfx::Size& page_size() { return test_job()->page_size(); }

  const gfx::Rect& content_area() { return test_job()->content_area(); }

  const gfx::Point& physical_offsets() {
    return test_job()->physical_offsets();
  }

  mojom::PrinterLanguageType type() { return test_job()->type(); }

  // Ends the run loop.
  void FakePrintCallback(const base::Value& error) {
    DCHECK(run_loop_);
    run_loop_->Quit();
  }

  // Starts a run loop that quits when the print callback is called to indicate
  // printing is complete.
  void WaitForCallback() {
    base::RunLoop run_loop;
    base::AutoReset<raw_ptr<base::RunLoop>> auto_reset(&run_loop_, &run_loop);
    run_loop.Run();
  }

 protected:
  // Override to create a `TestPrintJobWin` instead of a real one.
  bool SetupNewPrintJob(std::unique_ptr<PrinterQuery> query) override {
    print_job_ = base::MakeRefCounted<TestPrintJobWin>();
    print_job_->Initialize(std::move(query), RenderSourceName(),
                           number_pages());
    return true;
  }
  void SetupScriptedPrintPreview(
      SetupScriptedPrintPreviewCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  void ShowScriptedPrintPreview(bool is_modifiable) override {
    NOTREACHED_IN_MIGRATION();
  }
  void RequestPrintPreview(
      mojom::RequestPrintPreviewParamsPtr params) override {
    NOTREACHED_IN_MIGRATION();
  }
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  TestPrintJobWin* test_job() {
    return static_cast<TestPrintJobWin*>(print_job_.get());
  }

  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};
#endif  // BUILDFLAG(IS_WIN)

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
  scoped_refptr<TestPrintQueriesQueueWin> queue =
      base::MakeRefCounted<TestPrintQueriesQueueWin>();

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

  auto print_view_manager =
      std::make_unique<TestPrintViewManagerWin>(web_contents);
  PrintViewManager::SetReceiverImplForTesting(print_view_manager.get());

  print_view_manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(),
                                      false);

  base::Value::Dict print_ticket =
      test::GetPrintTicket(mojom::PrinterType::kLocal);
  const char kTestData[] = "abc";
  auto print_data = base::MakeRefCounted<base::RefCountedStaticMemory>(
      base::as_byte_span(kTestData));
  PrinterHandler::PrintCallback callback =
      base::BindOnce(&TestPrintViewManagerWin::FakePrintCallback,
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
#endif  // BUILDFLAG(IS_WIN)

}  // namespace printing
