// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"
#include "printing/printing_features.h"
#include "printing/printing_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/browser/printing/print_job_worker_oop.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/printer_query_oop.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#endif

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"  // nogncheck
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"  // nogncheck
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40567307)  ChromeOS uses different testing setup that isn't
// hooked up to make use of `TestPrintingContext` yet.
#error "ChromeOS not supported here yet"
#endif

using task_manager::TaskManagerInterface;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchUtility;
using task_manager::browsertest_util::WaitForTaskManagerRows;

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

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
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
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
void CancelPrintPreview(content::WebContents* preview_dialog) {
  // This script locates and clicks the Cancel button for a Print Preview
  // dialog.
  const char kScript[] = R"(
      const button = document.getElementsByTagName('print-preview-app')[0]
                       .$['sidebar']
                       .shadowRoot.querySelector('print-preview-button-strip')
                       .shadowRoot.querySelector('.cancel-button');
      button.click();)";

  // It is possible for sufficient processing for the cancel to complete such
  // that the renderer naturally terminates before ExecJs() returns here.  This
  // causes ExecJs() to return false, with a JavaScript error of
  // "Renderer terminated".  Since the termination can actually be a result of
  // a successful cancel, do not assert on this return result, just ignore the
  // error instead.  Rely upon tests using other methods to catch errors, such
  // as monitoring for the Print Preview to be done if that is needed.
  std::ignore = content::ExecJs(preview_dialog, kScript);
}

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

const char* GetPrintBackendString(PrintBackendFeatureVariation variation) {
  switch (variation) {
    case PrintBackendFeatureVariation::kInBrowserProcess:
      return "InBrowser";
    case PrintBackendFeatureVariation::kOopSandboxedService:
      return "OopSandboxed";
    case PrintBackendFeatureVariation::kOopUnsandboxedService:
      return "OopUnsandboxed";
  }
}

enum class PlatformPrintApiVariation {
#if BUILDFLAG(IS_WIN)
  // Windows print drivers can have a language type which alters how the print
  // data is processed.  While much of the GDI printing pipeline is not
  // concerned with the language type differences, certain portions of the
  // printing pipeline are impacted by it.  Most tests only need test against
  // one GDI language type.
  kGdiEmf,
  kGdiPostScriptLevel2,
  kGdiPostScriptLevel3,
  kGdiTextOnly,
  kXps,
#else
  kCups,
#endif
};

const char* GetPlatformPrintApiString(PlatformPrintApiVariation variation) {
  switch (variation) {
#if BUILDFLAG(IS_WIN)
    case PlatformPrintApiVariation::kGdiEmf:
      return "GdiEmf";
    case PlatformPrintApiVariation::kGdiPostScriptLevel2:
      return "GdiPostScriptLevel2";
    case PlatformPrintApiVariation::kGdiPostScriptLevel3:
      return "GdiPostScriptLevel3";
    case PlatformPrintApiVariation::kGdiTextOnly:
      return "GdiTextOnly";
    case PlatformPrintApiVariation::kXps:
      return "Xps";
#else
    case PlatformPrintApiVariation::kCups:
      return "Cups";
#endif
  }
}

// Caution must be taken with platform API variations, as `kXps` should not
// be generated with `kInBrowserProcess`.  Use of `testing::Combine()` between
// `PrintBackendFeatureVariation` and `PlatformPrintApiVariation` could
// inadvertently cause this illegal combination.  This can be avoided by using
// a local helper method to generate the allowed combinations.
//
// `SystemAccessProcessPrintBrowserTestBase` will check this constraint at
// runtime.
struct PrintBackendAndPlatformPrintApiVariation {
  PrintBackendFeatureVariation print_backend;
  PlatformPrintApiVariation platform_api;
};

// Tests using these variations are concerned with all the different language
// types on Windows.
constexpr PrintBackendAndPlatformPrintApiVariation
    kSandboxedServicePlatformPrintLanguageApiVariations[] = {
#if BUILDFLAG(IS_WIN)
        // TODO(crbug.com/40100562):  Include XPS variation.
        {PrintBackendFeatureVariation::kOopSandboxedService,
         PlatformPrintApiVariation::kGdiEmf},
        {PrintBackendFeatureVariation::kOopSandboxedService,
         PlatformPrintApiVariation::kGdiPostScriptLevel2},
        {PrintBackendFeatureVariation::kOopSandboxedService,
         PlatformPrintApiVariation::kGdiPostScriptLevel3},
        {PrintBackendFeatureVariation::kOopSandboxedService,
         PlatformPrintApiVariation::kGdiTextOnly},
#else
        {PrintBackendFeatureVariation::kOopSandboxedService,
         PlatformPrintApiVariation::kCups},
#endif
};

std::string GetPrintBackendAndPlatformPrintApiString(
    const PrintBackendAndPlatformPrintApiVariation& variation) {
  return base::JoinString({GetPrintBackendString(variation.print_backend),
                           GetPlatformPrintApiString(variation.platform_api)},
                          /*separator=*/"_");
}

std::string GetPrintBackendAndPlatformPrintApiTestSuffix(
    const testing::TestParamInfo<PrintBackendAndPlatformPrintApiVariation>&
        info) {
  return GetPrintBackendAndPlatformPrintApiString(info.param);
}

std::vector<PrintBackendAndPlatformPrintApiVariation>
GeneratePrintBackendAndPlatformPrintApiVariations(
    std::vector<PrintBackendFeatureVariation> print_backend_variations) {
  std::vector<PrintBackendAndPlatformPrintApiVariation> variations;

  for (PrintBackendFeatureVariation print_backend_variation :
       print_backend_variations) {
#if BUILDFLAG(IS_WIN)
    // Only need one GDI variation, not interested in different language types.
    // TODO(crbug.com/40100562):  Include XPS variation, only when the
    // `print_backend_variation` is not `kInBrowserProcess`.
    variations.emplace_back(print_backend_variation,
                            PlatformPrintApiVariation::kGdiEmf);
#else
    variations.emplace_back(print_backend_variation,
                            PlatformPrintApiVariation::kCups);
#endif
  }

  return variations;
}

