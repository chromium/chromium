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
#include "components/enterprise/common/proto/connectors.pb.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/browser/printing/print_job_worker_oop.h"
#include "chrome/browser/printing/printer_query_oop.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"  // nogncheck
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"  // nogncheck
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/822505)  ChromeOS uses different testing setup that isn't
// hooked up to make use of `TestPrintingContext` yet.
#error "ChromeOS not supported here yet"
#endif

namespace printing {

namespace {

#if BUILDFLAG(ENABLE_OOP_PRINTING) && !BUILDFLAG(IS_CHROMEOS)
constexpr gfx::SizeF kLetterPhysicalSize = gfx::SizeF(612, 792);
constexpr gfx::RectF kLetterPrintableArea = gfx::RectF(5, 5, 602, 782);
constexpr gfx::SizeF kLegalPhysicalSize = gfx::SizeF(612, 1008);
constexpr gfx::RectF kLegalPrintableArea = gfx::RectF(5, 5, 602, 998);

// The default margins are set to 1.0cm in //printing/print_settings.cc, which
// is about 28 printer units. The resulting content size is 556 x 736 for
// Letter, and similarly is 556 x 952 for Legal.
constexpr gfx::SizeF kLetterExpectedContentSize = gfx::SizeF(556, 736);
constexpr gfx::SizeF kLegalExpectedContentSize = gfx::SizeF(556, 952);
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
constexpr char kFakeDmToken[] = "fake-dm-token";

// The policy values below correspond to the schema described in
// https://chromeenterprise.google/policies/#OnPrintEnterpriseConnector
constexpr char kCloudAnalysisBlockingPolicy[] = R"({
  "service_provider": "google",
  "enable": [ {"url_list": ["*"], "tags": ["dlp"]} ],
  "block_until_verdict": 1,
  "block_large_files": true
})";

constexpr char kCloudAnalysisNonBlockingPolicy[] = R"({
  "service_provider": "google",
  "enable": [ {"url_list": ["*"], "tags": ["dlp"]} ],
  "block_until_verdict": 0,
  "block_large_files": true
})";

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
constexpr char kLocalAnalysisPolicy[] = R"({
  "service_provider": "local_user_agent",
  "enable": [ {"url_list": ["*"], "tags": ["dlp"]} ],
  "block_until_verdict": 1,
  "block_large_files": true
})";
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

using OnDidCompositeForContentAnalysis =
    base::RepeatingCallback<void(bool allowed)>;
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

class TestPrintJobWorker : public PrintJobWorker {
 public:
  // Callbacks to run for overrides.
  struct PrintCallbacks {
    ErrorCheckCallback error_check_callback;
    OnUseDefaultSettingsCallback did_use_default_settings_callback;
    OnGetSettingsWithUICallback did_get_settings_with_ui_callback;
  };

  TestPrintJobWorker(
      std::unique_ptr<PrintingContext::Delegate> printing_context_delegate,
      std::unique_ptr<PrintingContext> printing_context,
      PrintJob* print_job,
      PrintCallbacks* callbacks)
      : PrintJobWorker(std::move(printing_context_delegate),
                       std::move(printing_context),
                       print_job),
        callbacks_(callbacks) {}
  TestPrintJobWorker(const TestPrintJobWorker&) = delete;
  TestPrintJobWorker& operator=(const TestPrintJobWorker&) = delete;

  // `PrintJobWorker` overrides:
  void Cancel() override {
    callbacks_->error_check_callback.Run(mojom::ResultCode::kCanceled);
    PrintJobWorker::Cancel();
  }

 private:
  // `PrintJobWorker` overrides:
  void OnCancel() override {
    callbacks_->error_check_callback.Run(mojom::ResultCode::kCanceled);
    PrintJobWorker::OnCancel();
  }
  void OnFailure() override {
    callbacks_->error_check_callback.Run(mojom::ResultCode::kFailed);
    PrintJobWorker::OnFailure();
  }

  const raw_ptr<PrintCallbacks> callbacks_;
};

class TestPrinterQuery : public PrinterQuery {
 public:
  TestPrinterQuery(content::GlobalRenderFrameHostId rfh_id,
                   TestPrintJobWorker::PrintCallbacks* callbacks)
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

  std::unique_ptr<PrintJobWorker> CreatePrintJobWorker(
      PrintJob* print_job) override {
    return std::make_unique<TestPrintJobWorker>(
        std::move(printing_context_delegate_), std::move(printing_context_),
        print_job, callbacks_);
  }

  const raw_ptr<TestPrintJobWorker::PrintCallbacks> callbacks_;
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
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    OnDidUseDefaultSettingsCallback did_use_default_settings_callback;
    OnDidAskUserForSettingsCallback did_ask_user_for_settings_callback;
#else
    // Need to use the base class version of callbacks when the system dialog
    // must be displayed from the browser process.
    OnUseDefaultSettingsCallback did_use_default_settings_callback;
    OnGetSettingsWithUICallback did_get_settings_with_ui_callback;
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

  const raw_ptr<PrintCallbacks> callbacks_;
};

class TestPrinterQueryOop : public PrinterQueryOop {
 public:
  TestPrinterQueryOop(content::GlobalRenderFrameHostId rfh_id,
                      bool simulate_spooling_memory_errors,
                      TestPrintJobWorkerOop::PrintCallbacks* callbacks)
      : PrinterQueryOop(rfh_id),
        simulate_spooling_memory_errors_(simulate_spooling_memory_errors),
        callbacks_(callbacks) {}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
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
#else
  void UseDefaultSettings(SettingsCallback callback) override {
    DVLOG(1) << "Observed: invoke use default settings";
    PrinterQueryOop::UseDefaultSettings(std::move(callback));
    callbacks_->did_use_default_settings_callback.Run();
  }

  void GetSettingsWithUI(uint32_t document_page_count,
                         bool has_selection,
                         bool is_scripted,
                         SettingsCallback callback) override {
    DVLOG(1) << "Observed: invoke get settings with UI";
    PrinterQueryOop::GetSettingsWithUI(document_page_count, has_selection,
                                       is_scripted, std::move(callback));
    callbacks_->did_get_settings_with_ui_callback.Run();
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

  std::unique_ptr<PrintJobWorkerOop> CreatePrintJobWorkerOop(
      PrintJob* print_job) override {
    return std::make_unique<TestPrintJobWorkerOop>(
        std::move(printing_context_delegate_), std::move(printing_context_),
        print_document_client_id(), context_id(), print_job,
        print_from_system_dialog(), simulate_spooling_memory_errors_,
        callbacks_);
  }

  bool simulate_spooling_memory_errors_;
  const raw_ptr<TestPrintJobWorkerOop::PrintCallbacks> callbacks_;
};
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

class SystemAccessProcessPrintBrowserTestBase
    : public PrintBrowserTest,
      public PrintJob::Observer,
      public PrintViewManagerBase::TestObserver {
 public:
  SystemAccessProcessPrintBrowserTestBase() = default;
  ~SystemAccessProcessPrintBrowserTestBase() override = default;

  virtual bool UseService() = 0;

  // Only of interest when `UseService()` returns true.
  virtual bool SandboxService() = 0;

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  // Only of interest for content analysis tests. This will enable/disable the
  // kEnableLocalScanAfterPreview and kEnableCloudScanAfterPreview features so
  // that content analysis is done after the printing settings are picked from a
  // dialog.
  virtual bool EnableContentAnalysisAfterDialog() = 0;
#endif

  void SetUpFeatures() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
    if (EnableContentAnalysisAfterDialog()) {
      enabled_features.push_back({features::kEnableLocalScanAfterPreview, {}});
      enabled_features.push_back({features::kEnableCloudScanAfterPreview, {}});
    } else {
      disabled_features.push_back(features::kEnableLocalScanAfterPreview);
      disabled_features.push_back(features::kEnableCloudScanAfterPreview);
    }
#endif
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (UseService()) {
      enabled_features.push_back(
          {features::kEnableOopPrintDrivers,
           {{features::kEnableOopPrintDriversJobPrint.name, "true"},
            {features::kEnableOopPrintDriversSandbox.name,
             SandboxService() ? "true" : "false"}}});
    } else {
      disabled_features.push_back(features::kEnableOopPrintDrivers);
    }
#endif
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  void SetUp() override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    SetUpFeatures();

    if (UseService()) {
      // Safe to use `base::Unretained(this)` since this testing class
      // necessarily must outlive all interactions from the tests which will
      // run through `TestPrintJobWorkerOop`, the user of these callbacks.
      test_print_job_worker_oop_callbacks_.error_check_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OopErrorCheck,
              base::Unretained(this));
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
      test_print_job_worker_oop_callbacks_.did_use_default_settings_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidUseDefaultSettings,
              base::Unretained(this));
      test_print_job_worker_oop_callbacks_.did_ask_user_for_settings_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidAskUserForSettings,
              base::Unretained(this));
