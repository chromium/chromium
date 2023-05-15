// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/printing/print_browsertest.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/printing/test_print_preview_observer.h"
#include "chrome/browser/printing/test_print_view_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"
#include "printing/printing_features.h"
#include "printing/printing_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/browser/printing/print_job_worker_oop.h"
#include "chrome/browser/printing/printer_query_oop.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/822505)  ChromeOS uses different testing setup that isn't
// hooked up to make use of `TestPrintingContext` yet.
#error "ChromeOS not supported here yet"
#endif

namespace printing {

namespace {

#if !BUILDFLAG(IS_CHROMEOS)
constexpr gfx::Size kLetterPhysicalSize = gfx::Size(612, 792);
constexpr gfx::Rect kLetterPrintableArea = gfx::Rect(5, 5, 602, 782);
constexpr gfx::Size kLegalPhysicalSize = gfx::Size(612, 1008);
constexpr gfx::Rect kLegalPrintableArea = gfx::Rect(5, 5, 602, 998);

// The default margins are set to 1.0cm in //printing/print_settings.cc, which
// is about 28 printer units. The resulting content size is 556 x 736 for
// Letter, and similarly is 556 x 952 for Legal.
constexpr gfx::Size kLetterExpectedContentSize = gfx::Size(556, 736);
constexpr gfx::Size kLegalExpectedContentSize = gfx::Size(556, 952);
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
constexpr char kFakeDmToken[] = "fake-dm-token";
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

}  // namespace

#if BUILDFLAG(ENABLE_OOP_PRINTING)
using OnUseDefaultSettingsCallback = base::RepeatingClosure;
using OnGetSettingsWithUICallback = base::RepeatingClosure;

using ErrorCheckCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
using OnDidUseDefaultSettingsCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
using OnDidAskUserForSettingsCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
#endif
using OnDidUpdatePrintSettingsCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
using OnDidStartPrintingCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
#if BUILDFLAG(IS_WIN)
using OnDidRenderPrintedPageCallback =
    base::RepeatingCallback<void(uint32_t page_number,
                                 mojom::ResultCode result)>;
#endif
using OnDidRenderPrintedDocumentCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
using OnDidDocumentDoneCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
using OnDidCancelCallback = base::RepeatingClosure;
using OnDidShowErrorDialog = base::RepeatingClosure;

class TestPrinterQuery : public PrinterQuery {
 public:
  // Callbacks to run for overrides.
  struct PrintCallbacks {
    OnUseDefaultSettingsCallback did_use_default_settings_callback;
    OnGetSettingsWithUICallback did_get_settings_with_ui_callback;
  };

  TestPrinterQuery(content::GlobalRenderFrameHostId rfh_id,
                   PrintCallbacks* callbacks)
      : PrinterQuery(rfh_id), callbacks_(callbacks) {}

  void UseDefaultSettings(SettingsCallback callback) override {
    DVLOG(1) << "Observed: invoke use default settings";
    PrinterQuery::UseDefaultSettings(std::move(callback));
    callbacks_->did_use_default_settings_callback.Run();
  }

  void GetSettingsWithUI(uint32_t document_page_count,
                         bool has_selection,
                         bool is_scripted,
                         SettingsCallback callback) override {
    DVLOG(1) << "Observed: invoke get settings with UI";
    PrinterQuery::GetSettingsWithUI(document_page_count, has_selection,
                                    is_scripted, std::move(callback));
    callbacks_->did_get_settings_with_ui_callback.Run();
  }

  raw_ptr<PrintCallbacks> callbacks_;
};

class TestPrintJobWorkerOop : public PrintJobWorkerOop {
 public:
  // Callbacks to run for overrides are broken into the following steps:
  //   1.  Error case processing.  Call `error_check_callback` to reset any
  //       triggers that were primed to cause errors in the testing context.
  //   2.  Run the base class callback for normal handling.  If there was an
  //       access-denied error then this can lead to a retry.  The retry has a
  //       chance to succeed since error triggers were removed.
  //   3.  Exercise the associated test callback (e.g.,
  //       `did_start_printing_callback` when in `OnDidStartPrinting()`) to note
  //       the callback was observed and completed.  This ensures all base class
  //       processing was done before possibly quitting the test run loop.
  struct PrintCallbacks {
    ErrorCheckCallback error_check_callback;
    OnDidUseDefaultSettingsCallback did_use_default_settings_callback;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    OnDidAskUserForSettingsCallback did_ask_user_for_settings_callback;
#endif
    OnDidUpdatePrintSettingsCallback did_update_print_settings_callback;
    OnDidStartPrintingCallback did_start_printing_callback;
#if BUILDFLAG(IS_WIN)
    OnDidRenderPrintedPageCallback did_render_printed_page_callback;
#endif
    OnDidRenderPrintedDocumentCallback did_render_printed_document_callback;
    OnDidDocumentDoneCallback did_document_done_callback;
    OnDidCancelCallback did_cancel_callback;
  };

  TestPrintJobWorkerOop(
      std::unique_ptr<PrintingContext::Delegate> printing_context_delegate,
      std::unique_ptr<PrintingContext> printing_context,
      absl::optional<PrintBackendServiceManager::ClientId> client_id,
      absl::optional<PrintBackendServiceManager::ContextId> context_id,
      PrintJob* print_job,
      bool print_from_system_dialog,
      bool simulate_spooling_memory_errors,
      TestPrintJobWorkerOop::PrintCallbacks* callbacks)
      : PrintJobWorkerOop(std::move(printing_context_delegate),
                          std::move(printing_context),
                          client_id,
                          context_id,
                          print_job,
                          print_from_system_dialog,
                          simulate_spooling_memory_errors),
        callbacks_(callbacks) {}
  TestPrintJobWorkerOop(const TestPrintJobWorkerOop&) = delete;
  TestPrintJobWorkerOop& operator=(const TestPrintJobWorkerOop&) = delete;
  ~TestPrintJobWorkerOop() override = default;

 private:
  void OnDidStartPrinting(mojom::ResultCode result) override {
    DVLOG(1) << "Observed: start printing of document";
    callbacks_->error_check_callback.Run(result);
    PrintJobWorkerOop::OnDidStartPrinting(result);
    callbacks_->did_start_printing_callback.Run(result);
  }

#if BUILDFLAG(IS_WIN)
  void OnDidRenderPrintedPage(uint32_t page_number,
                              mojom::ResultCode result) override {
    DVLOG(1) << "Observed render for printed page " << page_number;
    callbacks_->error_check_callback.Run(result);
    PrintJobWorkerOop::OnDidRenderPrintedPage(page_number, result);
    callbacks_->did_render_printed_page_callback.Run(page_number, result);
  }
#endif  // BUILDFLAG(IS_WIN)

  void OnDidRenderPrintedDocument(mojom::ResultCode result) override {
    DVLOG(1) << "Observed render for printed document";
    callbacks_->error_check_callback.Run(result);
    PrintJobWorkerOop::OnDidRenderPrintedDocument(result);
    callbacks_->did_render_printed_document_callback.Run(result);
  }

  void OnDidDocumentDone(int job_id, mojom::ResultCode result) override {
    DVLOG(1) << "Observed: document done";
    callbacks_->error_check_callback.Run(result);
    PrintJobWorkerOop::OnDidDocumentDone(job_id, result);
    callbacks_->did_document_done_callback.Run(result);
  }

  void OnDidCancel(scoped_refptr<PrintJob> job,
                   mojom::ResultCode result) override {
    DVLOG(1) << "Observed: cancel";
    // Must not use `std::move(job)`, as that could potentially cause the `job`
    // (and consequentially `this`) to be destroyed before
    // `did_cancel_callback` is run.
    PrintJobWorkerOop::OnDidCancel(job, result);
    callbacks_->did_cancel_callback.Run();
  }