std::string GetServiceLaunchTimingTestSuffix(
    const testing::TestParamInfo<bool>& info) {
  return info.param ? "EarlyStart" : "OnDemand";
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

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
using OnFinishDocumentDoneCallback = base::RepeatingCallback<void(int job_id)>;
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
    base::RepeatingCallback<void(int job_id, mojom::ResultCode result)>;
using OnDidCancelCallback = base::RepeatingClosure;
using OnDidShowErrorDialog = base::RepeatingClosure;

class TestPrintJobWorker : public PrintJobWorker {
 public:
  // Callbacks to run for overrides.
  struct PrintCallbacks {
    ErrorCheckCallback error_check_callback;
    OnUseDefaultSettingsCallback did_use_default_settings_callback;
    OnGetSettingsWithUICallback did_get_settings_with_ui_callback;
    OnDidUpdatePrintSettingsCallback did_update_print_settings_callback;
    OnFinishDocumentDoneCallback did_finish_document_done_callback;
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
  void FinishDocumentDone(int job_id) override {
    callbacks_->did_finish_document_done_callback.Run(job_id);
    PrintJobWorker::FinishDocumentDone(job_id);
  }
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

  void UpdatePrintSettings(base::Value::Dict new_settings,
                           SettingsCallback callback) override {
    PrinterQuery::UpdatePrintSettings(
        std::move(new_settings),
        base::BindOnce(&TestPrinterQuery::OnDidUpdatePrintSettings,
                       base::Unretained(this), std::move(callback)));
  }

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

  void OnDidUpdatePrintSettings(SettingsCallback callback,
                                std::unique_ptr<PrintSettings> settings,
                                mojom::ResultCode result) {
    DVLOG(1) << "Observed: update print settings";
    std::move(callback).Run(std::move(settings), result);
    callbacks_->did_update_print_settings_callback.Run(result);
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
      std::optional<PrintBackendServiceManager::ClientId> client_id,
      std::optional<PrintBackendServiceManager::ContextId> context_id,
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
  void OnDidStartPrinting(mojom::ResultCode result, int job_id) override {
    DVLOG(1) << "Observed: start printing of document";
    callbacks_->error_check_callback.Run(result);
    PrintJobWorkerOop::OnDidStartPrinting(result, job_id);
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
    callbacks_->did_document_done_callback.Run(job_id, result);
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
  TestPrinterQueryOop(
      content::GlobalRenderFrameHostId rfh_id,
      bool simulate_spooling_memory_errors,
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG) && BUILDFLAG(IS_WIN)
      base::OnceClosure terminate_service_after_update_print_settings_callback,
#endif
      base::OnceClosure terminate_service_after_ask_user_for_settings_callback,
      TestPrintJobWorkerOop::PrintCallbacks* callbacks)
      : PrinterQueryOop(rfh_id),
        simulate_spooling_memory_errors_(simulate_spooling_memory_errors),
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG) && BUILDFLAG(IS_WIN)
        terminate_service_after_update_print_settings_callback_(
            std::move(terminate_service_after_update_print_settings_callback)),
#endif
        terminate_service_after_ask_user_for_settings_callback_(
            std::move(terminate_service_after_ask_user_for_settings_callback)),
        callbacks_(callbacks) {
  }

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
    if (terminate_service_after_ask_user_for_settings_callback_) {
      std::move(terminate_service_after_ask_user_for_settings_callback_).Run();
    }
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
    if (terminate_service_after_ask_user_for_settings_callback_) {
      std::move(terminate_service_after_ask_user_for_settings_callback_).Run();
    }
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
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG) && BUILDFLAG(IS_WIN)
    if (terminate_service_after_update_print_settings_callback_) {
      std::move(terminate_service_after_update_print_settings_callback_).Run();
    }
#endif
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

  const bool simulate_spooling_memory_errors_;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG) && BUILDFLAG(IS_WIN)
  base::OnceClosure terminate_service_after_update_print_settings_callback_;
#endif
  base::OnceClosure terminate_service_after_ask_user_for_settings_callback_;
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

  // Only applicable when `UseService()` returns true.
  virtual bool EarlyStartService() { return false; }

#if BUILDFLAG(IS_WIN)
  // Only applicable when `UseService()` returns true.
  virtual bool UseXps() = 0;
#endif

  void SetUpFeatures() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (UseService()) {
      enabled_features.push_back(
          {features::kEnableOopPrintDrivers,
           {{features::kEnableOopPrintDriversEarlyStart.name,
             EarlyStartService() ? "true" : "false"},
            {features::kEnableOopPrintDriversJobPrint.name, "true"},
            {features::kEnableOopPrintDriversSandbox.name,
             SandboxService() ? "true" : "false"}}});
#if BUILDFLAG(IS_WIN)
      if (UseXps()) {
        enabled_features.push_back({features::kUseXpsForPrinting, {}});
      } else {
        disabled_features.push_back(features::kUseXpsForPrinting);
      }
      // TODO(crbug.com/40111626):  Support `kUseXpsForPrintingFromPdf`.
      disabled_features.push_back(features::kUseXpsForPrintingFromPdf);
      // TODO(crbug.com/40212677):  Support `kReadPrinterCapabilitiesWithXps`.
      disabled_features.push_back(features::kReadPrinterCapabilitiesWithXps);
#endif  // BUILDFLAG(IS_WIN)
    } else {
      disabled_features.push_back(features::kEnableOopPrintDrivers);
#if BUILDFLAG(IS_WIN)
      CHECK(!UseXps());
      disabled_features.push_back(features::kUseXpsForPrinting);
      disabled_features.push_back(features::kUseXpsForPrintingFromPdf);
      disabled_features.push_back(features::kReadPrinterCapabilitiesWithXps);
#endif  // BUILDFLAG(IS_WIN)
    }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
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
      test_print_job_worker_callbacks_
          .did_update_print_settings_callback = base::BindRepeating(
          &SystemAccessProcessPrintBrowserTestBase::OnDidUpdatePrintSettings,
          base::Unretained(this));
      test_print_job_worker_callbacks_.did_finish_document_done_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnFinishDocumentDone,
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
    if (UseService() && !EarlyStartService()) {
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
#endif
    ASSERT_EQ(print_job_construction_count(), print_job_destruction_count());
  }

  void TearDownOnMainThread() override {
    PrintBrowserTest::TearDownOnMainThread();
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    PrintBackendServiceManager::ResetForTesting();
#endif
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
  void OnPrintPreviewDone() override {
    if (check_for_print_preview_done_) {
      CheckForQuit();
    }
  }

  void OnRegisterSystemPrintClient(bool succeeded) override {
    system_print_registration_succeeded_ = succeeded;
  }
#endif

  void OnDidPrintDocument() override {
    ++did_print_document_count_;
    CheckForQuit();
  }

  void OnRenderFrameDeleted() override {
    if (check_for_render_frame_deleted_) {
      CheckForQuit();
    }
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

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
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

  content::WebContents* PrintAfterPreviewIsReadyAndLoaded() {
    return PrintAfterPreviewIsReadyAndLoaded(PrintParams());
  }

  content::WebContents* PrintAfterPreviewIsReadyAndLoaded(
      const PrintParams& params) {
    return PrintAfterPreviewIsReadyAndMaybeLoaded(params,
                                                  /*wait_for_loaded=*/true);
  }

  content::WebContents* PrintAfterPreviewIsReadyAndMaybeLoaded(
      const PrintParams& params,
      bool wait_for_loaded) {
    // First invoke the Print Preview dialog with requested method.
    content::WebContents* preview_dialog =
        wait_for_loaded ? PrintAndWaitUntilPreviewIsReadyAndLoaded(params)
                        : PrintAndWaitUntilPreviewIsReady(params);
    if (!preview_dialog) {
      ADD_FAILURE() << "Unable to get Print Preview dialog";
      return nullptr;
    }

    // Print Preview is completely ready, can now initiate printing.
    // This script locates and clicks the Print button.
    const char kScript[] = R"(
      const button = document.getElementsByTagName('print-preview-app')[0]
                       .$['sidebar']
                       .shadowRoot.querySelector('print-preview-button-strip')
                       .shadowRoot.querySelector('.action-button');
      button.click();)";
    auto result = content::ExecJs(preview_dialog, kScript);
    // TODO(crbug.com/40926610):  Update once it is known if the assertion
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
      ADD_FAILURE() << "ExecJs() failed; if reason is because the renderer "
                       "terminated, it is possibly okay? "
                    << result.message();
    }
    WaitUntilCallbackReceived();
    return preview_dialog;
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

    // Queuing converted pages to be spooled occurs on the UI thread, while the
    // actual spooling occurs on the worker thread.  The worker thread polls for
    // pages, making it difficult to know if earlier pages that are successfully
    // converted will get spooled before some other error causes the job to be
    // canceled.  Do not use rendered page counts as part of any test
    // expectations in this case.  Other events should be used to know when it
    // is safe to terminate the test.
    DisableCheckForOnRenderedPrintedPage();
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

  void PrimeForFailInAskUserForSettings() {
    test_printing_context_factory()->SetFailErrorOnAskUserForSettings();
  }

  void PrimeForServiceTerminatesAfterGetUserSettings() {
    terminate_service_after_ask_user_for_settings_ = true;
  }
#endif

  void PrimeForFailInUpdatePrinterSettings() {
    test_printing_context_factory()->SetFailedErrorOnUpdatePrinterSettings();
  }

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
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void PrimeForServiceTerminatesAfterUpdatePrintSettings() {
    terminate_service_after_update_print_settings_ = true;
  }
#endif

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
#endif  // BUILDFLAG(IS_WIN)

  void PrimeForAccessDeniedErrorsInRenderPrintedDocument() {
    test_printing_context_factory()->SetAccessDeniedErrorOnRenderDocument(
        /*cause_errors=*/true);
  }

  void PrimeForAccessDeniedErrorsInDocumentDone() {
    test_printing_context_factory()->SetAccessDeniedErrorOnDocumentDone(
        /*cause_errors=*/true);
  }

#if BUILDFLAG(IS_WIN)
  void DisableCheckForOnRenderedPrintedPage() {
    check_for_rendered_printed_page_ = false;
  }
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  void SetCheckForPrintPreviewDone(bool check) {
    check_for_print_preview_done_ = check;
  }
#endif

  void SetCheckForRenderFrameDeleted(bool check) {
    check_for_render_frame_deleted_ = check;
  }

  const std::optional<bool> system_print_registration_succeeded() const {
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

  std::optional<int> document_done_job_id() const {
    return document_done_job_id_;
  }

  int cancel_count() const { return cancel_count_; }
  std::optional<mojom::ResultCode> in_process_last_error_result_code() const {
    return in_process_last_error_result_code_;
  }

  int print_job_construction_count() const {
    return print_job_construction_count_;
  }
  int print_job_destruction_count() const {
    return print_job_destruction_count_;
  }
  int did_print_document_count() const { return did_print_document_count_; }
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
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
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG) && BUILDFLAG(IS_WIN)
          base::BindLambdaForTesting([&]() {
            if (terminate_service_after_update_print_settings_) {
              ResetService();
            }
          }),
#endif
          base::BindLambdaForTesting([&]() {
            if (terminate_service_after_ask_user_for_settings_) {
              ResetService();
            }
          }),
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
    if (check_for_rendered_printed_page_) {
      CheckForQuit();
    }
  }
#endif

  void OnDidRenderPrintedDocument(mojom::ResultCode result) {
    render_printed_document_result_ = result;
    CheckForQuit();
  }

  void OnDidDocumentDone(int job_id, mojom::ResultCode result) {
    document_done_job_id_ = job_id;
    document_done_result_ = result;
    CheckForQuit();
  }

  void OnFinishDocumentDone(int job_id) { document_done_job_id_ = job_id; }

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

  void ResetService() {
    print_backend_service_.reset();
    test_remote_.reset();
  }

  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(IS_WIN)
  bool check_for_rendered_printed_page_ = true;
#endif
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  bool check_for_print_preview_done_ = false;
  bool check_for_render_frame_deleted_ = false;
  TestPrintJobWorker::PrintCallbacks test_print_job_worker_callbacks_;
  TestPrintJobWorkerOop::PrintCallbacks test_print_job_worker_oop_callbacks_;
  CreatePrinterQueryCallback test_create_printer_query_callback_;
  std::optional<bool> system_print_registration_succeeded_;
  bool did_use_default_settings_ = false;
  bool did_get_settings_with_ui_ = false;
  bool print_backend_service_use_detected_ = false;
  bool simulate_spooling_memory_errors_ = false;
#if BUILDFLAG(IS_WIN)
  std::optional<uint32_t> simulate_pdf_conversion_error_on_page_index_;
#endif  // BUILDFLAG(IS_WIN)
  mojo::Remote<mojom::PrintBackendService> test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
  std::optional<mojom::ResultCode> in_process_last_error_result_code_;
  bool reset_errors_after_check_ = true;
  int did_print_document_count_ = 0;
  mojom::ResultCode use_default_settings_result_ = mojom::ResultCode::kFailed;
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  mojom::ResultCode ask_user_for_settings_result_ = mojom::ResultCode::kFailed;
  bool terminate_service_after_ask_user_for_settings_ = false;
#endif
  mojom::ResultCode update_print_settings_result_ = mojom::ResultCode::kFailed;
  mojom::ResultCode start_printing_result_ = mojom::ResultCode::kFailed;
#if BUILDFLAG(IS_WIN)
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  bool terminate_service_after_update_print_settings_ = false;
#endif
  mojom::ResultCode render_printed_page_result_ = mojom::ResultCode::kFailed;
  int render_printed_pages_count_ = 0;
#endif  // BUILDFLAG(IS_WIN)
  mojom::ResultCode render_printed_document_result_ =
      mojom::ResultCode::kFailed;
  mojom::ResultCode document_done_result_ = mojom::ResultCode::kFailed;
  std::optional<int> document_done_job_id_;
  int cancel_count_ = 0;
  int print_job_construction_count_ = 0;
  int print_job_destruction_count_ = 0;
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  int composited_for_content_analysis_count_ = 0;
#endif
};