#else
      test_print_job_worker_oop_callbacks_.did_use_default_settings_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnUseDefaultSettings,
              base::Unretained(this));
      test_print_job_worker_oop_callbacks_.did_get_settings_with_ui_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnGetSettingsWithUI,
              base::Unretained(this));
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
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
      test_print_job_worker_callbacks_.error_check_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::InProcessErrorCheck,
              base::Unretained(this));
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

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // `PrintBackendServiceTestImpl` does a debug check on shutdown that there
  // are no residual persistent printing contexts left in the service.  For
  // tests which are known to break this (either by design, for test simplicity
  // or because a related change is only partly implemented), use this method
  // to notify the service to not DCHECK on such a condition.
  void SkipPersistentContextsCheckOnShutdown() {
    print_backend_service_->SkipPersistentContextsCheckOnShutdown();
  }

  // PrintViewManagerBase::TestObserver:
  void OnRegisterSystemPrintClient(bool succeeded) override {
    system_print_registration_succeeded_ = succeeded;
  }
#endif

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

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  void OnCompositedForContentAnalysis(bool allowed) {
    ++composited_for_content_analysis_count_;
    CheckForQuit();
  }
#endif

  TestPrintViewManager* SetUpAndReturnPrintViewManager(
      content::WebContents* web_contents) {
    // Safe to use `base::Unretained(this)` since this testing class
    // necessarily must outlive all interactions from the tests which will
    // run through `PrintViewManagerBase`, which is what causes new jobs to
    // be created and use this callback.
    auto manager = std::make_unique<TestPrintViewManager>(
        web_contents,
        base::BindRepeating(
            &SystemAccessProcessPrintBrowserTestBase::OnCreatedPrintJob,
            base::Unretained(this)));
    manager->AddTestObserver(*this);
#if BUILDFLAG(IS_WIN)
    if (simulate_pdf_conversion_error_on_page_index_.has_value()) {
      manager->set_simulate_pdf_conversion_error_on_page_index(
          *simulate_pdf_conversion_error_on_page_index_);
    }
#endif
    TestPrintViewManager* manager_ptr = manager.get();
    web_contents->SetUserData(PrintViewManager::UserDataKey(),
                              std::move(manager));
    return manager_ptr;
  }

  void SetUpPrintViewManager(content::WebContents* web_contents) {
    std::ignore = SetUpAndReturnPrintViewManager(web_contents);
  }

  void PrintAfterPreviewIsReadyAndLoaded() {
    PrintAfterPreviewIsReadyAndLoaded(PrintParams());
  }

  void PrintAfterPreviewIsReadyAndLoaded(const PrintParams& params) {
    PrintAfterPreviewIsReadyAndMaybeLoaded(params, /*wait_for_loaded=*/true);
  }

  void PrintAfterPreviewIsReadyAndMaybeLoaded(const PrintParams& params,
                                              bool wait_for_loaded) {
    // First invoke the Print Preview dialog with requested method.
    content::WebContents* preview_dialog =
        wait_for_loaded ? PrintAndWaitUntilPreviewIsReadyAndLoaded(params)
                        : PrintAndWaitUntilPreviewIsReady(params);
    ASSERT_TRUE(preview_dialog);

    // Print Preview is completely ready, can now initiate printing.
    // This script locates and clicks the Print button.
    const char kScript[] = R"(
      const button = document.getElementsByTagName('print-preview-app')[0]
                       .$['sidebar']
                       .shadowRoot.querySelector('print-preview-button-strip')
                       .shadowRoot.querySelector('.action-button');
      button.click();)";
    auto result = content::ExecJs(preview_dialog, kScript);
    // TODO(crbug.com/1472464):  Update once it is known if the assertion
    // should not happen if the failure is just because the renderer
    // terminated.
    // If the renderer terminates, it will return a failing result.  It has
    // been observed in other tests that sometimes the renderer terminates
    // and the test was successful; all the needed callbacks happened before
    // ExecJs() returned.
    // Add a warning for the logs to help with debugging, and then only do
    // the assert check after having done the wait.
    // If the renderer terminated but the printing was all successful, then
    // `WaitUntilCallbackReceived()` should return successfully, and any crash
    // logs should show the assert.  Otherwise the crashes for this bug should
    // change to become the test timeouts.
    if (!result) {
      LOG(ERROR) << "ExecJs() failed; if reason is because the renderer "
                    "terminated, it is possibly okay?";
      LOG(ERROR) << result.message();
    }
    WaitUntilCallbackReceived();
    ASSERT_TRUE(result);
  }

  void AdjustMediaAfterPreviewIsReadyAndLoaded() {
    // First invoke the Print Preview dialog with `StartPrint()`.
    content::WebContents* preview_dialog =
        PrintAndWaitUntilPreviewIsReadyAndLoaded();
    ASSERT_TRUE(preview_dialog);

    // Initial Print Preview is completely ready.
    // Create an observer and modify the paper size.  This will initiate another
    // preview render.
    // The default paper size is first in the list at index zero, so choose
    // the second item from the list to cause a change.
    TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
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
    content::WebContents* preview_dialog =
        PrintAndWaitUntilPreviewIsReadyAndLoaded();
    ASSERT_TRUE(preview_dialog);

    // Print Preview is completely ready, can now initiate printing.
    // This script locates and clicks the "Print using system dialog",
    // which is still enabled even if it is hidden.
    const char kPrintWithSystemDialogScript[] = R"(
      const printSystemDialog =
          document.getElementsByTagName('print-preview-app')[0]
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

#if BUILDFLAG(IS_MAC)
  void OpenPdfInPreviewOnceReadyAndLoaded() {
    // First invoke the Print Preview dialog with `StartPrint()`.
    content::WebContents* preview_dialog =
        PrintAndWaitUntilPreviewIsReadyAndLoaded();
    ASSERT_TRUE(preview_dialog);

    // Print Preview is completely ready, can now initiate printing.
    // This script locates and clicks "Open PDF in Preview", which is still
    // enabled even if it is hidden.
    const char kOpenPdfWithPreviewScript[] = R"(
      const openPdfInPreview =
          document.getElementsByTagName('print-preview-app')[0]
              .$['sidebar']
              .shadowRoot.querySelector('print-preview-link-container')
              .$['openPdfInPreviewLink'];
      openPdfInPreview.click();)";
    ASSERT_TRUE(content::ExecJs(preview_dialog, kOpenPdfWithPreviewScript));
    WaitUntilCallbackReceived();
  }
#endif  // BUILDFLAG(IS_MAC)

  void PrimeAsRepeatingErrorGenerator() { reset_errors_after_check_ = false; }

#if BUILDFLAG(IS_WIN)
  void PrimeForPdfConversionErrorOnPageIndex(uint32_t page_index) {
    simulate_pdf_conversion_error_on_page_index_ = page_index;
  }
#endif

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
  absl::optional<mojom::ResultCode> in_process_last_error_result_code() const {
    return in_process_last_error_result_code_;
  }

  int print_job_construction_count() const {
    return print_job_construction_count_;
  }
  int print_job_destruction_count() const {
    return print_job_destruction_count_;
  }
  int did_print_document_count() const { return did_print_document_count_; }
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  int composited_for_content_analysis_count() const {
    return composited_for_content_analysis_count_;
  }
#endif

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

  void OopErrorCheck(mojom::ResultCode result) {
    // Interested to reset any trigger for causing access-denied errors, so
    // that retry logic has a chance to be exercised and succeed.
    if (result == mojom::ResultCode::kAccessDenied) {
      ResetForNoAccessDeniedErrors();
    }
  }

  void InProcessErrorCheck(mojom::ResultCode result) {
    // This is expected to only be called with unsuccessful results.
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    in_process_last_error_result_code_ = result;
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
  TestPrintJobWorker::PrintCallbacks test_print_job_worker_callbacks_;
  TestPrintJobWorkerOop::PrintCallbacks test_print_job_worker_oop_callbacks_;
  CreatePrinterQueryCallback test_create_printer_query_callback_;
  absl::optional<bool> system_print_registration_succeeded_;
  bool did_use_default_settings_ = false;
  bool did_get_settings_with_ui_ = false;
  bool print_backend_service_use_detected_ = false;
  bool simulate_spooling_memory_errors_ = false;
#if BUILDFLAG(IS_WIN)
  absl::optional<uint32_t> simulate_pdf_conversion_error_on_page_index_;
#endif
  mojo::Remote<mojom::PrintBackendService> test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
  absl::optional<mojom::ResultCode> in_process_last_error_result_code_;
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
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  int composited_for_content_analysis_count_ = 0;
#endif
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
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  bool EnableContentAnalysisAfterDialog() override { return false; }
#endif
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
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  bool EnableContentAnalysisAfterDialog() override { return false; }
#endif
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemAccessProcessServicePrintBrowserTest,
    testing::Values(PrintBackendFeatureVariation::kOopSandboxedService,
                    PrintBackendFeatureVariation::kOopUnsandboxedService));

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

class SystemAccessProcessInBrowserPrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase {
 public:
  SystemAccessProcessInBrowserPrintBrowserTest() = default;
  ~SystemAccessProcessInBrowserPrintBrowserTest() override = default;

  bool UseService() override { return false; }
  bool SandboxService() override { return false; }
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  bool EnableContentAnalysisAfterDialog() override { return false; }
#endif
};