  raw_ptr<PrintCallbacks> callbacks_;
};

class TestPrinterQueryOop : public PrinterQueryOop {
 public:
  TestPrinterQueryOop(content::GlobalRenderFrameHostId rfh_id,
                      bool simulate_spooling_memory_errors,
                      TestPrintJobWorkerOop::PrintCallbacks* callbacks)
      : PrinterQueryOop(rfh_id),
        simulate_spooling_memory_errors_(simulate_spooling_memory_errors),
        callbacks_(callbacks) {}

  void OnDidUseDefaultSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings) override {
    DVLOG(1) << "Observed: use default settings";
    mojom::ResultCode result = print_settings->is_result_code()
                                   ? print_settings->get_result_code()
                                   : mojom::ResultCode::kSuccess;
    callbacks_->error_check_callback.Run(result);
    PrinterQueryOop::OnDidUseDefaultSettings(std::move(callback),
                                             std::move(print_settings));
    callbacks_->did_use_default_settings_callback.Run(result);
  }

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void OnDidAskUserForSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings) override {
    DVLOG(1) << "Observed: ask user for settings";
    mojom::ResultCode result = print_settings->is_result_code()
                                   ? print_settings->get_result_code()
                                   : mojom::ResultCode::kSuccess;
    callbacks_->error_check_callback.Run(result);
    PrinterQueryOop::OnDidAskUserForSettings(std::move(callback),
                                             std::move(print_settings));
    callbacks_->did_ask_user_for_settings_callback.Run(result);
  }
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

  void OnDidUpdatePrintSettings(
      const std::string& device_name,
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings) override {
    DVLOG(1) << "Observed: update print settings";
    mojom::ResultCode result = print_settings->is_result_code()
                                   ? print_settings->get_result_code()
                                   : mojom::ResultCode::kSuccess;
    callbacks_->error_check_callback.Run(result);
    PrinterQueryOop::OnDidUpdatePrintSettings(device_name, std::move(callback),
                                              std::move(print_settings));
    callbacks_->did_update_print_settings_callback.Run(result);
  }

  std::unique_ptr<PrintJobWorkerOop> CreatePrintJobWorker(
      PrintJob* print_job) override {
    return std::make_unique<TestPrintJobWorkerOop>(
        std::move(printing_context_delegate_), std::move(printing_context_),
        print_document_client_id(), context_id(), print_job,
        print_from_system_dialog(), simulate_spooling_memory_errors_,
        callbacks_);
  }

  bool simulate_spooling_memory_errors_;
  raw_ptr<TestPrintJobWorkerOop::PrintCallbacks> callbacks_;
};
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

class SystemAccessProcessPrintBrowserTestBase
    : public PrintBrowserTest,
      public PrintJob::Observer,
      public PrintViewManagerBase::Observer {
 public:
  SystemAccessProcessPrintBrowserTestBase() = default;
  ~SystemAccessProcessPrintBrowserTestBase() override = default;

  virtual bool UseService() = 0;

  // Only of interest when `UseService()` returns true.
  virtual bool SandboxService() = 0;

  void SetUp() override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (UseService()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          features::kEnableOopPrintDrivers,
          {{features::kEnableOopPrintDriversJobPrint.name, "true"},
           {features::kEnableOopPrintDriversSandbox.name,
            SandboxService() ? "true" : "false"}});

      // Safe to use `base::Unretained(this)` since this testing class
      // necessarily must outlive all interactions from the tests which will
      // run through `TestPrintJobWorkerOop`, the user of these callbacks.
      test_print_job_worker_oop_callbacks_.error_check_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::ErrorCheck,
              base::Unretained(this));
      test_print_job_worker_oop_callbacks_.did_use_default_settings_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidUseDefaultSettings,
              base::Unretained(this));
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
      test_print_job_worker_oop_callbacks_.did_ask_user_for_settings_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidAskUserForSettings,
              base::Unretained(this));
#endif
      test_print_job_worker_oop_callbacks_
          .did_update_print_settings_callback = base::BindRepeating(
          &SystemAccessProcessPrintBrowserTestBase::OnDidUpdatePrintSettings,
          base::Unretained(this));
      test_print_job_worker_oop_callbacks_.did_start_printing_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidStartPrinting,
              base::Unretained(this));
#if BUILDFLAG(IS_WIN)
      test_print_job_worker_oop_callbacks_.did_render_printed_page_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidRenderPrintedPage,
              base::Unretained(this));
#endif
      test_print_job_worker_oop_callbacks_
          .did_render_printed_document_callback = base::BindRepeating(
          &SystemAccessProcessPrintBrowserTestBase::OnDidRenderPrintedDocument,
          base::Unretained(this));
      test_print_job_worker_oop_callbacks_.did_document_done_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidDocumentDone,
              base::Unretained(this));
      test_print_job_worker_oop_callbacks_.did_cancel_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidCancel,
              base::Unretained(this));
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kEnableOopPrintDrivers});

      test_print_job_worker_callbacks_.did_use_default_settings_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnUseDefaultSettings,
              base::Unretained(this));
      test_print_job_worker_callbacks_.did_get_settings_with_ui_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnGetSettingsWithUI,
              base::Unretained(this));
    }
    test_create_printer_query_callback_ = base::BindRepeating(
        &SystemAccessProcessPrintBrowserTestBase::CreatePrinterQuery,
        base::Unretained(this), UseService());
    PrinterQuery::SetCreatePrinterQueryCallbackForTest(
        &test_create_printer_query_callback_);
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

    PrintBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (UseService()) {
      print_backend_service_ = PrintBackendServiceTestImpl::LaunchForTesting(
          test_remote_, test_print_backend(), /*sandboxed=*/true);
    }
#endif
    PrintBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    PrintBrowserTest::TearDown();
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    PrinterQuery::SetCreatePrinterQueryCallbackForTest(/*callback=*/nullptr);
    if (UseService()) {
      // Check that there is never a straggler client registration.
      EXPECT_EQ(
          PrintBackendServiceManager::GetInstance().GetClientsRegisteredCount(),
          0u);
    }
    PrintBackendServiceManager::ResetForTesting();