#if BUILDFLAG(ENABLE_OOP_PRINTING)

class SystemAccessProcessUnsandboxedEarlyStartServicePrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SystemAccessProcessUnsandboxedEarlyStartServicePrintBrowserTest() = default;
  ~SystemAccessProcessUnsandboxedEarlyStartServicePrintBrowserTest() override =
      default;

  bool UseService() override { return true; }
  bool SandboxService() override { return false; }
  bool EarlyStartService() override { return GetParam(); }
#if BUILDFLAG(IS_WIN)
  bool UseXps() override { return false; }
#endif

  bool DoesPrintBackendServiceTaskExist() {
    TaskManagerInterface* task_mgr = TaskManagerInterface::GetTaskManager();
    std::u16string title = MatchUtility(u"Print Backend Service");
    for (auto task_id : task_mgr->GetTaskIdsList()) {
      if (title == task_mgr->GetTitle(task_id)) {
        return true;
      }
    }
    return false;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemAccessProcessUnsandboxedEarlyStartServicePrintBrowserTest,
    /*EarlyStartService=*/testing::Bool(),
    GetServiceLaunchTimingTestSuffix);

class SystemAccessProcessPrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase,
      public testing::WithParamInterface<
          PrintBackendAndPlatformPrintApiVariation> {
 public:
  SystemAccessProcessPrintBrowserTest() = default;
  ~SystemAccessProcessPrintBrowserTest() override = default;

  bool UseService() override {
    return GetParam().print_backend !=
           PrintBackendFeatureVariation::kInBrowserProcess;
  }
  bool SandboxService() override {
    return GetParam().print_backend ==
           PrintBackendFeatureVariation::kOopSandboxedService;
  }
#if BUILDFLAG(IS_WIN)
  bool UseXps() override {
    return GetParam().platform_api == PlatformPrintApiVariation::kXps;
  }
#endif

#if BUILDFLAG(IS_WIN)
  mojom::PrinterLanguageType UseLanguageType() {
    switch (GetParam().platform_api) {
      case PlatformPrintApiVariation::kGdiEmf:
        return mojom::PrinterLanguageType::kNone;
      case PlatformPrintApiVariation::kGdiPostScriptLevel2:
        return mojom::PrinterLanguageType::kPostscriptLevel2;
      case PlatformPrintApiVariation::kGdiPostScriptLevel3:
        return mojom::PrinterLanguageType::kPostscriptLevel3;
      case PlatformPrintApiVariation::kGdiTextOnly:
        return mojom::PrinterLanguageType::kTextOnly;
      case PlatformPrintApiVariation::kXps:
        return mojom::PrinterLanguageType::kXps;
    }
  }
#endif
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemAccessProcessPrintBrowserTest,
    testing::ValuesIn(GeneratePrintBackendAndPlatformPrintApiVariations(
        {PrintBackendFeatureVariation::kInBrowserProcess,
         PrintBackendFeatureVariation::kOopSandboxedService,
         PrintBackendFeatureVariation::kOopUnsandboxedService})),
    GetPrintBackendAndPlatformPrintApiTestSuffix);