#if BUILDFLAG(ENABLE_OOP_PRINTING)

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
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  bool EnableContentAnalysisAfterDialog() override { return false; }
#endif
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
  GURL url(embedded_test_server()->GetURL("/printing/3_pages.html"));
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
  //     finished cleanly before completing the test.
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
  GURL url(embedded_test_server()->GetURL("/printing/3_pages.html"));
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

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       // TODO(crbug.com/1491616): Re-enable this test
                       DISABLED_StartPrintingPdfConversionFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForPdfConversionErrorOnPageIndex(/*page_index=*/1);

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/3_pages.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  if (UseService()) {
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  A print job is started.
    // 3.  PDF conversion fails, which results in the print job being
    //     canceled.
    // 4.  Wait for the print job to be destroyed, to ensure printing finished
    //     cleanly before completing the test.
    // No error dialog is shown.
    SetNumExpectedMessages(/*num=*/4);
  } else {
    // There are no callbacks for print stages with in-browser printing.  So
    // the print job is started, but that fails, and there is no capturing of
    // that result.
    // The expected events for this are:
    // 1.  Print job is started, but is canceled and destroyed due to failure
    //     during PDF conversion failure.
    // No error dialog is shown.
    SetNumExpectedMessages(/*num=*/1);
  }
  PrintAfterPreviewIsReadyAndLoaded();

  // No tracking of start printing or cancel callbacks for in-browser tests,
  // only for OOP.
  if (UseService()) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
    // TODO(crbug.com/1008222)  Include Windows coverage of
    // RenderPrintedDocument() once XPS print pipeline is added.
    EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kFailed);
  } else {
    EXPECT_THAT(in_process_last_error_result_code(),
                testing::Optional(mojom::ResultCode::kCanceled));
  }
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       StartPrintingFails) {
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

  if (UseService()) {
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  A print job is started, but that fails.
    // 3.  An error dialog is shown.
    // 4.  The print job is canceled.  The callback from the service could occur
    //     after the print job has been destroyed.
    // 5.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
  } else {
    // There are no callbacks for print stages with in-browser printing.  So
    // the print job is started, but that fails and gets canceled.
    // The expected events for this are:
    // 1.  An error dialog is shown.
    // 2.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/2);
  }

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(error_dialog_shown_count(), 1u);
  if (UseService()) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kFailed);
    EXPECT_EQ(cancel_count(), 1);
  } else {
    EXPECT_THAT(in_process_last_error_result_code(),
                testing::Optional(mojom::ResultCode::kCanceled));
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

  if (UseService()) {
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  A print job is started, but results in a cancel.
    // 3.  The print job is canceled.
    // 4.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/4);
  } else {
    // A print job is started, but results in a cancel.  There are no callbacks
    // to notice the start job.  The expected events for this are:
    // 1.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/1);
  }

  PrintAfterPreviewIsReadyAndLoaded();

  if (UseService()) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kCanceled);
    EXPECT_EQ(cancel_count(), 1);
  } else {
    EXPECT_THAT(in_process_last_error_result_code(),
                testing::Optional(mojom::ResultCode::kCanceled));
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
  GURL url(embedded_test_server()->GetURL("/printing/3_pages.html"));
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

  if (UseService()) {
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
    // 1.  Use default settings.
    // 2.  Ask the user for settings.
    // 3.  A print job is started.
    // 4.  Rendering for 1 page of document of content.
    // 5.  Completes with document done.
    // 6.  Wait until all processing for DidPrintDocument is known to have
    //     completed, to ensure printing finished cleanly before completing the
    //     test.
    // 7.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/7);
#endif  // BUILDFLAG(IS_WIN)
  } else {
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
  }
  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  if (UseService()) {
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
    EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1"),
              *document_print_settings());
  } else {
#if !BUILDFLAG(IS_WIN)
    EXPECT_TRUE(did_get_settings_with_ui());
    EXPECT_EQ(did_print_document_count(), 1);
#endif
    EXPECT_TRUE(!in_process_last_error_result_code().has_value());
    EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1"),
              *document_print_settings());
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

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  // macOS and Linux currently have to invoke a system dialog from within the
  // browser process.  There is not a callback to capture the result in these
  // cases.
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(ask_user_for_settings_result(), mojom::ResultCode::kSuccess);
#else
  EXPECT_TRUE(did_use_default_settings());
  EXPECT_TRUE(did_get_settings_with_ui());
#endif
  EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1"),
            *document_print_settings());
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       StartBasicPrintCancel) {
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

  if (UseService()) {
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(ask_user_for_settings_result(), mojom::ResultCode::kCanceled);
#else
    EXPECT_TRUE(did_use_default_settings());
    EXPECT_TRUE(did_get_settings_with_ui());
#endif
  } else {
    EXPECT_TRUE(did_use_default_settings());
    EXPECT_TRUE(did_get_settings_with_ui());

    // `PrintBackendService` should never be used when printing in-browser.
    EXPECT_FALSE(print_backend_service_use_detected());
  }
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(did_print_document_count(), 0);
  EXPECT_EQ(print_job_destruction_count(), 0);
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

  if (UseService()) {
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
  } else {
    // There are only partial overrides to track most steps in the printing
    // pipeline, so the expected events for this are:
    // 1.  Gets default settings.
    // 2.  Asks user for settings.
    // 3.  A print job is started, but that fails and gets canceled.  This does
    //     cause an error dialog to be shown.
    // 4.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    // 5.  The renderer will have initiated printing of document, which could
    //     invoke the print compositor.  Wait until all processing for
    //     DidPrintDocument is known to have completed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
  }

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  if (UseService()) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kFailed);
    EXPECT_EQ(cancel_count(), 1);
  } else {
    EXPECT_THAT(in_process_last_error_result_code(),
                testing::Optional(mojom::ResultCode::kCanceled));
  }
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(did_print_document_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       StartBasicPrintPdfConversionFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForPdfConversionErrorOnPageIndex(/*page_index=*/1);

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/3_pages.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  if (UseService()) {
    // The expected events for this are:
    // 1.  Gets default settings.
    // 2.  Asks user for settings.
    // 3.  A print job is started.
    // 4.  Notified of DidPrintDocument(), that composition of the print
    //     document has completed.
    // 5.  The PDF conversion fails, resulting in canceling the print job.
    // 6.  The print job is destroyed.
    // No error dialog is shown.
    SetNumExpectedMessages(/*num=*/6);
  } else {
    // There are only partial overrides to track most steps in the printing
    // pipeline, so the expected events for this are:
    // 1.  Gets default settings.
    // 2.  Asks user for settings.
    // 3.  A print job is started, but is canceled due to failure during PDF
    //     conversion.
    // 4.  The renderer will have initiated printing of document, which could
    //     invoke the print compositor.  Wait until all processing for
    //     DidPrintDocument is known to have completed, to ensure printing
    //     finished cleanly before completing the test.
    // No error dialog is shown.
    SetNumExpectedMessages(/*num=*/4);
  }

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  if (UseService()) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
    // TODO(crbug.com/1008222)  Include Windows coverage of
    // RenderPrintedDocument() once XPS print pipeline is added.
    EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kFailed);
  } else {
    EXPECT_THAT(in_process_last_error_result_code(),
                testing::Optional(mojom::ResultCode::kCanceled));
  }
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_CONCURRENT_BASIC_PRINT_DIALOGS)

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartBasicPrintConcurrentAllowed) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

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

  // The expected events for this are:
  // 1.  Gets default settings.
  // 2.  Asks user for settings.
  // 3.  Start the print job.
  // 4.  Rendering for 1 page of document of content.
  // 5.  Completes with document done.
  // 6.  Wait until all processing for DidPrintDocument is known to have
  //     completed, to ensure printing finished cleanly before completing the
  //     test.
  // 7.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/7);

  // Now initiate a system print that would exist concurrently with that.
  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  EXPECT_THAT(print_view_manager->print_now_result(), testing::Optional(true));

  // Cleanup before test shutdown.
  PrintBackendServiceManager::GetInstance().UnregisterClient(*client_id);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       SystemPrintFromPrintPreviewConcurrentAllowed) {
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
  // The expected events for this are:
  // 1.  Gets default settings.
  // 2.  Asks user for settings.
  // 3.  Start the print job.
  // 4.  Rendering for 1 page of document of content.
  // 5.  Completes with document done.
  // 6.  Wait until all processing for DidPrintDocument is known to have
  //     completed, to ensure printing finished cleanly before completing the
  //     test.
  // 7.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/7);

  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  // Concurrent system print is allowed.
  EXPECT_THAT(system_print_registration_succeeded(), testing::Optional(true));

  // Cleanup before test shutdown.
  PrintBackendServiceManager::GetInstance().UnregisterClient(*client_id);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#else  // BUILDFLAG(ENABLE_CONCURRENT_BASIC_PRINT_DIALOGS)

IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartBasicPrintConcurrentNotAllowed) {
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

  // Now initiate a system print that would exist concurrently with that.
  StartBasicPrint(web_contents);

  // Concurrent system print is not allowed.
  EXPECT_THAT(print_view_manager->print_now_result(), testing::Optional(false));
  // The denied concurrent print is silent without an error.
  EXPECT_EQ(error_dialog_shown_count(), 0u);

  // Cleanup before test shutdown.
  PrintBackendServiceManager::GetInstance().UnregisterClient(*client_id);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_F(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       SystemPrintFromPrintPreviewConcurrentNotAllowed) {
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
  // Inability to support this should be detected immediately without needing
  // to wait for callback.
  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/false);

  // Concurrent system print is not allowed.
  EXPECT_THAT(system_print_registration_succeeded(), testing::Optional(false));
  // The denied concurrent print is silent without an error.
  EXPECT_EQ(error_dialog_shown_count(), 0u);

  // Cleanup before test shutdown.
  PrintBackendServiceManager::GetInstance().UnregisterClient(*client_id);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#endif  // BUILDFLAG(ENABLE_CONCURRENT_BASIC_PRINT_DIALOGS)

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

  // The expected events for this are:
  // 1.  Get the default settings, which fails.
  // 2.  The print error dialog is shown.
  // No print job is created from such an early failure.
  SetNumExpectedMessages(/*num=*/2);

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kFailed);
#else
  EXPECT_TRUE(did_use_default_settings());