#endif
    ASSERT_EQ(print_job_construction_count(), print_job_destruction_count());
  }

  // `PrintBackendServiceTestImpl` does a debug check on shutdown that there
  // are no residual persistent printing contexts left in the service.  For
  // tests which are known to break this (either by design, for test simplicity
  // or because a related change is only partly implemented), use this method
  // to notify the service to not DCHECK on such a condition.
  void SkipPersistentContextsCheckOnShutdown() {
    print_backend_service_->SkipPersistentContextsCheckOnShutdown();
  }

  // PrintViewManagerBase::Observer:
  void OnRegisterSystemPrintClient(bool succeeded) override {
    system_print_registration_succeeded_ = succeeded;
  }

  void OnDidPrintDocument() override {
    ++did_print_document_count_;
    CheckForQuit();
  }

  // PrintJob::Observer:
  void OnDestruction() override {
    ++print_job_destruction_count_;
    CheckForQuit();
  }

  void OnCreatedPrintJob(PrintJob* print_job) {
    ++print_job_construction_count_;
    print_job->AddObserver(*this);
  }

  TestPrintViewManager* SetUpAndReturnPrintViewManager(
      content::WebContents* web_contents) {
    auto manager = std::make_unique<TestPrintViewManager>(
        web_contents,
        base::BindRepeating(
            &SystemAccessProcessPrintBrowserTestBase::OnCreatedPrintJob,
            base::Unretained(this)));
    manager->AddObserver(*this);
    TestPrintViewManager* manager_ptr = manager.get();
    web_contents->SetUserData(PrintViewManager::UserDataKey(),
                              std::move(manager));
    return manager_ptr;
  }

  void SetUpPrintViewManager(content::WebContents* web_contents) {
    std::ignore = SetUpAndReturnPrintViewManager(web_contents);
  }

  void PrintAfterPreviewIsReadyAndLoaded() {
    // First invoke the Print Preview dialog with `StartPrint()`.
    TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
    test::StartPrint(browser()->tab_strip_model()->GetActiveWebContents());
    content::WebContents* preview_dialog =
        print_preview_observer.WaitUntilPreviewIsReadyAndReturnPreviewDialog();
    ASSERT_TRUE(preview_dialog);

    set_rendered_page_count(print_preview_observer.rendered_page_count());

    // Print Preview is completely ready, can now initiate printing.
    // This script locates and clicks the Print button.
    const char kScript[] = R"(
      const button = document.getElementsByTagName('print-preview-app')[0]
                       .$['sidebar']
                       .shadowRoot.querySelector('print-preview-button-strip')
                       .shadowRoot.querySelector('.action-button');
      button.click();)";
    ASSERT_TRUE(content::ExecJs(preview_dialog, kScript));
    WaitUntilCallbackReceived();
  }

  void AdjustMediaAfterPreviewIsReadyAndLoaded() {
    // First invoke the Print Preview dialog with `StartPrint()`.
    TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
    test::StartPrint(browser()->tab_strip_model()->GetActiveWebContents());
    content::WebContents* preview_dialog =
        print_preview_observer.WaitUntilPreviewIsReadyAndReturnPreviewDialog();
    ASSERT_TRUE(preview_dialog);

    set_rendered_page_count(print_preview_observer.rendered_page_count());

    // Initial Print Preview is completely ready.
    // Reset the observer, and then modify the paper size.  This will initiate
    // another preview render.
    // The default paper size is first in the list at index zero, so choose
    // the second item from the list to cause a change.
    print_preview_observer.ResetForAnotherPreview();
    const char kSetPaperSizeScript[] = R"(
      var element =
          document.getElementsByTagName('print-preview-app')[0]
              .$['sidebar']
              .shadowRoot.querySelector('print-preview-media-size-settings');
      element.setSetting('mediaSize', element.capability.option[1]);)";
    ASSERT_TRUE(content::ExecJs(preview_dialog, kSetPaperSizeScript));
    print_preview_observer.WaitUntilPreviewIsReady();
  }

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  void SystemPrintFromPreviewOnceReadyAndLoaded(bool wait_for_callback) {
    // First invoke the Print Preview dialog with `StartPrint()`.
    TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
    test::StartPrint(browser()->tab_strip_model()->GetActiveWebContents());
    content::WebContents* preview_dialog =
        print_preview_observer.WaitUntilPreviewIsReadyAndReturnPreviewDialog();
    ASSERT_TRUE(preview_dialog);

    set_rendered_page_count(print_preview_observer.rendered_page_count());

    // Print Preview is completely ready, can now initiate printing.
    // This script locates and clicks the "Print using system dialog",
    // which is still enabled even if it is hidden.
    const char kPrintWithSystemDialogScript[] = R"(
      const printSystemDialog
          = document.getElementsByTagName('print-preview-app')[0]
              .$['sidebar']
              .shadowRoot.querySelector('print-preview-link-container')
              .$['systemDialogLink'];
        printSystemDialog.click();)";
    // It is possible for sufficient processing for the system print to
    // complete such that the renderer naturally terminates before ExecJs()
    // returns here.  This causes ExecJs() to return false, with a JavaScript
    // error of "Renderer terminated".  Since the termination can actually be
    // a result of successful print processing, do not assert on this return
    // result, just ignore the error instead.  Rely upon tests catching any
    // failure through the use of other expectation checks.
    std::ignore = content::ExecJs(preview_dialog, kPrintWithSystemDialogScript);
    if (wait_for_callback) {
      WaitUntilCallbackReceived();
    }
  }
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

  void PrimeAsRepeatingErrorGenerator() { reset_errors_after_check_ = false; }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  void PrimeForSpoolingSharedMemoryErrors() {
    simulate_spooling_memory_errors_ = true;
  }

  void PrimeForFailInUseDefaultSettings() {
    test_printing_context_factory()->SetFailErrorOnUseDefaultSettings();
  }

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  void PrimeForCancelInAskUserForSettings() {
    test_printing_context_factory()->SetCancelErrorOnAskUserForSettings();
  }
#endif

  void PrimeForCancelInNewDocument() {
    test_printing_context_factory()->SetCancelErrorOnNewDocument(
        /*cause_errors=*/true);
  }

  void PrimeForErrorsInNewDocument() {
    test_printing_context_factory()->SetFailedErrorOnNewDocument(
        /*cause_errors=*/true);
  }

  void PrimeForAccessDeniedErrorsInNewDocument() {
    test_printing_context_factory()->SetAccessDeniedErrorOnNewDocument(
        /*cause_errors=*/true);
  }

#if BUILDFLAG(IS_WIN)
  void PrimeForAccessDeniedErrorsInRenderPrintedPage() {
    test_printing_context_factory()->SetAccessDeniedErrorOnRenderPage(
        /*cause_errors=*/true);
  }

  void PrimeForDelayedRenderingUntilPage(uint32_t page_number) {
    print_backend_service_->set_rendering_delayed_until_page(page_number);
  }

  void PrimeForRenderingErrorOnPage(uint32_t page_number) {
    test_printing_context_factory()->SetFailedErrorForRenderPage(page_number);
  }