using SystemAccessProcessServicePrintBrowserTest =
    SystemAccessProcessPrintBrowserTest;

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemAccessProcessServicePrintBrowserTest,
    testing::ValuesIn(GeneratePrintBackendAndPlatformPrintApiVariations(
        {PrintBackendFeatureVariation::kOopSandboxedService,
         PrintBackendFeatureVariation::kOopUnsandboxedService})),
    GetPrintBackendAndPlatformPrintApiTestSuffix);

using SystemAccessProcessSandboxedServicePrintBrowserTest =
    SystemAccessProcessServicePrintBrowserTest;

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemAccessProcessSandboxedServicePrintBrowserTest,
    testing::ValuesIn(GeneratePrintBackendAndPlatformPrintApiVariations(
        {PrintBackendFeatureVariation::kOopSandboxedService})),
    GetPrintBackendAndPlatformPrintApiTestSuffix);

using SystemAccessProcessSandboxedServiceLanguagePrintBrowserTest =
    SystemAccessProcessServicePrintBrowserTest;

INSTANTIATE_TEST_SUITE_P(
    All,
    SystemAccessProcessSandboxedServiceLanguagePrintBrowserTest,
    testing::ValuesIn(kSandboxedServicePlatformPrintLanguageApiVariations),
    GetPrintBackendAndPlatformPrintApiTestSuffix);