#endif
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(did_print_document_count(), 0);
  EXPECT_EQ(print_job_construction_count(), 0);
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest, OpenPdfInPreview) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  if (UseService()) {
    // The expected events for this are:
    // 1.  Update printer settings.
    // 2.  A print job is started.
    // 3.  Rendering for 1 page of document of content.
    // 4.  Completes with document done.
    // 5.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
  } else {
    // The expected events for this are:
    // 1.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/1);
  }
  OpenPdfInPreviewOnceReadyAndLoaded();

  if (UseService()) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  } else {
    EXPECT_FALSE(in_process_last_error_result_code().has_value());
  }
  EXPECT_TRUE(destination_is_preview());
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}
#endif  // BUILDFLAG(IS_MAC)

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
class TestPrintViewManagerForContentAnalysis : public TestPrintViewManager {
 public:
  class Observer : public PrintViewManagerBase::TestObserver {
   public:
    void OnPrintNow(const content::RenderFrameHost* rfh) override {
      print_now_called_ = true;
    }

    void OnScriptedPrint() override { scripted_print_called_ = true; }

    void OnPrintPreviewDone() override {
      if (on_print_preview_done_) {
        std::move(on_print_preview_done_).Run();
      }
    }

    bool print_now_called() const { return print_now_called_; }

    bool scripted_print_called() const { return scripted_print_called_; }

    void set_on_print_preview_done_closure(base::OnceClosure closure) {
      on_print_preview_done_ = std::move(closure);
    }

   private:
    bool print_now_called_ = false;
    bool scripted_print_called_ = false;
    base::OnceClosure on_print_preview_done_;
  };

  TestPrintViewManagerForContentAnalysis(
      content::WebContents* web_contents,
      const char* policy_value,
      absl::optional<enterprise_connectors::ContentAnalysisRequest::Reason>
          expected_reason,
      OnDidCreatePrintJobCallback create_print_job_callback,
      OnDidCompositeForContentAnalysis composite_for_content_analysis_callback)
      : TestPrintViewManager(web_contents,
                             std::move(create_print_job_callback)),
        expected_reason_(expected_reason),
        policy_value_(policy_value),
        did_composite_for_content_analysis_callback_(
            std::move(composite_for_content_analysis_callback)) {
    AddTestObserver(observer_);
    PrintViewManager::SetReceiverImplForTesting(this);
  }

  ~TestPrintViewManagerForContentAnalysis() override {
    PrintViewManager::SetReceiverImplForTesting(nullptr);
  }

  void WaitOnPreview() { preview_run_loop_.Run(); }

  bool print_now_called() const { return observer_.print_now_called(); }

  bool scripted_print_called() const {
    return observer_.scripted_print_called();
  }

  const absl::optional<bool>& preview_allowed() const {
    return preview_allowed_;
  }

  int got_snapshot_count() const { return got_snapshot_count_; }

#if BUILDFLAG(IS_CHROMEOS)
  void set_allowed_by_dlp(bool allowed) { allowed_by_dlp_ = allowed; }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void set_on_print_preview_done_closure(base::OnceClosure closure) {
    observer_.set_on_print_preview_done_closure(std::move(closure));
  }

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
    // TODO(http://b/285243428): Change `expected_reason_` to a normal enum
    // value instead of an optional to check it in every test.
    if (expected_reason_) {
      EXPECT_EQ(data.reason, *expected_reason_);
    }

    PrintViewManager::OnGotSnapshotCallback(
        std::move(callback), std::move(data), rfh_id, std::move(params));
    got_snapshot_count_++;
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
    if (data.settings.cloud_or_local_settings.is_cloud_analysis()) {
      EXPECT_EQ(data.settings.cloud_or_local_settings.dm_token(), kFakeDmToken);
    } else {
      EXPECT_EQ(data.settings.cloud_or_local_settings.local_path(),
                "path_user");
      EXPECT_TRUE(data.settings.cloud_or_local_settings.user_specific());
    }
    EXPECT_TRUE(ExpectedBlockUntilVerdict(data.settings.block_until_verdict));
    EXPECT_TRUE(data.settings.block_large_files);
    EXPECT_EQ(data.url,
              web_contents()->GetOutermostWebContents()->GetLastCommittedURL());
    // TODO(http://b/285243428): Change `expected_reason_` to a normal enum
    // value instead of an optional to check it in every test.
    if (expected_reason_) {
      EXPECT_EQ(data.reason, *expected_reason_);
    }

    // The snapshot should be valid and populated.
    EXPECT_TRUE(LooksLikePdf(page_region.Map().GetMemoryAsSpan<char>()));