#endif

  void PrimeForAccessDeniedErrorsInRenderPrintedDocument() {
    test_printing_context_factory()->SetAccessDeniedErrorOnRenderDocument(
        /*cause_errors=*/true);
  }

  void PrimeForAccessDeniedErrorsInDocumentDone() {
    test_printing_context_factory()->SetAccessDeniedErrorOnDocumentDone(
        /*cause_errors=*/true);
  }

  const absl::optional<bool> system_print_registration_succeeded() const {
    return system_print_registration_succeeded_;
  }

  bool did_use_default_settings() const { return did_use_default_settings_; }

  bool did_get_settings_with_ui() const { return did_get_settings_with_ui_; }

  bool print_backend_service_use_detected() const {
    return print_backend_service_use_detected_;
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  mojom::ResultCode use_default_settings_result() const {
    return use_default_settings_result_;
  }

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  mojom::ResultCode ask_user_for_settings_result() const {
    return ask_user_for_settings_result_;
  }
#endif

  mojom::ResultCode update_print_settings_result() const {
    return update_print_settings_result_;
  }

  mojom::ResultCode start_printing_result() const {
    return start_printing_result_;
  }

#if BUILDFLAG(IS_WIN)
  mojom::ResultCode render_printed_page_result() const {
    return render_printed_page_result_;
  }
  int render_printed_page_count() const { return render_printed_pages_count_; }
#endif  // BUILDFLAG(IS_WIN)

  mojom::ResultCode render_printed_document_result() {
    return render_printed_document_result_;
  }

  mojom::ResultCode document_done_result() const {
    return document_done_result_;
  }

  int cancel_count() const { return cancel_count_; }

  int print_job_construction_count() const {
    return print_job_construction_count_;
  }
  int print_job_destruction_count() const {
    return print_job_destruction_count_;
  }
  int did_print_document_count() const { return did_print_document_count_; }

 private:
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  std::unique_ptr<PrinterQuery> CreatePrinterQuery(
      bool use_service,
      content::GlobalRenderFrameHostId rfh_id) {
    if (use_service) {
      return std::make_unique<TestPrinterQueryOop>(
          rfh_id, simulate_spooling_memory_errors_,
          &test_print_job_worker_oop_callbacks_);
    }
    return std::make_unique<TestPrinterQuery>(
        rfh_id, &test_print_job_worker_callbacks_);
  }

  void OnUseDefaultSettings() {
    did_use_default_settings_ = true;
    PrintBackendServiceDetectionCheck();
    CheckForQuit();
  }

  void OnGetSettingsWithUI() {
    did_get_settings_with_ui_ = true;
    PrintBackendServiceDetectionCheck();
    CheckForQuit();
  }

  void PrintBackendServiceDetectionCheck() {
    // Want to know if `PrintBackendService` clients are ever detected, since
    // registrations could have gone away by the time checks are made at the
    // end of tests.
    if (PrintBackendServiceManager::GetInstance().GetClientsRegisteredCount() >
        0) {
      print_backend_service_use_detected_ = true;
    }
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  void ErrorCheck(mojom::ResultCode result) {
    // Interested to reset any trigger for causing access-denied errors, so
    // that retry logic has a chance to be exercised and succeed.
    if (result == mojom::ResultCode::kAccessDenied) {
      ResetForNoAccessDeniedErrors();
    }
  }

  void OnDidUseDefaultSettings(mojom::ResultCode result) {
    use_default_settings_result_ = result;
    CheckForQuit();
  }

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  void OnDidAskUserForSettings(mojom::ResultCode result) {
    ask_user_for_settings_result_ = result;
    CheckForQuit();
  }
#endif

  void OnDidUpdatePrintSettings(mojom::ResultCode result) {
    update_print_settings_result_ = result;
    CheckForQuit();
  }

  void OnDidStartPrinting(mojom::ResultCode result) {
    start_printing_result_ = result;
    CheckForQuit();
  }

#if BUILDFLAG(IS_WIN)
  void OnDidRenderPrintedPage(uint32_t page_number, mojom::ResultCode result) {
    render_printed_page_result_ = result;
    if (result == mojom::ResultCode::kSuccess) {
      render_printed_pages_count_++;
    }
    CheckForQuit();
  }
#endif

  void OnDidRenderPrintedDocument(mojom::ResultCode result) {
    render_printed_document_result_ = result;
    CheckForQuit();
  }

  void OnDidDocumentDone(mojom::ResultCode result) {
    document_done_result_ = result;
    CheckForQuit();
  }

  void OnDidCancel() {
    ++cancel_count_;
    CheckForQuit();
  }

  void OnDidDestroyPrintJob() {
    ++print_job_destruction_count_;
    CheckForQuit();
  }

  void ResetForNoAccessDeniedErrors() {
    // Don't do the reset if test scenario is repeatedly return errors.
    if (!reset_errors_after_check_) {
      return;
    }

    test_printing_context_factory()->SetAccessDeniedErrorOnNewDocument(
        /*cause_errors=*/false);
#if BUILDFLAG(IS_WIN)
    test_printing_context_factory()->SetAccessDeniedErrorOnRenderPage(
        /*cause_errors=*/false);
#endif
    test_printing_context_factory()->SetAccessDeniedErrorOnRenderDocument(
        /*cause_errors=*/false);
    test_printing_context_factory()->SetAccessDeniedErrorOnDocumentDone(
        /*cause_errors=*/false);
  }

  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  TestPrinterQuery::PrintCallbacks test_print_job_worker_callbacks_;
  TestPrintJobWorkerOop::PrintCallbacks test_print_job_worker_oop_callbacks_;
  CreatePrinterQueryCallback test_create_printer_query_callback_;
  absl::optional<bool> system_print_registration_succeeded_;
  bool did_use_default_settings_ = false;
  bool did_get_settings_with_ui_ = false;
  bool print_backend_service_use_detected_ = false;
  bool simulate_spooling_memory_errors_ = false;
  mojo::Remote<mojom::PrintBackendService> test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
  bool reset_errors_after_check_ = true;
  int did_print_document_count_ = 0;
  mojom::ResultCode use_default_settings_result_ = mojom::ResultCode::kFailed;
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  mojom::ResultCode ask_user_for_settings_result_ = mojom::ResultCode::kFailed;
#endif
  mojom::ResultCode update_print_settings_result_ = mojom::ResultCode::kFailed;
  mojom::ResultCode start_printing_result_ = mojom::ResultCode::kFailed;
#if BUILDFLAG(IS_WIN)
  mojom::ResultCode render_printed_page_result_ = mojom::ResultCode::kFailed;
  int render_printed_pages_count_ = 0;
#endif
  mojom::ResultCode render_printed_document_result_ =
      mojom::ResultCode::kFailed;
  mojom::ResultCode document_done_result_ = mojom::ResultCode::kFailed;
  int cancel_count_ = 0;
  int print_job_construction_count_ = 0;
  int print_job_destruction_count_ = 0;
};

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Values for parameterized testing.
enum class PrintBackendFeatureVariation {
  // `PrintBackend` calls occur from browser process.
  kInBrowserProcess,
  // Use OOP `PrintBackend`.  Attempt to have `PrintBackendService` be
  // sandboxed.
  kOopSandboxedService,
  // Use OOP `PrintBackend`.  Always use `PrintBackendService` unsandboxed.
  kOopUnsandboxedService,
};

class SystemAccessProcessSandboxedServicePrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase {
 public:
  SystemAccessProcessSandboxedServicePrintBrowserTest() = default;
  ~SystemAccessProcessSandboxedServicePrintBrowserTest() override = default;

  bool UseService() override { return true; }
  bool SandboxService() override { return true; }
};

class SystemAccessProcessServicePrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase,
      public testing::WithParamInterface<PrintBackendFeatureVariation> {
 public:
  SystemAccessProcessServicePrintBrowserTest() = default;
  ~SystemAccessProcessServicePrintBrowserTest() override = default;

  bool UseService() override { return true; }
  bool SandboxService() override {
    return GetParam() == PrintBackendFeatureVariation::kOopSandboxedService;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemAccessProcessServicePrintBrowserTest,
    testing::Values(PrintBackendFeatureVariation::kOopSandboxedService,
                    PrintBackendFeatureVariation::kOopUnsandboxedService));

#endif

class SystemAccessProcessInBrowserPrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase {
 public:
  SystemAccessProcessInBrowserPrintBrowserTest() = default;
  ~SystemAccessProcessInBrowserPrintBrowserTest() override = default;

  bool UseService() override { return false; }
  bool SandboxService() override { return false; }
};

class SystemAccessProcessPrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase,
      public testing::WithParamInterface<PrintBackendFeatureVariation> {
 public:
  SystemAccessProcessPrintBrowserTest() = default;
  ~SystemAccessProcessPrintBrowserTest() override = default;

  bool UseService() override {
    return GetParam() != PrintBackendFeatureVariation::kInBrowserProcess;
  }
  bool SandboxService() override {
    return GetParam() == PrintBackendFeatureVariation::kOopSandboxedService;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemAccessProcessPrintBrowserTest,
    testing::Values(PrintBackendFeatureVariation::kInBrowserProcess,
                    PrintBackendFeatureVariation::kOopSandboxedService,
                    PrintBackendFeatureVariation::kOopUnsandboxedService));

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       UpdatePrintSettings) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  TestPrintViewManager print_view_manager(web_contents);
  PrintViewManager::SetReceiverImplForTesting(&print_view_manager);

  PrintAndWaitUntilPreviewIsReady();

  EXPECT_EQ(3u, rendered_page_count());

  const mojom::PrintPagesParamsPtr& snooped_params =
      print_view_manager.snooped_params();
  ASSERT_TRUE(snooped_params);
  EXPECT_EQ(test::kPrinterCapabilitiesDpi, snooped_params->params->dpi);

#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(kLegalPhysicalSize, snooped_params->params->page_size);
  EXPECT_EQ(kLegalPrintableArea, snooped_params->params->printable_area);
  EXPECT_EQ(kLegalExpectedContentSize, snooped_params->params->content_size);
#else
  EXPECT_EQ(kLetterPhysicalSize, snooped_params->params->page_size);
  EXPECT_EQ(kLetterPrintableArea, snooped_params->params->printable_area);
  EXPECT_EQ(kLetterExpectedContentSize, snooped_params->params->content_size);
#endif
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       UpdatePrintSettingsPrintableArea) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  TestPrintViewManager print_view_manager(web_contents);
  PrintViewManager::SetReceiverImplForTesting(&print_view_manager);

  AdjustMediaAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(1u, rendered_page_count());

  const mojom::PrintPagesParamsPtr& snooped_params =
      print_view_manager.snooped_params();
  ASSERT_TRUE(snooped_params);
  EXPECT_EQ(test::kPrinterCapabilitiesDpi, snooped_params->params->dpi);

#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(kLetterPhysicalSize, snooped_params->params->page_size);
  EXPECT_EQ(kLetterPrintableArea, snooped_params->params->printable_area);
  EXPECT_EQ(kLetterExpectedContentSize, snooped_params->params->content_size);
#else
  EXPECT_EQ(kLegalPhysicalSize, snooped_params->params->page_size);
  EXPECT_EQ(kLegalPrintableArea, snooped_params->params->printable_area);
  EXPECT_EQ(kLegalExpectedContentSize, snooped_params->params->content_size);
#endif
}

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrinting) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Update print settings.
  // 2.  A print job is started.
  // 3.  Rendering for 1 page of document of content.
  // 4.  Completes with document done.
  // 5.  Wait for the one print job to be destroyed, to ensure printing
  //    finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/5);
  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1008222)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 1);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
  absl::optional<PrintSettings> settings = document_print_settings();
  ASSERT_TRUE(settings);
  // Collect just the keys to compare the info options vs. advanced settings.
  std::vector<std::string> advanced_setting_keys;
  std::vector<std::string> print_info_options_keys;
  const PrintSettings::AdvancedSettings& advanced_settings =
      settings->advanced_settings();
  for (const auto& advanced_setting : advanced_settings) {
    advanced_setting_keys.push_back(advanced_setting.first);
  }
  for (const auto& option : test::kPrintInfoOptions) {
    print_info_options_keys.push_back(option.first);
  }
  EXPECT_THAT(advanced_setting_keys,
              testing::UnorderedElementsAreArray(print_info_options_keys));
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
}

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingMultipage) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