IN_PROC_BROWSER_TEST_P(
    SystemAccessProcessUnsandboxedEarlyStartServicePrintBrowserTest,
    ServiceLaunched) {
  chrome::ShowTaskManager(browser());

  // Wait for browser to open with a tab.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Now that startup is complete, look through the list of processes in the
  // Task Manager to see if a Print Backend service has been started (even
  // though there has not been any request for printing).
  CHECK_EQ(DoesPrintBackendServiceTaskExist(), EarlyStartService());
}

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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       UpdatePrintSettingsFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForFailInUpdatePrinterSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Update print settings, which fails.  No print job is created.
  // 2.  An error dialog is displayed.
  SetNumExpectedMessages(/*num=*/2);

  // Since the preview is loaded before initiating the Print, the error
  // is displayed in the preview and the dialog remains open.
  content::WebContents* preview_dialog = PrintAfterPreviewIsReadyAndLoaded();
  ASSERT_TRUE(preview_dialog);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kFailed);
  EXPECT_EQ(error_dialog_shown_count(), 1u);

  // Need to close the dialog to ensure proper cleanup is done before
  // sanity checks at test termination.  This posts to UI thread to close,
  // so need to ensure that has a chance to run before terminating.
  CancelPrintPreview(preview_dialog);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       PrintWithPreviewBeforeLoadedUpdatePrintSettingsFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForFailInUpdatePrinterSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Update print settings, which fails.  No print job is created.
  // 2.  An error dialog is displayed.
  SetNumExpectedMessages(/*num=*/2);
  PrintAfterPreviewIsReadyAndMaybeLoaded(PrintParams(),
                                         /*wait_for_loaded=*/false);

  // Print Preview dialog will have been closed, ensure that the UI thread
  // runs all posted tasks to reflect that before checking it is gone.
  base::RunLoop().RunUntilIdle();
  content::WebContents* preview_dialog =
      PrintPreviewDialogController::GetInstance()->GetPrintPreviewForContents(
          web_contents);
  ASSERT_FALSE(preview_dialog);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kFailed);

  // Initiating printing before the document is ready hides the Print Preview
  // dialog.  An error dialog should get shown to notify the user.
  EXPECT_EQ(error_dialog_shown_count(), 1u);
}

IN_PROC_BROWSER_TEST_P(
    SystemAccessProcessSandboxedServiceLanguagePrintBrowserTest,
    StartPrinting) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
#if BUILDFLAG(IS_WIN)
  SetPrinterLanguageTypeForSubsequentContexts(UseLanguageType());
#endif
  constexpr int kJobId = 1;
  SetNewDocumentJobId(kJobId);

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
  // TODO(crbug.com/40100562)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 1);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_THAT(document_done_job_id(), testing::Optional(kJobId));
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
  std::optional<PrintSettings> settings = document_print_settings();
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingMultipage) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  constexpr int kJobId = 1;
  SetNewDocumentJobId(kJobId);

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
  // TODO(crbug.com/40100562)  Include Windows coverage of
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
  // TODO(crbug.com/40100562)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 3);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_THAT(document_done_job_id(), testing::Optional(kJobId));
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
                       StartPrintingPdfConversionFails) {
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
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  Print job is started, but is canceled and destroyed due to failure
    //     during PDF conversion failure.
    // No error dialog is shown.
    SetNumExpectedMessages(/*num=*/2);
  }
  PrintAfterPreviewIsReadyAndLoaded();

  // No tracking of start printing or cancel callbacks for in-browser tests,
  // only for OOP.
  if (UseService()) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
  } else {
    EXPECT_THAT(in_process_last_error_result_code(),
                testing::Optional(mojom::ResultCode::kCanceled));
  }
  // TODO(crbug.com/40288222):  Update expectation once an error is shown for
  // this failure.
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
    // The print job is started, but that fails and gets canceled.
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  An error dialog is shown.
    // 3.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/3);
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
    // 1.  Update print settings.
    // 2.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/2);
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartPrintingAccessDenied) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  constexpr int kJobId = 1;
  SetNewDocumentJobId(kJobId);
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
  // TODO(crbug.com/40100562)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 1);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_THAT(document_done_job_id(), testing::Optional(kJobId));
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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
IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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

// TODO(crbug.com/40100562)  Include Windows once XPS print pipeline is added.
#if !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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
  // TODO(crbug.com/40100562)  Include Windows coverage of
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
  constexpr int kJobId = 1;
  SetNewDocumentJobId(kJobId);

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
    // Once the transition to system print is initiated, the expected events
    // are:
    // 1.  Update print settings.
    // 2.  There are no other callbacks that trigger for print stages with
    //     in-browser printing for the Windows case.  The only other expected
    //     event for this is to wait for the one print job to be destroyed, to
    //     ensure printing finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/2);
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
    // TODO(crbug.com/40100562)  Include Windows coverage of
    // RenderPrintedDocument() once XPS print pipeline is added.
    EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(render_printed_page_count(), 1);
#else
    EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
    EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1",
                                                   /*page_ranges=*/nullptr),
              *document_print_settings());
  } else {
#if !BUILDFLAG(IS_WIN)
    EXPECT_TRUE(did_get_settings_with_ui());
    EXPECT_EQ(did_print_document_count(), 1);
#endif
    EXPECT_TRUE(!in_process_last_error_result_code().has_value());
    EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1",
                                                   /*page_ranges=*/nullptr),
              *document_print_settings());
  }
  EXPECT_THAT(document_done_job_id(), testing::Optional(kJobId));
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

#if BUILDFLAG(IS_WIN)
// This test is Windows-only because of Print Preview behavior in
// `onPrintWithSystemDialog_()`.  For Windows this call ends up going through
// `PrintViewManagerBase::PrintForPrintPreview()`, and thus invokes
// `UpdatePrintSettings()` before displaying the system dialog.  Other
// platforms end up going through `PrintViewManager::PrintForSystemDialogNow()`
// and thus do not update print settings before the system dialog is displayed.
IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       SystemPrintFromPrintPreviewUpdatePrintSettingsFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForFailInUpdatePrinterSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // Once the transition to system print is initiated, the expected events
  // are:
  // 1.  Update the print settings, which fails.  No further printing calls
  //     are made.  No print job is created because of such an early failure.
  // 2.  An error dialog is displayed.
  SetNumExpectedMessages(/*num=*/2);

  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kFailed);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
}