    PrintViewManager::OnCompositedForContentAnalysis(
        base::BindOnce(
            [](base::OnceCallback<void(bool should_proceed)> callback,
               OnDidCompositeForContentAnalysis*
                   did_composite_for_content_analysis_callback,
               bool allowed) {
              std::move(callback).Run(allowed);
              did_composite_for_content_analysis_callback->Run(allowed);
            },
            std::move(callback), &did_composite_for_content_analysis_callback_),
        std::move(data), rfh_id, status, std::move(page_region));
  }

  void ContentAnalysisBeforePrintingDocument(
      enterprise_connectors::ContentAnalysisDelegate::Data scanning_data,
      scoped_refptr<base::RefCountedMemory> print_data,
      const gfx::Size& page_size,
      const gfx::Rect& content_area,
      const gfx::Point& offsets) override {
    // The settings passed to this function should match the content of the
    // print Connector policy.
    EXPECT_EQ(scanning_data.settings.tags.size(), 1u);
    EXPECT_TRUE(base::Contains(scanning_data.settings.tags, "dlp"));
    if (scanning_data.settings.cloud_or_local_settings.is_cloud_analysis()) {
      EXPECT_EQ(scanning_data.settings.cloud_or_local_settings.dm_token(),
                kFakeDmToken);
    } else {
      EXPECT_EQ(scanning_data.settings.cloud_or_local_settings.local_path(),
                "path_user");
      EXPECT_TRUE(
          scanning_data.settings.cloud_or_local_settings.user_specific());
    }
    EXPECT_TRUE(
        ExpectedBlockUntilVerdict(scanning_data.settings.block_until_verdict));
    EXPECT_TRUE(scanning_data.settings.block_large_files);
    EXPECT_EQ(scanning_data.url,
              web_contents()->GetOutermostWebContents()->GetLastCommittedURL());
    // TODO(http://b/285243428): Change `expected_reason_` to a normal enum
    // value instead of an optional to check it in every test.
    if (expected_reason_) {
      EXPECT_EQ(scanning_data.reason, *expected_reason_);
    }

    // The data of the document should be a valid PDF as this code should be
    // called as the print job is about to start printing.
    EXPECT_TRUE(LooksLikePdf(base::span<const char>(
        print_data->front_as<const char>(), print_data->size())));

    TestPrintViewManager::ContentAnalysisBeforePrintingDocument(
        std::move(scanning_data), print_data, page_size, content_area, offsets);
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
    TestPrintViewManager::CompleteScriptedPrint(rfh, std::move(params),
                                                std::move(callback));

    for (auto& observer : GetTestObservers()) {
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

  bool ExpectedBlockUntilVerdict(
      enterprise_connectors::BlockUntilVerdict block_until_verdict) {
    if (policy_value_ == kCloudAnalysisNonBlockingPolicy) {
      return block_until_verdict ==
             enterprise_connectors::BlockUntilVerdict::kNoBlock;
    }

    return block_until_verdict ==
           enterprise_connectors::BlockUntilVerdict::kBlock;
  }

#if BUILDFLAG(IS_CHROMEOS)
  bool allowed_by_dlp_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Indicates whether the preview was allowed after checking against content
  // analysis and DLP (if on CrOS). This is `absl::nullopt` until then.
  absl::optional<bool> preview_allowed_;

  // Used to validate the corresponding `ContentAnalysisDelegate::Data` passed
  // in various content analysis-related functions. A value of `absl::nullopt`
  // means the value shouldn't be checked.
  absl::optional<enterprise_connectors::ContentAnalysisRequest::Reason>
      expected_reason_;

  // Used to validate the corresponding `ContentAnalysisDelegate::Data` passed
  // in various content analysis-related functions. Corresponds to the value
  // return by `PolicyValue()` for the current test.
  const char* policy_value_ = nullptr;

  base::RunLoop preview_run_loop_;
  OnDidCompositeForContentAnalysis did_composite_for_content_analysis_callback_;
  Observer observer_;

  // Tracks how many times a snapshot is obtained for doing analysis.
  int got_snapshot_count_ = 0;
};

class ContentAnalysisPrintBrowserTestBase
    : public SystemAccessProcessPrintBrowserTestBase {
 public:
  ContentAnalysisPrintBrowserTestBase() {
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken(kFakeDmToken));
    enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
            base::DoNothing(),
            base::BindRepeating(
                &ContentAnalysisPrintBrowserTestBase::ScanningResponse,
                base::Unretained(this)),
            kFakeDmToken));

    // These overrides make the overall tests faster as the content analysis
    // dialog won't stay in each state for mandatory minimum times.
    enterprise_connectors::ContentAnalysisDialog::
        SetMinimumPendingDialogTimeForTesting(base::Milliseconds(0));
    enterprise_connectors::ContentAnalysisDialog::SetShowDialogDelayForTesting(
        base::Milliseconds(0));
    enterprise_connectors::ContentAnalysisDialog::
        SetSuccessDialogTimeoutForTesting(base::Milliseconds(0));
  }

  enterprise_connectors::ContentAnalysisResponse ScanningResponse(
      const std::string& contents,
      const base::FilePath& path) {
    ++scanning_responses_;
    enterprise_connectors::ContentAnalysisResponse response;

    auto* result = response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

    if (!ContentAnalysisAllowsPrint()) {
      auto* rule = result->add_triggered_rules();
      rule->set_rule_name("blocking_rule_name");
      rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
    }

    return response;
  }

  void SetUp() override {
    test_printing_context_factory()->SetPrinterNameForSubsequentContexts(
        "printer_name");
    SystemAccessProcessPrintBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    SystemAccessProcessPrintBrowserTestBase::SetUpOnMainThread();
    enterprise_connectors::test::SetAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::AnalysisConnector::PRINT, PolicyValue());
  }

  void TearDownOnMainThread() override {
    enterprise_connectors::test::ClearAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::AnalysisConnector::PRINT);
    SystemAccessProcessPrintBrowserTestBase::TearDownOnMainThread();
  }

  TestPrintViewManagerForContentAnalysis*
  SetUpAndReturnPrintViewManagerForContentAnalysis(
      content::WebContents* web_contents,
      absl::optional<enterprise_connectors::ContentAnalysisRequest::Reason>
          expected_reason) {
    // Safe to use `base::Unretained(this)` since this testing class
    // necessarily must outlive all interactions from the tests which will
    // run through `PrintViewManagerBase`, which is what causes new jobs to
    // be created and use this callback.
    auto manager = std::make_unique<TestPrintViewManagerForContentAnalysis>(
        web_contents, PolicyValue(), expected_reason,
        base::BindRepeating(
            &SystemAccessProcessPrintBrowserTestBase::OnCreatedPrintJob,
            base::Unretained(this)),
        base::BindRepeating(&SystemAccessProcessPrintBrowserTestBase::
                                OnCompositedForContentAnalysis,
                            base::Unretained(this)));
    manager->AddTestObserver(*this);
    TestPrintViewManagerForContentAnalysis* manager_ptr = manager.get();
    web_contents->SetUserData(PrintViewManager::UserDataKey(),
                              std::move(manager));
    return manager_ptr;
  }

  int scanning_responses_count() { return scanning_responses_; }

  bool SandboxService() override { return true; }

  bool EnableContentAnalysisAfterDialog() override { return false; }

  int GetExpectedNewDocumentCalledCount() {
    return PrintAllowedOrNonBlockingPolicy() ? (UseService() ? 2 : 1) : 0;
  }

  // The value OnPrintEnterpriseConnector should be set to.
  virtual const char* PolicyValue() const = 0;

  // Whether content analysis should let printing proceed.
  virtual bool ContentAnalysisAllowsPrint() const = 0;

  // Helper to check if printing is allowed altogether or not. It's possible for
  // the policy to be set to be non-blocking and still obtain a "block" verdict
  // from a content analysis server, and in such a case the printing will be
  // allowed to proceed.
  bool PrintAllowedOrNonBlockingPolicy() {
    return ContentAnalysisAllowsPrint() ||
           PolicyValue() == kCloudAnalysisNonBlockingPolicy;
  }

 private:
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  enterprise_connectors::FakeContentAnalysisSdkManager sdk_manager_;
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  // Counts the number of times `ScanningResponse` is called, why is equivalent
  // to the number of times a printed page's bytes would reach a scanner.
  int scanning_responses_ = 0;
};

using ContentAnalysisBeforePrintPreviewVariation =
    testing::tuple<const char* /*policy_value*/,
                   bool /*content_analysis_allows_print*/,
                   bool /*oop_enabled*/>;

class ContentAnalysisBeforePrintPreviewBrowserTest
    : public ContentAnalysisPrintBrowserTestBase,
      public testing::WithParamInterface<
          ContentAnalysisBeforePrintPreviewVariation> {
 public:
  bool EnableContentAnalysisAfterDialog() override { return false; }
  const char* PolicyValue() const override { return std::get<0>(GetParam()); }
  bool ContentAnalysisAllowsPrint() const override {
    return std::get<1>(GetParam());
  }
  bool UseService() override { return std::get<2>(GetParam()); }
};

using ContentAnalysisAfterPrintPreviewVariation =
    testing::tuple<const char* /*policy_value*/,
                   bool /*content_analysis_allows_print*/,
                   bool /*oop_enabled*/>;

class ContentAnalysisAfterPrintPreviewBrowserTest
    : public ContentAnalysisPrintBrowserTestBase,
      public testing::WithParamInterface<
          ContentAnalysisAfterPrintPreviewVariation> {
 public:
  bool EnableContentAnalysisAfterDialog() override { return true; }

  const char* PolicyValue() const override { return std::get<0>(GetParam()); }
  bool ContentAnalysisAllowsPrint() const override {
    return std::get<1>(GetParam());
  }
  bool UseService() override { return std::get<2>(GetParam()); }

  // PrintJob::Observer:
  void OnCanceling() override { CheckForQuit(); }
};

using ContentAnalysisScriptedPreviewlessVariation =
    testing::tuple<const char* /*policy_value*/,
                   bool /*content_analysis_allows_print*/,
                   bool /*oop_enabled*/>;

class ContentAnalysisScriptedPreviewlessPrintBrowserTestBase
    : public ContentAnalysisPrintBrowserTestBase,
      public testing::WithParamInterface<
          ContentAnalysisScriptedPreviewlessVariation> {
 public:
  const char* PolicyValue() const override { return std::get<0>(GetParam()); }
  bool ContentAnalysisAllowsPrint() const override {
    return std::get<1>(GetParam());
  }
  bool UseService() override { return std::get<2>(GetParam()); }

  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    cmd_line->AppendSwitch(switches::kDisablePrintPreview);
    ContentAnalysisPrintBrowserTestBase::SetUpCommandLine(cmd_line);
  }
};

class ContentAnalysisScriptedPreviewlessPrintBeforeDialogBrowserTest
    : public ContentAnalysisScriptedPreviewlessPrintBrowserTestBase {
 public:
  bool EnableContentAnalysisAfterDialog() override { return false; }

  void RunScriptedPrintTest(const std::string& script) {
    AddPrinter("printer_name");

    ASSERT_TRUE(embedded_test_server()->Started());
    GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
        web_contents,
        enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

    if (PrintAllowedOrNonBlockingPolicy()) {
      if (UseService()) {
        // The expected events are:
        // 1.  The document is composited for content analysis.
        // 2.  The print job used for scanning is destroyed.
        // 3.  Get the default settings.
        // 4.  Ask the user for settings.
        // 5.  A print job is started.
        // 6.  The one page of the document is rendered.
        // 7.  Receive document done notification.
        // 8.  Wait until all processing for DidPrintDocument is known to have
        //     completed, to ensure printing finished cleanly before completing
        //     the test.
        // 9.  Wait for the one print job to be destroyed, to ensure printing
        //     finished cleanly before completing the test.
        SetNumExpectedMessages(/*num=*/9);
      } else {
        // The expected events for this are:
        // 1.  The document is composited for content analysis.
        // 2.  The print job used for scanning is destroyed.
        // 3.  Use default settings.
        // 4.  Ask the user for settings.
        // 5.  Wait until all processing for DidPrintDocument is known to have
        //     completed, to ensure printing finished cleanly before completing
        //     the test.
        // 6.  Wait for the actual printing job to be destroyed, to ensure
        //     printing finished cleanly before completing the test.
        SetNumExpectedMessages(/*num=*/6);
      }
    } else {
      // The expected events for this are:
      // 1.  Use default settings.
      // 2.  The document is composited for content analysis.
      // 3.  The print job used for scanning is destroyed.
      SetNumExpectedMessages(/*num=*/3);

      if (UseService()) {
        // When printing is denied, the printing context in the Print Backend
        // service leaks with no way to delete it.  It will persist there until
        // there is a gap with no printing activity from the user, at which
        // point the Print Backend service is shutdown.
        SkipPersistentContextsCheckOnShutdown();
      }
    }

    content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(), script);

    WaitUntilCallbackReceived();

    ASSERT_EQ(print_view_manager->scripted_print_called(),
              PrintAllowedOrNonBlockingPolicy());
    EXPECT_EQ(composited_for_content_analysis_count(), 1);
    EXPECT_EQ(scanning_responses_count(), 1);

    // Validate that `NewDocument()` is only called for actual printing, not as
    // part of content analysis, since that can needlessly prompt the user.
    // When printing OOP, an extra call for a new document will occur since it
    // gets called in both the browser process and in the Print Backend service.
    EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
  }
};