#if BUILDFLAG(IS_WIN)
  // Windows GDI results in a callback for each rendered page.
  // The expected events for this are:
  // 1.  Update print settings.
  // 2.  A print job is started.
  // 3.  First page is rendered.
  // 4.  Second page is rendered.
  // 5.  Third page is rendered.
  // 6.  Completes with document done.
  // 7.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  // TODO(crbug.com/1008222)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  SetNumExpectedMessages(/*num=*/7);
#else
  // The expected events for this are:
  // 1.  Update print settings.
  // 2.  A print job is started.
  // 3.  Document is rendered.
  // 4.  Completes with document done.
  // 5.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/5);
#endif
  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1008222)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 3);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

IN_PROC_BROWSER_TEST_P(SystemAccessProcessServicePrintBrowserTest,
                       StartPrintingSpoolingSharedMemoryError) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForSpoolingSharedMemoryErrors();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // No attempt to retry is made if a job has a shared memory error when trying
  // to spool a page/document fails on a shared memory error.  The test
  // sequence for this is:
  // 1.  Update print settings.
  // 2.  A print job is started.
  // 3.  Spooling to send the render data will fail.  An error dialog is shown.
  // 4.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 5.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/5);

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(cancel_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

// TODO(crbug.com/1384459): Flaky on MSan builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_StartPrintingFails DISABLED_StartPrintingFails
#else
#define MAYBE_StartPrintingFails StartPrintingFails
#endif
IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       MAYBE_StartPrintingFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForErrorsInNewDocument();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  if (GetParam() == PrintBackendFeatureVariation::kInBrowserProcess) {
    // There are no callbacks for print stages with in-browser printing.  So
    // the print job is started, but that fails, and there is no capturing of
    // that result.
    // The expected events for this are:
    // 1.  An error dialog is shown.
    // 2.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/2);
  } else {
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  A print job is started, but that fails.
    // 3.  An error dialog is shown.
    // 4.  The print job is canceled.  The callback from the service could occur
    //     after the print job has been destroyed.
    // 5.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
  }

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kFailed);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  // No tracking of cancel for in-browser tests, only for OOP.
  if (GetParam() != PrintBackendFeatureVariation::kInBrowserProcess) {
    EXPECT_EQ(cancel_count(), 1);
  }
  EXPECT_EQ(print_job_destruction_count(), 1);
}

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       StartPrintingCanceled) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForCancelInNewDocument();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  if (GetParam() == PrintBackendFeatureVariation::kInBrowserProcess) {
    // A print job is started, but results in a cancel.  There are no callbacks
    // to notice the start job.  The expected events for this are:
    // 1.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/1);
  } else {
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  A print job is started, but results in a cancel.
    // 3.  The print job is canceled.
    // 4.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/4);
  }

  PrintAfterPreviewIsReadyAndLoaded();

  // No tracking of start printing or cancel callbacks for in-browser tests,
  // only for OOP.
  if (GetParam() != PrintBackendFeatureVariation::kInBrowserProcess) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kCanceled);
    EXPECT_EQ(cancel_count(), 1);
  }
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingAccessDenied) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForAccessDeniedErrorsInNewDocument();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Update print settings.
  // 2.  A print job is started, but has an access-denied error.
  // 3.  A retry to start the print job with adjusted access will succeed.
  // 4.  Rendering for 1 page of document of content.
  // 5.  Completes with document done.
  // 6.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/6);

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1008222)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 1);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingRepeatedAccessDenied) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeAsRepeatingErrorGenerator();
  PrimeForAccessDeniedErrorsInNewDocument();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // Test of a misbehaving printer driver which only returns access-denied
  // errors.  The expected events for this are:
  // 1.  Update print settings.
  // 2.  A print job is started, but has an access-denied error.
  // 3.  A retry to start the print job with adjusted access will still fail.
  // 4.  An error dialog is shown.
  // 5.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 6.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/6);

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kAccessDenied);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(cancel_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingRenderPageAccessDenied) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForAccessDeniedErrorsInRenderPrintedPage();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // No attempt to retry is made if an access-denied error occurs when trying
  // to render a page.  The expected events for this are:
  // 1.  Update print settings.
  // 2.  A print job is started.
  // 3.  Rendering for 1 page of document of content fails with access denied.
  // 4.  An error dialog is shown.
  // 5.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 6.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/6);

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kAccessDenied);
  EXPECT_EQ(render_printed_page_count(), 0);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(cancel_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingMultipageMidJobError) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  // Delay rendering until all pages have been sent, to avoid any race
  // conditions related to error handling.  This is to ensure that page 3 is in
  // the service queued for processing, before we let page 2 be processed and
  // have it trigger an error that could affect page 3 processing.
  PrimeForDelayedRenderingUntilPage(/*page_number=*/3);
  PrimeForRenderingErrorOnPage(/*page_number=*/2);

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Update print settings.
  // 2.  Start the print job.
  // 3.  First page render callback shows success.
  // 4.  Second page render callback shows failure.  Will start failure
  //     processing to cancel the print job.
  // 5.  A printing error dialog is displayed.
  // 6.  Third page render callback will show it was canceled (due to prior
  //     failure).  This is disregarded by the browser, since the job has
  //     already been canceled.
  // 7.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 8.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/8);

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
  // First failure page is `kFailed`, but is followed by another page with
  // status `kCanceled`.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kCanceled);
  EXPECT_EQ(render_printed_page_count(), 1);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(cancel_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}
#endif  // BUILDFLAG(IS_WIN)