// This test is Windows-only because of Print Preview behavior in
// `onPrintWithSystemDialog_()`.  For Windows this call ends up going through
// `PrintViewManagerBase::PrintForPrintPreview()`, and thus invokes
// `UpdatePrintSettings()` before displaying the system dialog.  Other
// platforms end up going through `PrintViewManager::PrintForSystemDialogNow()`
// and thus do not update print settings before the system dialog is displayed.
IN_PROC_BROWSER_TEST_P(
    SystemAccessProcessSandboxedServicePrintBrowserTest,
    PrintPreviewAfterSystemPrintFromPrintPreviewUpdatePrintSettingsFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForFailInUpdatePrinterSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // First invoke system print from Print Preview.  Must wait until the
  // PrintPreviewUI is completely done before proceeding to the second part
  // of this test to ensure that the client is unregistered from the
  // `PrintBackendServiceManager`.
  SetCheckForPrintPreviewDone(/*check=*/true);

  // Once the transition to system print is initiated, the expected events
  // are:
  // 1.  Update the print settings, which fails.  No further printing calls
  //     are made.  No print job is created because of such an early failure.
  // 2.  An error dialog is displayed.
  // 3.  Print Preview UI is done.
  SetNumExpectedMessages(/*num=*/3);
  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kFailed);
  EXPECT_EQ(error_dialog_shown_count(), 1u);

  // Reset before initiating another Print Preview.
  PrepareRunloop();
  ResetNumReceivedMessages();

  // No longer expect the `PrintPreviewUI` to issue a done callback as part of
  // the test expectations, since the Print Preview will stay open displaying
  // an error message.  There will still be a preview done callback during
  // test shutdown though, so disable doing an expectation check for that.
  SetCheckForPrintPreviewDone(/*check=*/false);

  // The expected events for this are:
  // 1.  Update the print settings, which fails.  No further printing calls
  //     are made.  No print job is created because of such an early failure.
  // 2.  An error dialog is displayed.
  SetNumExpectedMessages(/*num=*/2);
  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kFailed);
  EXPECT_EQ(error_dialog_shown_count(), 2u);
}

// This test is Windows-only, since it is the only platform which can invoke
// the system print dialog from within `PrintingContext::UpdatePrintSettings()`.
// From that system dialog we can cause a cancel to occur.
// TODO(crbug.com/40561724):  Expand this to also cover in-browser, once an
// appropriate signal is available to use for tracking expected events.
IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       SystemPrintFromPrintPreviewCancelRetry) {
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

  // First invoke system print from Print Preview.  Must wait until the
  // PrintPreviewUI is completely done before proceeding to the second part
  // of this test to ensure that the client is unregistered from the
  // `PrintBackendServiceManager`.
  SetCheckForPrintPreviewDone(/*check=*/true);

  // The expected events for this are:
  // 1.  Update the print settings, which indicates to cancel the print
  //     request.  No further printing calls are made.  No print job is
  //     created because of such an early cancel.
  // 2.  Print Preview UI is done.
  SetNumExpectedMessages(/*num=*/2);

  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kCanceled);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 0);

  // Now try to initiate the system print from a Print Preview again.
  // Same number of expected events.
  PrepareRunloop();
  ResetNumReceivedMessages();

  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kCanceled);
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 0);
}