class ContentAnalysisScriptedPreviewlessPrintAfterDialogBrowserTest
    : public ContentAnalysisScriptedPreviewlessPrintBrowserTestBase {
 public:
  bool EnableContentAnalysisAfterDialog() override { return true; }

  void RunScriptedPrintTest(const std::string& script) {
    AddPrinter("printer_name");

    if (UseService() && !PrintAllowedOrNonBlockingPolicy()) {
      // This results in a stranded context left in the Print Backend service.
      // It will persist harmlessly until the service terminates after a short
      // period of no printing activity.
      SkipPersistentContextsCheckOnShutdown();
    }

    ASSERT_TRUE(embedded_test_server()->Started());
    GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
        web_contents,
        enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

    if (PrintAllowedOrNonBlockingPolicy()) {
      if (UseService()) {
        // The expected events are:
        // 1.  Get the default settings.
        // 2.  Ask the user for settings.
        // 3.  A print job is started.
        // 4.  The one page of the document is rendered.
        // 5.  Receive document done notification.
        // 6.  Wait until all processing for DidPrintDocument is known to have
        //     completed, to ensure printing finished cleanly before
        //     completing the test.
        // 7.  Wait for the one print job to be destroyed, to ensure printing
        //     finished cleanly before completing the test.
        SetNumExpectedMessages(/*num=*/7);
      } else {
        // The expected events for this are:
        // 1.  Use default settings.
        // 2.  Ask the user for settings.
        // 3.  The print compositor will complete generating the document.
        // 4.  The print job is destroyed.
        SetNumExpectedMessages(/*num=*/4);
      }
    } else {
      // The expected events for this are:
      // 1.  Use default settings.
      // 2.  Ask the user for settings.
      // 3.  Wait until all processing for DidPrintDocument is known to have
      //     completed, to ensure printing finished cleanly before
      //     completing the test.
      // 4.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/4);
    }

    content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(), script);

    WaitUntilCallbackReceived();

    ASSERT_TRUE(print_view_manager->scripted_print_called());
    EXPECT_EQ(composited_for_content_analysis_count(), 0);
    EXPECT_EQ(scanning_responses_count(), 1);

    // Validate that `NewDocument()` is only called for actual printing, not as
    // part of content analysis, since that can needlessly prompt the user.
    // When printing OOP, an extra call for a new document will occur since it
    // gets called in both the browser process and in the Print Backend service.
    EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
  }
};

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ContentAnalysisBeforePrintPreviewBrowserTest,
                       PrintWithPreview) {
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
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

  if (PrintAllowedOrNonBlockingPolicy()) {
    if (UseService()) {
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Update print settings.
      // 4.  A print job is started.
      // 5.  Rendering for 1 page of document of content.
      // 6.  Completes with document done.
      // 7.  Wait for the one print job to be destroyed, to ensure printing
      //     finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/7);
    } else {
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/3);
    }
    PrintAfterPreviewIsReadyAndLoaded();
  } else {
    // The expected events for this are:
    // 1.  The document is composited for content analysis.
    // 2.  The print job used for scanning is destroyed.
    SetNumExpectedMessages(/*num=*/2);
    test::StartPrint(web_contents);
    WaitUntilCallbackReceived();
  }

  ASSERT_EQ(print_view_manager->preview_allowed(),
            PrintAllowedOrNonBlockingPolicy());
  EXPECT_EQ(composited_for_content_analysis_count(), 1);
  EXPECT_EQ(print_view_manager->got_snapshot_count(), 1);
  EXPECT_EQ(scanning_responses_count(), 1);
  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisBeforePrintPreviewBrowserTest,
                       WindowDotPrint) {
  if (UseService()) {
    // TODO(crbug.com/1464566):  Enable this test variant once an extra system
    // dialog is not being displayed before analysis completes.
    GTEST_SKIP();
  }

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
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

  if (PrintAllowedOrNonBlockingPolicy()) {
    if (UseService()) {
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Update print settings.
      // 4.  A print job is started.
      // 5.  Rendering for 1 page of document of content.
      // 6.  Completes with document done.
      // 7.  Wait for the one print job to be destroyed, to ensure printing
      //     finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/7);
    } else {
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/3);
    }
    const PrintParams kParams{.invoke_method =
                                  InvokePrintMethod::kWindowDotPrint};
    PrintAfterPreviewIsReadyAndLoaded(kParams);
  } else {
    // The expected events for this are:
    // 1.  The document is composited for content analysis.
    // 2.  The print job used for scanning is destroyed.
    SetNumExpectedMessages(/*num=*/2);
    content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(),
                                "window.print();");
    WaitUntilCallbackReceived();
  }

  ASSERT_EQ(print_view_manager->preview_allowed(),
            PrintAllowedOrNonBlockingPolicy());
  EXPECT_EQ(composited_for_content_analysis_count(), 1);
  EXPECT_EQ(print_view_manager->got_snapshot_count(), 1);
  EXPECT_EQ(scanning_responses_count(), 1);
  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisBeforePrintPreviewBrowserTest,
                       SystemPrintFromPrintPreview) {
  AddPrinter("printer_name");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

#if BUILDFLAG(IS_WIN)
  // `PRINT_PREVIEW_PRINT` is expected here since scanning takes place before
  // the print preview dialog where the system dialog print is selected.
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);
#else
  // TODO(http://b/285243428): Update expectation once a second analysis scan
  // isn't done for system print from Print Preview.
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents, absl::nullopt);
#endif

  // Since the content analysis scan happens before the Print Preview dialog,
  // checking behavior when requesting the system print dialog from print
  // preview only is possible if the scan permits it.
  // TODO(http://b/266119859):  Update test behavior and expectations for when
  // scans are done after hitting Print from Print Preview.
  if (PrintAllowedOrNonBlockingPolicy()) {
    if (UseService()) {
#if BUILDFLAG(IS_WIN)
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Update print settings.
      // 4.  A second print job is started, for actual printing.
      // 5.  Rendering for 1 page of document of content.
      // 6.  Completes with document done.
      // 7.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/7);
#else
      // TODO(http://b/285243428):  Update expectation once a second analysis
      // scan isn't done for system print from Print Preview.
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning before Print Preview is destroyed.
      // 3.  Use default settings.
      // 4.  Ask the user for settings.
      // 5.  The document is composited again for content analysis.
      // 6.  The print job used for scanning before system print is destroyed.
      // 7.  A print job is started for actual printing.
      // 8.  The print compositor will complete generating the document.
      // 9.  Rendering for 1 page of document of content.
      // 10. Completes with document done.
      // 11. Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/11);
#endif
    } else {
#if BUILDFLAG(IS_WIN)
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/3);
#else
      // TODO(http://b/285243428):  Update expectation once a second analysis
      // scan isn't done for system print from Print Preview.
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  The document is composited again for content analysis.
      // 4.  The print job used for a second scan is destroyed.
      // 5.  Use default settings.
      // 6.  Ask the user for settings.
      // 7.  Wait until all processing for DidPrintDocument is known to have
      //     completed, to ensure printing finished cleanly before completing
      //     the test.
      // 8.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/8);
#endif
    }
    SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);
  } else {
    // The expected events for this are:
    // 1.  The document is composited for content analysis.
    // 2.  The print job used for scanning is destroyed.
    SetNumExpectedMessages(/*num=*/2);
    test::StartPrint(browser()->tab_strip_model()->GetActiveWebContents());
    WaitUntilCallbackReceived();
  }

  // TODO(http://b/266119859):  Change this check when scans are done after
  // clicking Print from Print Preview instead of before displaying the dialog.
  ASSERT_EQ(print_view_manager->preview_allowed(),
            PrintAllowedOrNonBlockingPolicy());
#if BUILDFLAG(IS_WIN)
  const int kCompositedForContentAnalysisCount = 1;
#else
  // TODO(http://b/285243428):  Update expectation once a second analysis scan
  // isn't done for system print from Print Preview.
  const int kCompositedForContentAnalysisCount =
      PrintAllowedOrNonBlockingPolicy() ? 2 : 1;
#endif
  EXPECT_EQ(composited_for_content_analysis_count(),
            kCompositedForContentAnalysisCount);
  EXPECT_EQ(scanning_responses_count(), kCompositedForContentAnalysisCount);