// TODO(crbug.com/1008222)  Include Windows once XPS print pipeline is added.
#if !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingRenderDocumentAccessDenied) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForAccessDeniedErrorsInRenderPrintedDocument();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // No attempt to retry is made if an access-denied error occurs when trying
  // to render a document.  The expected events for this are:
  // 1.  Update print settings.
  // 2.  A print job is started.
  // 3.  Rendering for 1 page of document of content fails with access denied.
  // 4.  An error dialog is shown.
  // 5.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 6.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/6);

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kAccessDenied);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(cancel_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}
#endif  // !BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingDocumentDoneAccessDenied) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForAccessDeniedErrorsInDocumentDone();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // No attempt to retry is made if an access-denied error occurs when trying
  // do wrap-up a rendered document.  The expected events are:
  // 1.  Update print settings.
  // 2.  A print job is started.
  // 3.  Rendering for 1 page of document of content.
  // 4.  Document done results in an access-denied error.
  // 5.  An error dialog is shown.
  // 6.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 7.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/7);

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1008222)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 1);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kAccessDenied);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(cancel_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       SystemPrintFromPrintPreview) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  if (GetParam() == PrintBackendFeatureVariation::kInBrowserProcess) {
#if BUILDFLAG(IS_WIN)
    // There are no callbacks that trigger for print stages with in-browser
    // printing for the Windows case.  The only expected event for this is to
    // wait for the one print job to be destroyed, to ensure printing finished
    // cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/1);
#else
    // Once the transition to system print is initiated, the expected events
    // are:
    // 1.  Use default settings.
    // 2.  Ask the user for settings.
    // 3.  Wait until all processing for DidPrintDocument is known to have
    //     completed, to ensure printing finished cleanly before completing the
    //     test.
    // 4.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/4);
#endif  // BUILDFLAG(IS_WIN)
  } else {
#if BUILDFLAG(IS_WIN)
    // Once the transition to system print is initiated, the expected events
    // are:
    // 1.  Update print settings.
    // 2.  A print job is started.
    // 3.  Rendering for 1 page of document of content.
    // 4.  Completes with document done.
    // 5.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
#else
    // Once the transition to system print is initiated, the expected events
    // are:
    // 1.  A print job is started.
    // 2.  Rendering for 1 page of document of content.
    // 3.  Completes with document done.
    // 4.  Wait until all processing for DidPrintDocument is known to have
    //     completed, to ensure printing finished cleanly before completing the
    //     test.
    // 5.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
#endif  // BUILDFLAG(IS_WIN)
  }
  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  if (GetParam() == PrintBackendFeatureVariation::kInBrowserProcess) {
#if !BUILDFLAG(IS_WIN)
    EXPECT_TRUE(did_get_settings_with_ui());
    EXPECT_EQ(did_print_document_count(), 1);
#endif
    EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1"),
              *document_print_settings());
  } else {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(IS_WIN)
    // TODO(crbug.com/1008222)  Include Windows coverage of
    // RenderPrintedDocument() once XPS print pipeline is added.
    EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(render_printed_page_count(), 1);
#else
    EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
    EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1"),
              *document_print_settings());
#else
    // TODO(crbug.com/1414968):  Update the expectation once system print
    // settings are properly reflected at start of job print.
    EXPECT_NE(*test::MakeUserModifiedPrintSettings("printer1"),
              *document_print_settings());
#endif
  }
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

#if BUILDFLAG(IS_WIN)
// This test is Windows-only, since it is the only platform which can invoke
// the system print dialog from within `PrintingContext::UpdatePrintSettings()`.
// From that system dialog we can cause a cancel to occur.
// TODO(crbug.com/809738):  Expand this to also cover in-browser, once an
// appropriate signal is available to use for tracking expected events.
// TODO(crbug.com/1435566):  Enable this test once it works without the need
// for --single-process-tests flag.
IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       DISABLED_SystemPrintFromPrintPreviewCancelRetry) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForCancelInAskUserForSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Update the print settings, which indicates to cancel the print
  //     request.  No further printing calls are made.
  // No print job is created because of such an early cancel.
  SetNumExpectedMessages(/*num=*/1);

  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kCanceled);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 0);

  // Now try to initiate the system print from a Print Preview again.
  // Same number of expected events.
  ResetNumReceivedMessages();

  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kCanceled);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 0);
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartBasicPrint) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings.
  // 3.  A print job is started.
  // 4.  The print compositor will complete generating the document.
  // 5.  The document is rendered.
  // 6.  Receive document done notification.
  // 7.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/7);
#else
  // The expected events for this are:
  // 1.  Get default settings, followed by asking user for settings.  This is
  //     invoked from the browser process, so there is no override to observe
  //     this.  Then a print job is started.
  // 2.  The print compositor will complete generating the document.
  // 3.  The document is rendered.
  // 4.  Receive document done notification.
  // 5.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/5);
#endif

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  // macOS and Linux currently have to invoke a system dialog from within the
  // browser process.  There is not a callback to capture the result in these
  // cases.
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(ask_user_for_settings_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1"),
            *document_print_settings());
#else
  // TODO(crbug.com/1414968):  Update the expectation once system print
  // settings are properly reflected at start of job print.
  EXPECT_NE(*test::MakeUserModifiedPrintSettings("printer1"),
            *document_print_settings());
#endif
  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1008222)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 1);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(did_print_document_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

// TODO(crbug.com/1375007): Very flaky on Mac and slightly on Linux.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_StartBasicPrintCancel DISABLED_StartBasicPrintCancel
#else
#define MAYBE_StartBasicPrintCancel StartBasicPrintCancel
#endif
IN_PROC_BROWSER_TEST_F(SystemAccessProcessInBrowserPrintBrowserTest,
                       MAYBE_StartBasicPrintCancel) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForCancelInAskUserForSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings, which indicates to cancel the print
  //     request.  No further printing calls are made.
  // No print job is created because of such an early cancel.
  SetNumExpectedMessages(/*num=*/2);
#else
  // TODO(crbug.com/1375007)  Need a good signal to use for test expectations.
#endif

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  EXPECT_TRUE(did_use_default_settings());
  EXPECT_TRUE(did_get_settings_with_ui());
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(did_print_document_count(), 0);
  EXPECT_EQ(print_job_destruction_count(), 0);

  // `PrintBackendService` should never be used when printing in-browser.
  EXPECT_FALSE(print_backend_service_use_detected());
}

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       StartBasicPrintFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForErrorsInNewDocument();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  if (GetParam() == PrintBackendFeatureVariation::kInBrowserProcess) {
    // There are only partial overrides to track most steps in the printing
    // pipeline, so the expected events for this are:
    // 1.  Gets default settings.
    // 2.  Asks user for settings.
    // 3.  A print job is started, but that fails.  There is no override to
    //     this notice directly.  This does cause an error dialog to be shown.
    // 4.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    // 5.  The renderer will have initiated printing of document, which could
    //     invoke the print compositor.  Wait until all processing for
    //     DidPrintDocument is known to have completed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
  } else {
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    // The expected events for this are:
    // 1.  Gets default settings.
    // 2.  Asks user for settings.
    // 3.  A print job is started, which fails.
    // 4.  An error dialog is shown.
    // 5.  The print job is canceled.  The callback from the service could occur
    //     after the print job has been destroyed.
    // 6.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    // 7.  The renderer will have initiated printing of document, which could
    //     invoke the print compositor.  Wait until all processing for
    //     DidPrintDocument is known to have completed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/7);
#else
    // The expected events for this are:
    // 1.  Get default settings, followed by asking user for settings.  This is
    //     invoked from the browser process, so there is no override to observe
    //     this.  Then a print job is started, which fails.
    // 2.  An error dialog is shown.
    // 3.  The print job is canceled.  The callback from the service could occur
    //     after the print job has been destroyed.
    // 4.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    // 5.  The print compositor will have started to generate the document.
    //     Wait until that is known to have completed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  }

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kFailed);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(
      cancel_count(),
      GetParam() == PrintBackendFeatureVariation::kInBrowserProcess ? 0 : 1);
  EXPECT_EQ(did_print_document_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

// macOS and Linux currently have to invoke a system dialog from within the
// browser process.  There is not a callback to capture the result in these
// cases.
// TODO(crbug.com/1374188)  Re-enable for Linux once `AskForUserSettings()` is
// able to be pushed OOP for Linux.
#undef MAYBE_StartBasicPrintCancel
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_StartBasicPrintCancel DISABLED_StartBasicPrintCancel
#else
#define MAYBE_StartBasicPrintCancel StartBasicPrintCancel
#endif
IN_PROC_BROWSER_TEST_P(SystemAccessProcessServicePrintBrowserTest,
                       MAYBE_StartBasicPrintCancel) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForCancelInAskUserForSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings, which indicates to cancel the print
  //     request.  No further printing calls are made.
  // No print job is created because of such an early cancel.
  SetNumExpectedMessages(/*num=*/2);

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(ask_user_for_settings_result(), mojom::ResultCode::kCanceled);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(did_print_document_count(), 0);
  EXPECT_EQ(print_job_construction_count(), 0);
}

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartBasicPrintConcurrent) {
  // Linux allows concurrent printing, so regular setup for printing is needed.
  // It is uninteresting to do a full print in this case, it is better to exit
  // the print sequence early, but at a known time after when PrintNow() would
  // fail if concurrent printing isn't allowed.  That can be achieved by just
  // canceling out from asking for settings.
#if BUILDFLAG(IS_LINUX)
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForCancelInAskUserForSettings();
#endif

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  TestPrintViewManager* print_view_manager =
      SetUpAndReturnPrintViewManager(web_contents);

  // Pretend that a window has started a system print.
  absl::optional<PrintBackendServiceManager::ClientId> client_id =
      PrintBackendServiceManager::GetInstance().RegisterQueryWithUiClient();
  ASSERT_TRUE(client_id.has_value());

#if BUILDFLAG(IS_LINUX)
  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings, which indicates to cancel the print
  //     request.  No further printing calls are made.
  // No print job is created because of such an early cancel.
  SetNumExpectedMessages(/*num=*/2);
#endif

  // Now initiate a system print that would exist concurrently with that.
  StartBasicPrint(web_contents);

#if BUILDFLAG(IS_LINUX)
  WaitUntilCallbackReceived();
#endif

  const absl::optional<bool>& result = print_view_manager->print_now_result();
  ASSERT_TRUE(result.has_value());
  // With the exception of Linux, concurrent system print is not allowed.
#if BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(*result);
#else
  // The denied concurrent print is silent without an error.
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_FALSE(*result);
#endif

  // Cleanup before test shutdown.
  PrintBackendServiceManager::GetInstance().UnregisterClient(*client_id);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       SystemPrintFromPrintPreviewConcurrent) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // Pretend that another tab has started a system print.
  // TODO(crbug.com/809738)  Improve on this test by using a persistent fake
  // system print dialog.
  absl::optional<PrintBackendServiceManager::ClientId> client_id =
      PrintBackendServiceManager::GetInstance().RegisterQueryWithUiClient();
  ASSERT_TRUE(client_id.has_value());

  // Now do a print preview which will try to switch to doing system print.