// TODO(crbug.com/40942272):  Enable test for Linux and macOS once renderer
// RunLoop behavior can be made to work with test expectations.
IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest,
                       SystemPrintAfterSystemPrintFromPrintPreview) {
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

  // First invoke system print from Print Preview.  Wait until the
  // PrintPreviewUI is done before proceeding to the second part of the
  // test.
  SetCheckForPrintPreviewDone(/*check=*/true);

  if (UseService()) {
    // Once the transition to system print is initiated, the expected events
    // are:
    // 1.  Update the print settings.  This internally invokes the system
    //     print dialog which cancels.
    // 2.  Print Preview is done.
    // No print job is created because of such an early cancel.
    SetNumExpectedMessages(/*num=*/2);
  } else {
    // Once the transition to system print is initiated, the expected events
    // are:
    // 1.  Update the print settings.
    // 2.  Print Preview is done.
    // No print job is created because of such an early cancel.
    SetNumExpectedMessages(/*num=*/2);
  }
  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  if (UseService()) {
    // Windows invokes system print dialog from UpdatePrintSettings().
    EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kCanceled);
  } else {
    // User settings are invoked from within UpdatePrintSettings().
    EXPECT_FALSE(did_use_default_settings());
    EXPECT_FALSE(did_get_settings_with_ui());

    // `PrintBackendService` should never be used when printing in-browser.
    EXPECT_FALSE(print_backend_service_use_detected());
  }

  // Reset before initiating system print.
  PrepareRunloop();
  ResetNumReceivedMessages();

  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings, which cancels out.  No further printing
  // calls are made.
  SetNumExpectedMessages(/*num=*/2);

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  if (UseService()) {
    EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(ask_user_for_settings_result(), mojom::ResultCode::kCanceled);
  } else {
    EXPECT_TRUE(did_use_default_settings());
    EXPECT_TRUE(did_get_settings_with_ui());
  }
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 0);
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       PrintPreviewPrintAfterSystemPrintRendererCrash) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  content::RenderProcessHost* frame_rph = frame->GetProcess();

  KillPrintRenderFrame frame_content(frame_rph,
                                     GetPrintRenderFrame(frame).get());
  frame_content.OverrideBinderForTesting(frame);

  // With the renderer being prepared to fake a crash, the test needs to watch
  // for it being deleted.
  SetCheckForRenderFrameDeleted(/*check=*/true);
  content::ScopedAllowRendererCrashes allow_renderer_crash;

  // First invoke system print directly.

  // The expected events for this are:
  // 1.  Printing is attempted, but quickly get notified that the render frame
  //     has been deleted because the renderer "crashed".
  SetNumExpectedMessages(/*num=*/1);

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  // After renderer crash, reload the page again in the same tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Now try to initiate print from a Print Preview.
  PrepareRunloop();
  ResetNumReceivedMessages();

  // No longer interested in when the renderer is deleted.
  SetCheckForRenderFrameDeleted(/*check=*/false);

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
  // TODO(crbug.com/40100562)  Include Windows coverage of
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartBasicPrint) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  constexpr int kJobId = 1;
  SetNewDocumentJobId(kJobId);

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
  EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1",
                                                 /*page_ranges=*/nullptr),
            *document_print_settings());
  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40100562)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 1);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_THAT(document_done_job_id(), testing::Optional(kJobId));
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(did_print_document_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
                       StartBasicPrintPageRanges) {
  const PageRanges kPageRanges{{/*from=*/2, /*to=*/3}};
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  SetUserSettingsPageRangesForSubsequentContext(kPageRanges);
  constexpr int kJobId = 1;
  SetNewDocumentJobId(kJobId);

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/7_pages.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

#if BUILDFLAG(IS_WIN)
  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings.
  // 3.  A print job is started.
  // 4.  The print compositor will complete generating the document.
  // 5.  Page 2 of the document is rendered.
  // 6.  Page 3 of the document is rendered.
  // 7.  Receive document done notification.
  // 8.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  // TODO(crbug.com/40100562)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  SetNumExpectedMessages(/*num=*/8);
#else
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
#endif

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
  EXPECT_EQ(*test::MakeUserModifiedPrintSettings("printer1", &kPageRanges),
            *document_print_settings());
  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40100562)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  EXPECT_EQ(render_printed_page_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(render_printed_page_count(), 2);
#else
  EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
#endif
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  EXPECT_THAT(document_done_job_id(), testing::Optional(kJobId));
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
                       StartBasicPrintAskUserForSettingsFails) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForFailInAskUserForSettings();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings, which indicates a failure from system print
  //     dialog.  No further printing calls are made.
  // 3.  An error dialog is displayed.
  // No print job is created because of such an early failure.
  SetNumExpectedMessages(/*num=*/3);

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  if (UseService()) {
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(ask_user_for_settings_result(), mojom::ResultCode::kFailed);
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
  EXPECT_EQ(error_dialog_shown_count(), 1u);
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
  } else {
    EXPECT_THAT(in_process_last_error_result_code(),
                testing::Optional(mojom::ResultCode::kCanceled));
  }
  // TODO(crbug.com/40288222):  Update expectation once an error is shown for
  // this failure.
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_CONCURRENT_BASIC_PRINT_DIALOGS)

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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
  std::optional<PrintBackendServiceManager::ClientId> client_id =
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
IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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
  // TODO(crbug.com/40561724)  Improve on this test by using a persistent fake
  // system print dialog.
  std::optional<PrintBackendServiceManager::ClientId> client_id =
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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
  std::optional<PrintBackendServiceManager::ClientId> client_id =
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
IN_PROC_BROWSER_TEST_P(SystemAccessProcessSandboxedServicePrintBrowserTest,
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
  // TODO(crbug.com/40561724)  Improve on this test by using a persistent fake
  // system print dialog.
  std::optional<PrintBackendServiceManager::ClientId> client_id =
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessServicePrintBrowserTest,
                       StartBasicPrintServiceDisappearsAfterGetSettings) {
  PrimeForServiceTerminatesAfterGetUserSettings();

  // Pretending the service terminated will result in a stranded context left
  // in the test Print Backend service which actually does still exist.
  SkipPersistentContextsCheckOnShutdown();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings.  This succeeds; however, because the
  //     service is detected to have terminated, the print request is aborted.
  // 3.  An error dialog is shown.
  // No print job is created from such an early failure.
  SetNumExpectedMessages(/*num=*/3);

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(ask_user_for_settings_result(), mojom::ResultCode::kSuccess);
#else
  EXPECT_TRUE(did_use_default_settings());
  EXPECT_TRUE(did_get_settings_with_ui());
#endif
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(did_print_document_count(), 0);
  EXPECT_EQ(print_job_construction_count(), 0);
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG) && BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(
    SystemAccessProcessServicePrintBrowserTest,
    SystemPrintFromPrintPreviewUpdatePrintSettingsServiceDisappearsAfterGetSettings) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  PrimeForServiceTerminatesAfterUpdatePrintSettings();

  // Pretending the service terminated will result in a stranded context left
  // in the test Print Backend service which actually does still exist.
  SkipPersistentContextsCheckOnShutdown();

  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SetUpPrintViewManager(web_contents);

  // Once the transition to system print is initiated, the expected events
  // are:
  // 1.  Ask the user for settings.  This succeeds; however, because the
  //     service is detected to have terminated, the print request is aborted.
  // 2.  An error dialog is shown.
  // No print job is created because of such an early failure.
  SetNumExpectedMessages(/*num=*/2);

  SystemPrintFromPreviewOnceReadyAndLoaded(/*wait_for_callback=*/true);

  EXPECT_EQ(update_print_settings_result(), mojom::ResultCode::kSuccess);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG) && BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SystemAccessProcessPrintBrowserTest, OpenPdfInPreview) {
  AddPrinter("printer1");
  SetPrinterNameForSubsequentContexts("printer1");
  constexpr int kJobId = 1;
  SetNewDocumentJobId(kJobId);

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
    // 1.  Update printer settings.
    // 2.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/2);
  }
  OpenPdfInPreviewOnceReadyAndLoaded();

  if (UseService()) {
    EXPECT_EQ(start_printing_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(render_printed_document_result(), mojom::ResultCode::kSuccess);
    EXPECT_EQ(document_done_result(), mojom::ResultCode::kSuccess);
  } else {
    EXPECT_FALSE(in_process_last_error_result_code().has_value());
  }
  EXPECT_THAT(document_done_job_id(), testing::Optional(kJobId));
  EXPECT_TRUE(destination_is_preview());
  EXPECT_EQ(error_dialog_shown_count(), 0u);
  EXPECT_EQ(print_job_destruction_count(), 1);
}
#endif  // BUILDFLAG(IS_MAC)

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
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
      enterprise_connectors::ContentAnalysisRequest::Reason expected_reason,
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

  const std::optional<bool>& preview_allowed() const {
    return preview_allowed_;
  }

#if BUILDFLAG(IS_CHROMEOS)
  void set_allowed_by_dlp(bool allowed) { allowed_by_dlp_ = allowed; }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void set_on_print_preview_done_closure(base::OnceClosure closure) {
    observer_.set_on_print_preview_done_closure(std::move(closure));
  }

 protected:
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
    EXPECT_EQ(scanning_data.reason, expected_reason_);

    // The data of the document should be a valid PDF as this code should be
    // called as the print job is about to start printing.
    EXPECT_TRUE(LooksLikePdf(*print_data));

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
  // analysis and DLP (if on CrOS). This is `std::nullopt` until then.
  std::optional<bool> preview_allowed_;

  // Used to validate the corresponding `ContentAnalysisDelegate::Data` passed
  // in various content analysis-related functions.
  enterprise_connectors::ContentAnalysisRequest::Reason expected_reason_;

  // Used to validate the corresponding `ContentAnalysisDelegate::Data` passed
  // in various content analysis-related functions. Corresponds to the value
  // return by `PolicyValue()` for the current test.
  const char* policy_value_ = nullptr;

  base::RunLoop preview_run_loop_;
  OnDidCompositeForContentAnalysis did_composite_for_content_analysis_callback_;
  Observer observer_;
};