#if BUILDFLAG(IS_WIN)
  // One print job is always used to do scanning, and if printing is allowed
  // then a second print job will be used for actual printing.
  EXPECT_EQ(print_job_destruction_count(),
            PrintAllowedOrNonBlockingPolicy() ? 2 : 1);

  // There should be only one scan made, even though there could be up to two
  // printing dialogs presented to the user.
  EXPECT_EQ(print_view_manager->got_snapshot_count(), 1);
#else
  // TODO(http://b/285243428):  Update expectations to match Windows behavior
  // once a second analysis scan isn't done for system print from Print Preview.

  // A separate print job is always used for each scan, and if printing is
  // allowed then another print job will be used for actual printing.
  EXPECT_EQ(print_job_destruction_count(),
            PrintAllowedOrNonBlockingPolicy() ? 3 : 1);
  EXPECT_EQ(print_view_manager->got_snapshot_count(),
            PrintAllowedOrNonBlockingPolicy() ? 2 : 1);
#endif

  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(ContentAnalysisBeforePrintPreviewBrowserTest,
                       OpenPdfInPreviewFromPrintPreview) {
  AddPrinter("printer_name");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

  // Since the content analysis scan happens before the Print Preview dialog,
  // checking behavior when requesting opening in Preview from the print preview
  // preview only is possible if the scan permits it.
  if (PrintAllowedOrNonBlockingPolicy()) {
    if (UseService()) {
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning before Print Preview is destroyed.
      // 3.  Ask the user for settings.
      // 4.  A print job is started for actual printing.
      // 5.  The print compositor will complete generating the document.
      // 6.  Completes with document done.
      // 7.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/7);
    } else {
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/3);
    }
    OpenPdfInPreviewOnceReadyAndLoaded();
  } else {
    // The expected events for this are:
    // 1.  The document is composited for content analysis.
    // 2.  The print job used for scanning is destroyed.
    SetNumExpectedMessages(/*num=*/2);
    test::StartPrint(browser()->tab_strip_model()->GetActiveWebContents());
    WaitUntilCallbackReceived();
  }

  ASSERT_EQ(print_view_manager->preview_allowed(),
            PrintAllowedOrNonBlockingPolicy());
  EXPECT_EQ(composited_for_content_analysis_count(), 1);
  EXPECT_EQ(scanning_responses_count(), 1);

  // A separate print job is always used for each scan, and if printing is
  // allowed then another print job will be used for actual printing.
  EXPECT_EQ(print_job_destruction_count(),
            PrintAllowedOrNonBlockingPolicy() ? 2 : 1);
  EXPECT_EQ(print_view_manager->got_snapshot_count(),
            PrintAllowedOrNonBlockingPolicy() ? 1 : 1);

  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}
#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_P(ContentAnalysisAfterPrintPreviewBrowserTest,
                       PrintWithPreview) {
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
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

  if (PrintAllowedOrNonBlockingPolicy() && UseService()) {
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  A print job is started.
    // 3.  Rendering for 1 page of document of content.
    // 4.  Completes with document done.
    // 5.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
  } else {
    print_view_manager->set_on_print_preview_done_closure(base::BindOnce(
        &ContentAnalysisBeforePrintPreviewBrowserTest::CheckForQuit,
        base::Unretained(this)));
    // Expect an extra message for the print job created after content
    // analysis to be destroyed.
    SetNumExpectedMessages(/*num=*/PrintAllowedOrNonBlockingPolicy() ? 2 : 1);
  }

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_THAT(print_view_manager->preview_allowed(), testing::Optional(true));

  // Since the scanned document was the one shown in the print preview dialog,
  // no snapshotting should have taken place.
  EXPECT_EQ(composited_for_content_analysis_count(), 0);
  EXPECT_EQ(print_view_manager->got_snapshot_count(), 0);
  EXPECT_EQ(scanning_responses_count(), 1);

  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}

// TODO(crbug.com/1496991): Timeout on Mac
#if BUILDFLAG(IS_MAC)
#define MAYBE_PrintWithPreviewBeforeLoaded DISABLED_PrintWithPreviewBeforeLoaded
#else
#define MAYBE_PrintWithPreviewBeforeLoaded PrintWithPreviewBeforeLoaded
#endif
IN_PROC_BROWSER_TEST_P(ContentAnalysisAfterPrintPreviewBrowserTest,
                       MAYBE_PrintWithPreviewBeforeLoaded) {
  AddPrinter("printer_name");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

  if (PrintAllowedOrNonBlockingPolicy() && UseService()) {
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  A print job is started.
    // 3.  Rendering for 1 page of document of content.
    // 4.  Completes with document done.
    // 5.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/5);
  } else {
    print_view_manager->set_on_print_preview_done_closure(base::BindOnce(
        &ContentAnalysisBeforePrintPreviewBrowserTest::CheckForQuit,
        base::Unretained(this)));
    // Expect an extra message for the print job created after content
    // analysis to be destroyed.
    SetNumExpectedMessages(/*num=*/PrintAllowedOrNonBlockingPolicy() ? 2 : 1);
  }

  PrintAfterPreviewIsReadyAndMaybeLoaded(PrintParams(),
                                         /*wait_for_loaded=*/false);

  EXPECT_THAT(print_view_manager->preview_allowed(), testing::Optional(true));

  // Since the scanned document was the one shown in the print preview dialog,
  // no snapshotting should have taken place.
  EXPECT_EQ(composited_for_content_analysis_count(), 0);
  EXPECT_EQ(print_view_manager->got_snapshot_count(), 0);
  EXPECT_EQ(scanning_responses_count(), 1);

  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisAfterPrintPreviewBrowserTest,
                       SystemPrintFromPrintPreview) {
  AddPrinter("printer_name");

  if (UseService() && !PrintAllowedOrNonBlockingPolicy()) {
    // This results in a stranded context left in the Print Backend service.
    // It will persist harmlessly until the service terminates after a short
    // period of no printing activity.
    SkipPersistentContextsCheckOnShutdown();
  }

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

  if (PrintAllowedOrNonBlockingPolicy()) {
    if (UseService()) {
#if BUILDFLAG(IS_WIN)
      // The expected events for this are:
      // 1.  Update print settings.
      // 2.  A print job is started, for actual printing.
      // 3.  Rendering for 1 page of document of content.
      // 4.  Completes with document done.
      // 5.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/5);
#else
      // The expected events for this are:
      // 1.  Get the default settings.
      // 2.  Ask the user for settings.
      // 3.  A print job is started for actual printing.
      // 4.  The print compositor will complete generating the document.
      // 5.  Rendering for 1 page of document of content.
      // 6.  Completes with document done.
      // 7.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/7);
#endif  // BUILDFLAG(IS_WIN)
    } else {
#if BUILDFLAG(IS_WIN)
      // The expected event for this is:
      // 1.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/1);
#else
      // The expected events for this are:
      // 1.  Get the default settings.
      // 2.  Ask the user for settings.
      // 3.  The print compositor will complete generating the document.
      // 4.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/4);
#endif  // BUILDFLAG(IS_WIN)
    }
    SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);
  } else {
#if BUILDFLAG(IS_WIN)
    if (UseService()) {
      // The expected events for this are:
      // 1.  Update print settings.
      // 2.  The print job is cancelled.
      // 3.  The print job is destroyed.
      SetNumExpectedMessages(/*num=*/3);
    } else {
      // The expected events for this are:
      // 1.  The print job is cancelled.
      // 2.  The print job is destroyed.
      SetNumExpectedMessages(/*num=*/2);
    }
#else
    // The expected events for this are:
    // 1.  Use default settings.
    // 2.  Ask the user for settings.
    // 3.  The print compositor will complete generating the document.
    // 4.  The print job is cancelled.
    // 5.  The print job is destroyed.
    SetNumExpectedMessages(/*num=*/5);
#endif  // BUILDFLAG(IS_WIN)
    SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);
  }

  EXPECT_THAT(print_view_manager->preview_allowed(), testing::Optional(true));

  // TODO(crbug.com/1457901): Update these assertions once all cases for this
  // test are re-enabled.
  EXPECT_EQ(composited_for_content_analysis_count(), 0);
  EXPECT_EQ(print_job_destruction_count(), 1);
  EXPECT_EQ(print_view_manager->got_snapshot_count(), 0);
  EXPECT_EQ(scanning_responses_count(), 1);

  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(ContentAnalysisAfterPrintPreviewBrowserTest,
                       OpenPdfInPreviewFromPrintPreview) {
  AddPrinter("printer_name");

  if (UseService() && !PrintAllowedOrNonBlockingPolicy()) {
    // This results in a stranded context left in the Print Backend service.
    // It will persist harmlessly until the service terminates after a short
    // period of no printing activity.
    SkipPersistentContextsCheckOnShutdown();
  }

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

  if (PrintAllowedOrNonBlockingPolicy()) {
    if (UseService()) {
      // The expected events for this are:
      // 1.  Ask the user for settings.
      // 2.  A print job is started for actual printing.
      // 3.  The print compositor will complete generating the document.
      // 4.  Completes with document done.
      // 5.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/5);
    } else {
      // The expected events for this are:
      // 1.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/1);
    }
  } else {
    print_view_manager->set_on_print_preview_done_closure(base::BindOnce(
        &ContentAnalysisBeforePrintPreviewBrowserTest::CheckForQuit,
        base::Unretained(this)));
    // Expect an extra message for the print job created after content
    // analysis to be destroyed.
    SetNumExpectedMessages(/*num=*/PrintAllowedOrNonBlockingPolicy() ? 2 : 1);
  }
  OpenPdfInPreviewOnceReadyAndLoaded();

  EXPECT_THAT(print_view_manager->preview_allowed(), testing::Optional(true));

  EXPECT_EQ(composited_for_content_analysis_count(), 0);
  EXPECT_EQ(print_job_destruction_count(),
            PrintAllowedOrNonBlockingPolicy() ? 1 : 0);
  EXPECT_EQ(print_view_manager->got_snapshot_count(), 0);
  EXPECT_EQ(scanning_responses_count(), 1);

  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}