#if BUILDFLAG(IS_LINUX)
  // The expected events for this are:
  // 1.  Start printing.
  // 2.  The document is rendered.
  // 3.  Receive document done notification.
  // 4.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/4);

  constexpr bool kWaitForCallback = true;
#else
  // Inability to support this should be detected immediately without needing
  // to wait for callback.
  constexpr bool kWaitForCallback = false;
#endif

  SystemPrintFromPreviewOnceReadyAndLoaded(kWaitForCallback);

  // With the exception of Linux, concurrent system print is not allowed.
  ASSERT_TRUE(system_print_registration_succeeded().has_value());
#if BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(*system_print_registration_succeeded());
#else
  // The denied concurrent print is silent without an error.
  EXPECT_FALSE(*system_print_registration_succeeded());
  EXPECT_EQ(error_dialog_shown_count(), 0u);
#endif

  // Cleanup before test shutdown.
  PrintBackendServiceManager::GetInstance().UnregisterClient(*client_id);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

IN_PROC_BROWSER_TEST_P(SystemAccessProcessServicePrintBrowserTest,
                       StartBasicPrintUseDefaultFails) {
  PrimeForFailInUseDefaultSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  // The expected events for this are:
  // 1.  Get the default settings, which fails.
  // 2.  The print error dialog is shown.
  // No print job is created from such an early failure.
  SetNumExpectedMessages(/*num=*/2);
#else
  // When get default settings is invoked from the browser process, there is no
  // override to observe this failure.  This means the expected events are:
  // 1.  The print error dialog is shown.
  // No print job is created from such an early failure.
  SetNumExpectedMessages(/*num=*/1);
#endif

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kFailed);
#endif
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(did_print_document_count(), 0);
  EXPECT_EQ(print_job_construction_count(), 0);
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

#endif  //  BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
class TestPrintViewManagerForContentAnalysis : public TestPrintViewManager {
 public:
  class Observer : public PrintViewManagerBase::Observer {
   public:
    void OnPrintNow(const content::RenderFrameHost* rfh) override {
      print_now_called_ = true;
    }

    void OnScriptedPrint() override { scripted_print_called_ = true; }

    bool print_now_called() const { return print_now_called_; }

    bool scripted_print_called() const { return scripted_print_called_; }

   private:
    bool print_now_called_ = false;
    bool scripted_print_called_ = false;
  };

  static TestPrintViewManagerForContentAnalysis* CreateForWebContents(
      content::WebContents* web_contents) {
    auto manager =
        std::make_unique<TestPrintViewManagerForContentAnalysis>(web_contents);
    auto* manager_ptr = manager.get();
    web_contents->SetUserData(PrintViewManager::UserDataKey(),
                              std::move(manager));
    return manager_ptr;
  }

  explicit TestPrintViewManagerForContentAnalysis(
      content::WebContents* web_contents)
      : TestPrintViewManager(web_contents) {
    AddObserver(observer_);
    PrintViewManager::SetReceiverImplForTesting(this);
  }

  ~TestPrintViewManagerForContentAnalysis() override {
    PrintViewManager::SetReceiverImplForTesting(nullptr);
  }

  void WaitOnScanning() { scanning_run_loop_.Run(); }

  void WaitOnPreview() { preview_run_loop_.Run(); }

  bool print_now_called() const { return observer_.print_now_called(); }

  bool scripted_print_called() const {
    return observer_.scripted_print_called();
  }

  const absl::optional<bool>& preview_allowed() const {
    return preview_allowed_;
  }

#if BUILDFLAG(IS_CHROMEOS)
  void set_allowed_by_dlp(bool allowed) { allowed_by_dlp_ = allowed; }
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  void OnGotSnapshotCallback(
      base::OnceCallback<void(bool should_proceed)> callback,
      enterprise_connectors::ContentAnalysisDelegate::Data data,
      content::GlobalRenderFrameHostId rfh_id,
      mojom::DidPrintDocumentParamsPtr params) override {
    ASSERT_TRUE(web_contents());
    ASSERT_TRUE(params);
    EXPECT_TRUE(params->content->metafile_data_region.IsValid());
    EXPECT_EQ(data.url,
              web_contents()->GetOutermostWebContents()->GetLastCommittedURL());

    PrintViewManager::OnGotSnapshotCallback(
        std::move(callback), std::move(data), rfh_id, std::move(params));
  }