using ContentAnalysisConfigurationVariation = testing::tuple<
    const char* /*policy_value*/,
    bool /*content_analysis_allows_print*/,
    PrintBackendAndPlatformPrintApiVariation /*backend_and_print_api*/>;

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
      enterprise_connectors::ContentAnalysisRequest::Reason expected_reason) {
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

  static std::string GetTestSuffix(
      const testing::TestParamInfo<ContentAnalysisConfigurationVariation>&
          info) {
    return base::JoinString(
        {GetPolicyTypeString(std::get<0>(info.param)),
         GetAllowsPrintString(std::get<1>(info.param)),
         GetPrintBackendAndPlatformPrintApiString(std::get<2>(info.param))},
        /*separator=*/"_");
  }

 protected:
  static const char* GetPolicyTypeString(const char* policy_value) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
    if (policy_value == kLocalAnalysisPolicy) {
      return "LocalPolicy";
    }
#endif
    if (policy_value == kCloudAnalysisBlockingPolicy) {
      return "BlockingCloudPolicy";
    }
    CHECK_EQ(policy_value, kCloudAnalysisNonBlockingPolicy);
    return "NonBlockingCloudPolicy";
  }

  static const char* GetAllowsPrintString(bool allows_print) {
    return allows_print ? "AllowsPrint" : "DisallowsPrint";
  }

 private:
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  enterprise_connectors::FakeContentAnalysisSdkManager sdk_manager_;
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  // Counts the number of times `ScanningResponse` is called, why is equivalent
  // to the number of times a printed page's bytes would reach a scanner.
  int scanning_responses_ = 0;
};

class ContentAnalysisAfterPrintPreviewBrowserTest
    : public ContentAnalysisPrintBrowserTestBase,
      public testing::WithParamInterface<
          ContentAnalysisConfigurationVariation> {
 public:
  const char* PolicyValue() const override { return std::get<0>(GetParam()); }
  bool ContentAnalysisAllowsPrint() const override {
    return std::get<1>(GetParam());
  }
  bool UseService() override {
    return backend_and_print_api().print_backend !=
           PrintBackendFeatureVariation::kInBrowserProcess;
  }
#if BUILDFLAG(IS_WIN)
  bool UseXps() override {
    return backend_and_print_api().platform_api ==
           PlatformPrintApiVariation::kXps;
  }
#endif

  // PrintJob::Observer:
  void OnCanceling() override { CheckForQuit(); }

 private:
  PrintBackendAndPlatformPrintApiVariation backend_and_print_api() const {
    return std::get<2>(GetParam());
  }
};

class ContentAnalysisScriptedPreviewlessPrintBrowserTestBase
    : public ContentAnalysisPrintBrowserTestBase,
      public testing::WithParamInterface<
          ContentAnalysisConfigurationVariation> {
 public:
  const char* PolicyValue() const override { return std::get<0>(GetParam()); }
  bool ContentAnalysisAllowsPrint() const override {
    return std::get<1>(GetParam());
  }
  bool UseService() override {
    return backend_and_print_api().print_backend !=
           PrintBackendFeatureVariation::kInBrowserProcess;
  }
#if BUILDFLAG(IS_WIN)
  bool UseXps() override {
    return backend_and_print_api().platform_api ==
           PlatformPrintApiVariation::kXps;
  }
#endif

  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    cmd_line->AppendSwitch(switches::kDisablePrintPreview);
    ContentAnalysisPrintBrowserTestBase::SetUpCommandLine(cmd_line);
  }

 private:
  PrintBackendAndPlatformPrintApiVariation backend_and_print_api() const {
    return std::get<2>(GetParam());
  }
};

class ContentAnalysisScriptedPreviewlessPrintAfterDialogBrowserTest
    : public ContentAnalysisScriptedPreviewlessPrintBrowserTestBase {
 public:
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

IN_PROC_BROWSER_TEST_P(ContentAnalysisAfterPrintPreviewBrowserTest,
                       PrintWithPreviewBeforeLoaded) {
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
        &ContentAnalysisAfterPrintPreviewBrowserTest::CheckForQuit,
        base::Unretained(this)));
    if (PrintAllowedOrNonBlockingPolicy()) {
      // The expected events for this are:
      // 1.  Update print settings.
      // 2.  Print preview completes.
      // 3.  Wait for the one print job to be destroyed, to ensure printing
      //     finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/3);
    } else {
      // The expected events for this are:
      // 1.  Print preview completes.  No print job is created.
      SetNumExpectedMessages(/*num=*/1);
    }
  }

  PrintAfterPreviewIsReadyAndMaybeLoaded(PrintParams(),
                                         /*wait_for_loaded=*/false);

  EXPECT_THAT(print_view_manager->preview_allowed(), testing::Optional(true));
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
      // 1.  Update print settings.
      // 2.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/2);
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
    // The expected events for this are:
    // 1.  Update print settings.
    // 2.  The print job is cancelled.
    // 3.  The print job is destroyed.
    SetNumExpectedMessages(/*num=*/3);
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
  EXPECT_EQ(print_job_destruction_count(), 1);
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
      // 1.  Update print settings.
      // 2.  Wait for the actual printing job to be destroyed, to ensure
      //     printing finished cleanly before completing the test.
      SetNumExpectedMessages(/*num=*/2);
    }
  } else {
    print_view_manager->set_on_print_preview_done_closure(base::BindOnce(
        &ContentAnalysisAfterPrintPreviewBrowserTest::CheckForQuit,
        base::Unretained(this)));
    // The expected events for this are:
    // 1.  Print Preview is done.
    SetNumExpectedMessages(/*num=*/1);
  }
  OpenPdfInPreviewOnceReadyAndLoaded();

  EXPECT_THAT(print_view_manager->preview_allowed(), testing::Optional(true));

  EXPECT_EQ(print_job_destruction_count(),
            PrintAllowedOrNonBlockingPolicy() ? 1 : 0);
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
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
        /*backend_and_print_api=*/
        testing::ValuesIn(GeneratePrintBackendAndPlatformPrintApiVariations(
            {PrintBackendFeatureVariation::kInBrowserProcess,
             PrintBackendFeatureVariation::kOopSandboxedService}))),
    ContentAnalysisAfterPrintPreviewBrowserTest::GetTestSuffix);

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

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
        /*backend_and_print_api=*/
        testing::ValuesIn(GeneratePrintBackendAndPlatformPrintApiVariations(
            {PrintBackendFeatureVariation::kInBrowserProcess,
             PrintBackendFeatureVariation::kOopSandboxedService}))),
    ContentAnalysisScriptedPreviewlessPrintAfterDialogBrowserTest::
        GetTestSuffix);

#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace printing