#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_P(
    ContentAnalysisScriptedPreviewlessPrintAfterDialogBrowserTest,
    PrintNow) {
  AddPrinter("printer_name");

  if (UseService() && !PrintAllowedOrNonBlockingPolicy()) {
    // This results in a stranded context left in the Print Backend service.
    // It will persist harmlessly until the service terminates after a short
    // period of no printing activity.
    SkipPersistentContextsCheckOnShutdown();
  }

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

  if (PrintAllowedOrNonBlockingPolicy()) {
    if (UseService()) {
      // The expected events are:
      // 1.  Get the default settings.
      // 2.  Ask the user for settings.
      // 3.  A print job is started.
      // 4.  The one page of the document is rendered.
      // 5.  Receive document done notification.
      // 6.  Wait until all processing for DidPrintDocument is known to have
      //     completed, to ensure printing finished cleanly before completing
      //     the test.
      // 7.  Wait for the one print job to be destroyed, to ensure printing
      //     finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/7);
    } else {
      // The expected events for this are:
      // 1.  Use default settings.
      // 2.  Ask the user for settings.
      // 3.  The print compositor will complete generating the document.
      // 4.  The print job is destroyed.
      SetNumExpectedMessages(/*num=*/4);
    }
  } else {
    // The expected events for this are:
    // 1.  Use default settings.
    // 2.  Ask the user for settings.
    // 3.  The print compositor will complete generating the document.
    // 4.  The print job is destroyed.
    SetNumExpectedMessages(/*num=*/4);
  }

  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_preview_disabled=*/true,
             /*has_selection=*/false);

  WaitUntilCallbackReceived();

  ASSERT_TRUE(print_view_manager->scripted_print_called());
  EXPECT_EQ(composited_for_content_analysis_count(), 0);
  EXPECT_EQ(scanning_responses_count(), 1);

  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}

IN_PROC_BROWSER_TEST_P(
    ContentAnalysisScriptedPreviewlessPrintAfterDialogBrowserTest,
    DocumentExecPrint) {
  RunScriptedPrintTest("document.execCommand('print');");
}

IN_PROC_BROWSER_TEST_P(
    ContentAnalysisScriptedPreviewlessPrintAfterDialogBrowserTest,
    WindowPrint) {
  RunScriptedPrintTest("window.print()");
}

IN_PROC_BROWSER_TEST_P(
    ContentAnalysisScriptedPreviewlessPrintBeforeDialogBrowserTest,
    PrintNow) {
  AddPrinter("printer_name");

  if (UseService() && !PrintAllowedOrNonBlockingPolicy()) {
    // This results in a stranded context left in the Print Backend service.
    // It will persist harmlessly until the service terminates after a short
    // period of no printing activity.
    SkipPersistentContextsCheckOnShutdown();
  }

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  auto* print_view_manager = SetUpAndReturnPrintViewManagerForContentAnalysis(
      web_contents,
      enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

  if (PrintAllowedOrNonBlockingPolicy()) {
    if (UseService()) {
      // The expected events are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Get the default settings.
      // 4.  Ask the user for settings.
      // 5.  A print job is started.
      // 6.  The one page of the document is rendered.
      // 7.  Receive document done notification.
      // 8.  Wait until all processing for DidPrintDocument is known to have
      //     completed, to ensure printing finished cleanly before completing
      //     the test.
      // 9.  Wait for the one print job to be destroyed, to ensure printing
      //     finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/9);
    } else {
      // The expected events for this are:
      // 1.  The document is composited for content analysis.
      // 2.  The print job used for scanning is destroyed.
      // 3.  Use default settings.
      // 4.  Ask the user for settings.
      // 5.  Wait until all processing for DidPrintDocument is known to have
      //     completed, to ensure printing finished cleanly before completing
      //     the test.
      // 6.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/6);
    }
  } else {
    // The expected events for this are:
    // 1.  Get the default settings.
    // 2.  The document is composited for content analysis.
    // 3.  The print job used for scanning is destroyed.
    SetNumExpectedMessages(/*num=*/3);
  }

  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_preview_disabled=*/true,
             /*has_selection=*/false);

  WaitUntilCallbackReceived();

  // PrintNow uses the same code path as scripted prints to scan printed
  // pages, so print_now_called() should always happen and
  // scripted_print_called() should be called with the same result that is
  // expected from scanning.
  EXPECT_TRUE(print_view_manager->print_now_called());
  EXPECT_EQ(print_view_manager->scripted_print_called(),
            PrintAllowedOrNonBlockingPolicy());
  EXPECT_EQ(composited_for_content_analysis_count(), 1);
  EXPECT_EQ(scanning_responses_count(), 1);

  // Validate that `NewDocument()` is only called for actual printing, not as
  // part of content analysis, since that can needlessly prompt the user.
  // When printing OOP, an extra call for a new document will occur since it
  // gets called in both the browser process and in the Print Backend service.
  EXPECT_EQ(new_document_called_count(), GetExpectedNewDocumentCalledCount());
}

IN_PROC_BROWSER_TEST_P(
    ContentAnalysisScriptedPreviewlessPrintBeforeDialogBrowserTest,
    DocumentExecPrint) {
  RunScriptedPrintTest("document.execCommand('print');");
}

IN_PROC_BROWSER_TEST_P(
    ContentAnalysisScriptedPreviewlessPrintBeforeDialogBrowserTest,
    WindowPrint) {
  RunScriptedPrintTest("window.print()");
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(ContentAnalysisBeforePrintPreviewBrowserTest,
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
          web_contents,
          enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);
  print_view_manager->set_allowed_by_dlp(false);

  test::StartPrint(browser()->tab_strip_model()->GetActiveWebContents());

  print_view_manager->WaitOnPreview();
  EXPECT_THAT(print_view_manager->preview_allowed(), testing::Optional(false));
  EXPECT_EQ(scanning_responses_count(), 0);

  // This is always 0 because printing is always blocked by the DLP policy.
  ASSERT_EQ(new_document_called_count(), 0);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(
    All,
    ContentAnalysisBeforePrintPreviewBrowserTest,
    testing::Combine(
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
        /*policy_value=*/testing::Values(kCloudAnalysisBlockingPolicy,
                                         kCloudAnalysisNonBlockingPolicy,
                                         kLocalAnalysisPolicy),
#else
        /*policy_value=*/testing::Values(kCloudAnalysisBlockingPolicy,
                                         kCloudAnalysisNonBlockingPolicy),
#endif
        /*content_analysis_allows_print=*/testing::Bool(),
        /*oop_enabled=*/testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    All,
    ContentAnalysisAfterPrintPreviewBrowserTest,
    testing::Combine(
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
        /*policy_value=*/testing::Values(kCloudAnalysisBlockingPolicy,
                                         kCloudAnalysisNonBlockingPolicy,
                                         kLocalAnalysisPolicy),
#else
        /*policy_value=*/testing::Values(kCloudAnalysisBlockingPolicy,
                                         kCloudAnalysisNonBlockingPolicy),
#endif
        /*content_analysis_allows_print=*/testing::Bool(),
        /*oop_enabled=*/testing::Bool()));

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
INSTANTIATE_TEST_SUITE_P(
    All,
    ContentAnalysisScriptedPreviewlessPrintBeforeDialogBrowserTest,
    testing::Combine(
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
        /*policy_value=*/testing::Values(kCloudAnalysisBlockingPolicy,
                                         kCloudAnalysisNonBlockingPolicy,
                                         kLocalAnalysisPolicy),
#else
        /*policy_value=*/testing::Values(kCloudAnalysisBlockingPolicy,
                                         kCloudAnalysisNonBlockingPolicy),
#endif
        /*content_analysis_allows_print=*/testing::Bool(),
        /*oop_enabled=*/testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    All,
    ContentAnalysisScriptedPreviewlessPrintAfterDialogBrowserTest,
    testing::Combine(
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
        /*policy_value=*/testing::Values(kCloudAnalysisBlockingPolicy,
                                         kCloudAnalysisNonBlockingPolicy,
                                         kLocalAnalysisPolicy),
#else
        /*policy_value=*/testing::Values(kCloudAnalysisBlockingPolicy,
                                         kCloudAnalysisNonBlockingPolicy),
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
        /*content_analysis_allows_print=*/testing::Bool(),
        /*oop_enabled=*/testing::Bool()));

#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

}  // namespace printing