  void OnCompositedForContentAnalysis(
      base::OnceCallback<void(bool should_proceed)> callback,
      enterprise_connectors::ContentAnalysisDelegate::Data data,
      content::GlobalRenderFrameHostId rfh_id,
      mojom::PrintCompositor::Status status,
      base::ReadOnlySharedMemoryRegion page_region) override {
    EXPECT_TRUE(content::RenderFrameHost::FromID(rfh_id));
    EXPECT_EQ(status, mojom::PrintCompositor::Status::kSuccess);

    // The settings passed to this function should match the content of the
    // print Connector policy.
    EXPECT_EQ(data.settings.tags.size(), 1u);
    EXPECT_TRUE(base::Contains(data.settings.tags, "dlp"));
    EXPECT_TRUE(data.settings.cloud_or_local_settings.is_cloud_analysis());
    EXPECT_EQ(data.settings.cloud_or_local_settings.dm_token(), kFakeDmToken);
    EXPECT_EQ(data.settings.block_until_verdict,
              enterprise_connectors::BlockUntilVerdict::kBlock);
    EXPECT_TRUE(data.settings.block_large_files);
    EXPECT_EQ(data.url,
              web_contents()->GetOutermostWebContents()->GetLastCommittedURL());

    // The snapshot should be valid and populated.
    EXPECT_TRUE(LooksLikePdf(page_region.Map().GetMemoryAsSpan<char>()));

    PrintViewManager::OnCompositedForContentAnalysis(
        base::BindOnce(
            [](base::OnceCallback<void(bool should_proceed)> callback,
               base::RunLoop* scanning_run_loop, bool allowed) {
              std::move(callback).Run(allowed);
              scanning_run_loop->Quit();
            },
            std::move(callback), &scanning_run_loop_),
        std::move(data), rfh_id, status, std::move(page_region));
  }

#if BUILDFLAG(IS_CHROMEOS)
  void OnDlpPrintingRestrictionsChecked(
      content::GlobalRenderFrameHostId rfh_id,
      base::OnceCallback<void(bool should_proceed)> callback,
      bool should_proceed) override {
    PrintViewManager::OnDlpPrintingRestrictionsChecked(
        rfh_id, std::move(callback), allowed_by_dlp_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void CompleteScriptedPrint(content::RenderFrameHost* rfh,
                             mojom::ScriptedPrintParamsPtr params,
                             ScriptedPrintCallback callback) override {
    std::move(callback).Run(nullptr);

    for (auto& observer : GetObservers()) {
      observer.OnScriptedPrint();
    }
  }

 private:
  void PrintPreviewRejectedForTesting() override {
    preview_allowed_ = false;
    preview_run_loop_.Quit();
  }

  void PrintPreviewAllowedForTesting() override {
    preview_allowed_ = true;
    preview_run_loop_.Quit();
  }

#if BUILDFLAG(IS_CHROMEOS)
  bool allowed_by_dlp_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Indicates whether the preview was allowed after checking against content
  // analysis and DLP (if on CrOS). This is unpopulated until then.
  absl::optional<bool> preview_allowed_;

  base::RunLoop preview_run_loop_;
  base::RunLoop scanning_run_loop_;
  Observer observer_;
};

struct ContentAnalysisTestCase {
  bool content_analysis_allows_print = false;
  bool oop_enabled = false;
};

class ContentAnalysisPrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase,
      public testing::WithParamInterface<ContentAnalysisTestCase> {
 public:
  ContentAnalysisPrintBrowserTest() {
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken(kFakeDmToken));
    enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::FakeContentAnalysisDelegate::Create,
            base::DoNothing(),
            base::BindRepeating(
                &ContentAnalysisPrintBrowserTest::ScanningResponse,
                base::Unretained(this)),
            kFakeDmToken));
    enterprise_connectors::ContentAnalysisDialog::SetShowDialogDelayForTesting(
        base::Milliseconds(0));
  }

  void SetUp() override {
    test_printing_context_factory()->SetPrinterNameForSubsequentContexts(
        "printer_name");
    SystemAccessProcessPrintBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    safe_browsing::SetAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::AnalysisConnector::PRINT,
        R"({
          "service_provider": "google",
          "enable": [ {"url_list": ["*"], "tags": ["dlp"]} ],
          "block_until_verdict": 1,
          "block_large_files": true
        })");
    SystemAccessProcessPrintBrowserTestBase::SetUpOnMainThread();
  }

  bool content_analysis_allows_print() const {
    return GetParam().content_analysis_allows_print;
  }
  bool UseService() override { return GetParam().oop_enabled; }
  bool SandboxService() override { return true; }

  enterprise_connectors::ContentAnalysisResponse ScanningResponse(
      const std::string& contents,
      const base::FilePath& path) {
    enterprise_connectors::ContentAnalysisResponse response;

    auto* result = response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

    if (!content_analysis_allows_print()) {
      auto* rule = result->add_triggered_rules();
      rule->set_rule_name("blocking_rule_name");
      rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
    }

    return response;
  }

  int new_document_called_count() {
    return test_printing_context_factory()->new_document_called_count();
  }
};

class ContentAnalysisScriptedPreviewlessPrintBrowserTest
    : public ContentAnalysisPrintBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    cmd_line->AppendSwitch(switches::kDisablePrintPreview);
    ContentAnalysisPrintBrowserTest::SetUpCommandLine(cmd_line);
  }

  void RunScriptedPrintTest(const std::string& script) {
    AddPrinter("printer_name");

    if (UseService()) {
      // Test does not do extra cleanup beyond the check for analysis
      // permission.
      SkipPersistentContextsCheckOnShutdown();
    }

    ASSERT_TRUE(embedded_test_server()->Started());
    GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    auto* print_view_manager =
        TestPrintViewManagerForContentAnalysis::CreateForWebContents(
            web_contents);
    content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(), script);

    print_view_manager->WaitOnScanning();
    ASSERT_EQ(print_view_manager->scripted_print_called(),
              content_analysis_allows_print());

    // Validate that `NewDocument` was never call as that can needlessly
    // prompt the user.
    ASSERT_EQ(new_document_called_count(), 0);
  }
};

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ContentAnalysisPrintBrowserTest, PrintNow) {
  AddPrinter("printer_name");

  if (UseService()) {
    // Test does not do extra cleanup beyond the check for analysis permission.
    SkipPersistentContextsCheckOnShutdown();
  }

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager =
      TestPrintViewManagerForContentAnalysis::CreateForWebContents(
          web_contents);

  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
             /*print_renderer=*/mojo::NullAssociatedRemote(),
#endif
             /*print_preview_disabled=*/true,
             /*has_selection=*/false);

  print_view_manager->WaitOnScanning();

  // PrintNow uses the same code path as scripted prints to scan printed pages,
  // so print_now_called() should always happen and scripted_print_called()
  // should be called with the same result that is expected from scanning.
  ASSERT_TRUE(print_view_manager->print_now_called());
  ASSERT_EQ(print_view_manager->scripted_print_called(),
            content_analysis_allows_print());

  // Validate that `NewDocument` was never call as that can needlessly
  // prompt the user.
  ASSERT_EQ(new_document_called_count(), 0);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisPrintBrowserTest, PrintWithPreview) {
  AddPrinter("printer_name");

  if (UseService()) {
    // Test does not do extra cleanup beyond the check for analysis permission.
    SkipPersistentContextsCheckOnShutdown();
  }

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager =
      TestPrintViewManagerForContentAnalysis::CreateForWebContents(
          web_contents);

  test::StartPrint(browser()->tab_strip_model()->GetActiveWebContents());

  print_view_manager->WaitOnScanning();
  ASSERT_EQ(print_view_manager->preview_allowed(),
            content_analysis_allows_print());

  // Validate that `NewDocument` was never call as that can needlessly
  // prompt the user.
  ASSERT_EQ(new_document_called_count(), 0);
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisScriptedPreviewlessPrintBrowserTest,
                       DocumentExecPrint) {
  RunScriptedPrintTest("document.execCommand('print');");
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisScriptedPreviewlessPrintBrowserTest,
                       WindowPrint) {
  RunScriptedPrintTest("window.print()");
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ContentAnalysisPrintBrowserTest,
                       BlockedByDLPThenNoContentAnalysis) {
  AddPrinter("printer_name");
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager =
      TestPrintViewManagerForContentAnalysis::CreateForWebContents(
          web_contents);
  print_view_manager->set_allowed_by_dlp(false);

  test::StartPrint(browser()->tab_strip_model()->GetActiveWebContents());

  print_view_manager->WaitOnPreview();
  ASSERT_TRUE(print_view_manager->preview_allowed().has_value());
  ASSERT_FALSE(print_view_manager->preview_allowed().value());

  // This is always 0 because printing is always blocked by the DLP policy.
  ASSERT_EQ(new_document_called_count(), 0);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(
    All,
    ContentAnalysisPrintBrowserTest,
    testing::Values(
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/true,
                                /*oop_enabled=*/true},
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/true,
                                /*oop_enabled=*/false},
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/false,
                                /*oop_enabled=*/true},
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/false,
                                /*oop_enabled=*/false}));

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
INSTANTIATE_TEST_SUITE_P(
    All,
    ContentAnalysisScriptedPreviewlessPrintBrowserTest,
    testing::Values(
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/true,
                                /*oop_enabled=*/true},
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/true,
                                /*oop_enabled=*/false},
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/false,
                                /*oop_enabled=*/true},
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/false,
                                /*oop_enabled=*/false}));
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

}  // namespace printing
