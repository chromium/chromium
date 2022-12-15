// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom-test-utils.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_setup.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"
#include "printing/printing_context_factory_for_test.h"
#include "printing/printing_features.h"
#include "printing/printing_utils.h"
#include "printing/test_printing_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/browser/printing/print_job_worker_oop.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

namespace printing {

using testing::_;

using OnDidCreatePrintJobCallback =
    base::RepeatingCallback<void(PrintJob* print_job)>;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
using OnUseDefaultSettingsCallback = base::RepeatingClosure;
using OnGetSettingsWithUICallback = base::RepeatingClosure;

using ErrorCheckCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
using OnDidUseDefaultSettingsCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
#if BUILDFLAG(IS_WIN)
using OnDidAskUserForSettingsCallback =
    base::RepeatingCallback<void(mojom::ResultCode result)>;
#endif
using OnDidStartPrintingCallback =
    base::RepeatingCallback<void(mojom::ResultCode result,
                                 PrintJob* print_job)>;
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

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

namespace {

constexpr int kTestPrintingDpi = 72;
constexpr int kTestPrinterCapabilitiesMaxCopies = 99;
constexpr gfx::Size kTestPrinterCapabilitiesDpi(kTestPrintingDpi,
                                                kTestPrintingDpi);
constexpr int kTestPrintSettingsCopies = 42;

const std::vector<gfx::Size> kTestPrinterCapabilitiesDefaultDpis{
    kTestPrinterCapabilitiesDpi};
const PrinterBasicInfoOptions kTestDummyPrintInfoOptions{{"opt1", "123"},
                                                         {"opt2", "456"}};

constexpr int kDefaultDocumentCookie = 1234;

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
constexpr char kFakeDmToken[] = "fake-dm-token";
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

std::unique_ptr<TestPrintingContext> MakeDefaultTestPrintingContext(
    PrintingContext::Delegate* delegate,
    bool skip_system_calls,
    const std::string& printer_name) {
  auto context =
      std::make_unique<TestPrintingContext>(delegate, skip_system_calls);

  // Setup a sample page setup, which is needed to pass checks in
  // `PrintRenderFrameHelper` that the print params are valid.
  constexpr gfx::Size kPhysicalSize = gfx::Size(200, 200);
  constexpr gfx::Rect kPrintableArea = gfx::Rect(0, 0, 200, 200);
  const PageMargins kRequestedMargins(0, 0, 5, 5, 5, 5);
  const PageSetup kPageSetup(kPhysicalSize, kPrintableArea, kRequestedMargins,
                             /*forced_margins=*/false,
                             /*text_height=*/0);

  auto settings = std::make_unique<PrintSettings>();
  settings->set_copies(kTestPrintSettingsCopies);
  settings->set_dpi(kTestPrintingDpi);
  settings->set_page_setup_device_units(kPageSetup);
  settings->set_device_name(base::ASCIIToUTF16(printer_name));
  context->SetDeviceSettings(printer_name, std::move(settings));
  return context;
}

void OnDidUpdatePrintSettings(
    std::unique_ptr<PrintSettings>& snooped_settings,
    scoped_refptr<PrintQueriesQueue> queue,
    std::unique_ptr<PrinterQuery> printer_query,
    mojom::PrintManagerHost::UpdatePrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(printer_query);
  auto params = mojom::PrintPagesParams::New();
  params->params = mojom::PrintParams::New();
  if (printer_query->last_status() == mojom::ResultCode::kSuccess) {
    RenderParamsFromPrintSettings(printer_query->settings(),
                                  params->params.get());
    params->params->document_cookie = printer_query->cookie();
    params->pages = printer_query->settings().ranges();
    snooped_settings =
        std::make_unique<PrintSettings>(printer_query->settings());
  }
  bool canceled = printer_query->last_status() == mojom::ResultCode::kCanceled;

  std::move(callback).Run(std::move(params), canceled);

  if (printer_query->cookie() && printer_query->settings().dpi()) {
    queue->QueuePrinterQuery(std::move(printer_query));
  } else {
    printer_query->StopWorker();
  }
}

class BrowserPrintingContextFactoryForTest
    : public PrintingContextFactoryForTest {
 public:
  std::unique_ptr<PrintingContext> CreatePrintingContext(
      PrintingContext::Delegate* delegate,
      bool skip_system_calls) override {
    auto context = MakeDefaultTestPrintingContext(delegate, skip_system_calls,
                                                  printer_name_);

    if (failed_error_for_new_document_)
      context->SetNewDocumentFails();
    if (access_denied_errors_for_new_document_)
      context->SetNewDocumentBlockedByPermissions();
#if BUILDFLAG(IS_WIN)
    if (access_denied_errors_for_render_page_)
      context->SetOnRenderPageBlockedByPermissions();
    if (failed_error_for_render_page_number_) {
      context->SetOnRenderPageFailsForPage(
          failed_error_for_render_page_number_);
    }
#endif
    if (access_denied_errors_for_render_document_)
      context->SetOnRenderDocumentBlockedByPermissions();
    if (access_denied_errors_for_document_done_)
      context->SetDocumentDoneBlockedByPermissions();

    if (fail_on_use_default_settings_)
      context->SetUseDefaultSettingsFails();
#if BUILDFLAG(IS_WIN)
    if (cancel_on_ask_user_for_settings_)
      context->SetAskUserForSettingsCanceled();
#endif

    context->SetNewDocumentCalledClosure(base::BindRepeating(
        &BrowserPrintingContextFactoryForTest::NewDocumentCalled,
        base::Unretained(this)));

    return std::move(context);
  }

  void SetPrinterNameForSubsequentContexts(const std::string& printer_name) {
    printer_name_ = printer_name;
  }

  void SetFailedErrorOnNewDocument(bool cause_errors) {
    failed_error_for_new_document_ = cause_errors;
  }

  void SetAccessDeniedErrorOnNewDocument(bool cause_errors) {
    access_denied_errors_for_new_document_ = cause_errors;
  }

#if BUILDFLAG(IS_WIN)
  void SetAccessDeniedErrorOnRenderPage(bool cause_errors) {
    access_denied_errors_for_render_page_ = cause_errors;
  }

  void SetFailedErrorForRenderPage(uint32_t page_number) {
    failed_error_for_render_page_number_ = page_number;
  }
#endif

  void SetAccessDeniedErrorOnRenderDocument(bool cause_errors) {
    access_denied_errors_for_render_document_ = cause_errors;
  }

  void SetAccessDeniedErrorOnDocumentDone(bool cause_errors) {
    access_denied_errors_for_document_done_ = cause_errors;
  }

  void SetFailErrorOnUseDefaultSettings() {
    fail_on_use_default_settings_ = true;
  }

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  void SetCancelErrorOnAskUserForSettings() {
    cancel_on_ask_user_for_settings_ = true;
  }
#endif

  void NewDocumentCalled() { ++new_document_called_count_; }

  int new_document_called_count() { return new_document_called_count_; }

 private:
  std::string printer_name_;
  bool failed_error_for_new_document_ = false;
  bool access_denied_errors_for_new_document_ = false;
#if BUILDFLAG(IS_WIN)
  bool access_denied_errors_for_render_page_ = false;
  uint32_t failed_error_for_render_page_number_ = 0;
#endif
  bool access_denied_errors_for_render_document_ = false;
  bool access_denied_errors_for_document_done_ = false;
  bool fail_on_use_default_settings_ = false;
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  bool cancel_on_ask_user_for_settings_ = false;
#endif
  int new_document_called_count_ = 0;
};

class PrintPreviewObserver : PrintPreviewUI::TestDelegate {
 public:
  explicit PrintPreviewObserver(bool wait_for_loaded)
      : PrintPreviewObserver(wait_for_loaded, /*pages_per_sheet=*/1) {}

  PrintPreviewObserver(bool wait_for_loaded, int pages_per_sheet)
      : pages_per_sheet_(pages_per_sheet), wait_for_loaded_(wait_for_loaded) {
    PrintPreviewUI::SetDelegateForTesting(this);
  }

  PrintPreviewObserver(const PrintPreviewObserver&) = delete;
  PrintPreviewObserver& operator=(const PrintPreviewObserver&) = delete;

  ~PrintPreviewObserver() override {
    PrintPreviewUI::SetDelegateForTesting(nullptr);
  }

  void WaitUntilPreviewIsReady() {
    if (rendered_page_count_ >= expected_rendered_page_count_)
      return;

    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
    run_loop.Run();

    if (queue_.has_value()) {
      std::string message;
      EXPECT_TRUE(queue_->WaitForMessage(&message));
      EXPECT_EQ("\"success\"", message);
    }
  }

  content::WebContents* GetPrintPreviewDialog() { return preview_dialog_; }

  uint32_t rendered_page_count() const { return rendered_page_count_; }

 private:
  // PrintPreviewUI::TestDelegate:
  void DidGetPreviewPageCount(uint32_t page_count) override {
    // `page_count` is the number of pages to be generated but doesn't take
    // N-up into consideration.  Since `DidRenderPreviewPage()` is called after
    // any N-up processing is performed, determine the number of times that
    // function is expected to be called.
    expected_rendered_page_count_ =
        (page_count + pages_per_sheet_ - 1) / pages_per_sheet_;
  }

  // PrintPreviewUI::TestDelegate:
  void DidRenderPreviewPage(content::WebContents* preview_dialog) override {
    ++rendered_page_count_;
    DVLOG(2) << "Rendered preview page " << rendered_page_count_
             << " of a total expected " << expected_rendered_page_count_;
    CHECK_LE(rendered_page_count_, expected_rendered_page_count_);
    if (rendered_page_count_ == expected_rendered_page_count_ && run_loop_) {
      run_loop_->Quit();
      preview_dialog_ = preview_dialog;

      if (wait_for_loaded_) {
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
    }
  }

  absl::optional<content::DOMMessageQueue> queue_;

  // Rendered pages are provided after N-up processing, which will be different
  // from the count provided to `DidGetPreviewPageCount()` when
  // `pages_per_sheet_` is larger than one.
  const int pages_per_sheet_;
  uint32_t expected_rendered_page_count_ = 1;
  uint32_t rendered_page_count_ = 0;

  const bool wait_for_loaded_;
  raw_ptr<content::WebContents, DanglingUntriaged> preview_dialog_ = nullptr;
  base::RunLoop* run_loop_ = nullptr;
};

class TestPrintRenderFrame
    : public mojom::PrintRenderFrameInterceptorForTesting {
 public:
  TestPrintRenderFrame(content::RenderFrameHost* frame_host,
                       content::WebContents* web_contents,
                       int document_cookie,
                       base::RepeatingClosure msg_callback)
      : frame_host_(frame_host),
        web_contents_(web_contents),
        document_cookie_(document_cookie),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        msg_callback_(msg_callback) {}
  ~TestPrintRenderFrame() override = default;

  void OnDidPrintFrameContent(int document_cookie,
                              mojom::DidPrintContentParamsPtr param,
                              PrintFrameContentCallback callback) const {
    EXPECT_EQ(document_cookie, document_cookie_);
    ASSERT_TRUE(param->metafile_data_region.IsValid());
    EXPECT_GT(param->metafile_data_region.GetSize(), 0U);
    std::move(callback).Run(document_cookie, std::move(param));
    task_runner_->PostTask(FROM_HERE, msg_callback_);
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame>(
        std::move(handle)));
  }

  static mojom::DidPrintContentParamsPtr GetDefaultDidPrintContentParams() {
    auto printed_frame_params = mojom::DidPrintContentParams::New();
    // Creates a small amount of region to avoid passing empty data to mojo.
    constexpr size_t kSize = 10;
    base::MappedReadOnlyRegion region_mapping =
        base::ReadOnlySharedMemoryRegion::Create(kSize);
    printed_frame_params->metafile_data_region =
        std::move(region_mapping.region);
    return printed_frame_params;
  }

  // mojom::PrintRenderFrameInterceptorForTesting
  mojom::PrintRenderFrame* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }
  void PrintFrameContent(mojom::PrintFrameContentParamsPtr params,
                         PrintFrameContentCallback callback) override {
    // Sends the printed result back.
    OnDidPrintFrameContent(params->document_cookie,
                           GetDefaultDidPrintContentParams(),
                           std::move(callback));

    auto* client = PrintCompositeClient::FromWebContents(web_contents_);
    if (!client)
      return;

    // Prints its children.
    content::RenderFrameHost* child = ChildFrameAt(frame_host_.get(), 0);
    for (size_t i = 1; child; i++) {
      if (child->GetSiteInstance() != frame_host_->GetSiteInstance()) {
        client->PrintCrossProcessSubframe(gfx::Rect(), params->document_cookie,
                                          child);
      }
      child = ChildFrameAt(frame_host_.get(), i);
    }
  }

 private:
  raw_ptr<content::RenderFrameHost, DanglingUntriaged> frame_host_;
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  const int document_cookie_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::RepeatingClosure msg_callback_;
  mojo::AssociatedReceiver<mojom::PrintRenderFrame> receiver_{this};
};

class KillPrintRenderFrame
    : public mojom::PrintRenderFrameInterceptorForTesting {
 public:
  explicit KillPrintRenderFrame(content::RenderProcessHost* rph) : rph_(rph) {}
  ~KillPrintRenderFrame() override = default;

  void OverrideBinderForTesting(content::RenderFrameHost* render_frame_host) {
    render_frame_host->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PrintRenderFrame::Name_,
            base::BindRepeating(&KillPrintRenderFrame::Bind,
                                base::Unretained(this)));
  }

  void KillRenderProcess(int document_cookie,
                         mojom::DidPrintContentParamsPtr param,
                         PrintFrameContentCallback callback) const {
    std::move(callback).Run(document_cookie, std::move(param));
    rph_->Shutdown(0);
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame>(
        std::move(handle)));
  }

  // mojom::PrintRenderFrameInterceptorForTesting
  mojom::PrintRenderFrame* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }
  void PrintFrameContent(mojom::PrintFrameContentParamsPtr params,
                         PrintFrameContentCallback callback) override {
    // Sends the printed result back.
    const size_t kSize = 10;
    mojom::DidPrintContentParamsPtr printed_frame_params =
        mojom::DidPrintContentParams::New();
    base::MappedReadOnlyRegion region_mapping =
        base::ReadOnlySharedMemoryRegion::Create(kSize);
    printed_frame_params->metafile_data_region =
        std::move(region_mapping.region);
    KillRenderProcess(params->document_cookie, std::move(printed_frame_params),
                      std::move(callback));
  }

 private:
  const raw_ptr<content::RenderProcessHost> rph_;
  mojo::AssociatedReceiver<mojom::PrintRenderFrame> receiver_{this};
};

}  // namespace

class TestPrintViewManager : public PrintViewManager {
 public:
  explicit TestPrintViewManager(content::WebContents* web_contents)
      : PrintViewManager(web_contents) {}
  TestPrintViewManager(content::WebContents* web_contents,
                       OnDidCreatePrintJobCallback callback)
      : PrintViewManager(web_contents),
        on_did_create_print_job_(std::move(callback)) {}
  TestPrintViewManager(const TestPrintViewManager&) = delete;
  TestPrintViewManager& operator=(const TestPrintViewManager&) = delete;
  ~TestPrintViewManager() override = default;

  bool StartPrinting(content::WebContents* contents) {
    auto* print_view_manager = TestPrintViewManager::FromWebContents(contents);
    if (!print_view_manager)
      return false;

    content::RenderFrameHost* rfh_to_use = GetFrameToPrint(contents);
    if (!rfh_to_use)
      return false;

    return print_view_manager->PrintNow(rfh_to_use);
  }

  void WaitUntilPreviewIsShownOrCancelled() {
    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
    run_loop.Run();
  }

  PrintSettings* snooped_settings() { return snooped_settings_.get(); }

  const absl::optional<bool>& print_now_result() const {
    return print_now_result_;
  }

  static TestPrintViewManager* CreateForWebContents(
      content::WebContents* web_contents) {
    auto manager = std::make_unique<TestPrintViewManager>(web_contents);
    auto* manager_ptr = manager.get();
    web_contents->SetUserData(PrintViewManager::UserDataKey(),
                              std::move(manager));
    return manager_ptr;
  }

  // `PrintViewManagerBase` overrides.
  bool PrintNow(content::RenderFrameHost* rfh) override {
    print_now_result_ = PrintViewManager::PrintNow(rfh);
    return *print_now_result_;
  }
  void ShowInvalidPrinterSettingsError() override { ShowPrintErrorDialog(); }
  bool CreateNewPrintJob(std::unique_ptr<PrinterQuery> query) override {
    if (!PrintViewManager::CreateNewPrintJob(std::move(query)))
      return false;
    if (on_did_create_print_job_)
      on_did_create_print_job_.Run(print_job_.get());
    return true;
  }

 protected:
  base::RunLoop* run_loop_ = nullptr;

 private:
  void PrintPreviewAllowedForTesting() override {
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  // printing::mojom::PrintManagerHost:
  void UpdatePrintSettings(int32_t cookie,
                           base::Value::Dict job_settings,
                           UpdatePrintSettingsCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::unique_ptr<PrinterQuery> printer_query =
        queue_->PopPrinterQuery(cookie);
    if (!printer_query) {
      printer_query =
          queue_->CreatePrinterQuery(content::GlobalRenderFrameHostId());
    }
    auto* printer_query_ptr = printer_query.get();
    printer_query_ptr->SetSettings(
        std::move(job_settings),
        base::BindOnce(&OnDidUpdatePrintSettings, std::ref(snooped_settings_),
                       queue_, std::move(printer_query), std::move(callback)));
  }

  std::unique_ptr<PrintSettings> snooped_settings_;
  absl::optional<bool> print_now_result_;
  OnDidCreatePrintJobCallback on_did_create_print_job_;
};

class TestPrintViewManagerForDLP : public TestPrintViewManager {
 public:
  // Used to simulate Data Leak Prevention polices and possible user actions.
  enum class RestrictionLevel {
    // No DLP restrictions set - printing is allowed.
    kNotSet,
    // The user is warned and selects "continue" - printing is allowed.
    kWarnAllow,
    // The user is warned and selects "cancel" - printing is not allowed.
    kWarnCancel,
    // Printing is blocked, no print preview is shown.
    kBlock,
  };

  static TestPrintViewManagerForDLP* CreateForWebContents(
      content::WebContents* web_contents,
      RestrictionLevel restriction_level) {
    auto manager = std::make_unique<TestPrintViewManagerForDLP>(
        web_contents, restriction_level);
    auto* manager_ptr = manager.get();
    web_contents->SetUserData(PrintViewManager::UserDataKey(),
                              std::move(manager));
    return manager_ptr;
  }

  // Used by the TestPrintViewManagerForDLP to check that the correct action is
  // taken based on the restriction level.
  enum class PrintAllowance {
    // No checks done yet to determine whether printing is allowed or not.
    kUnknown,
    // There are no restrictions/user allowed printing.
    kAllowed,
    // There are BLOCK restrictions or user canceled the printing.
    kDisallowed,
  };

  TestPrintViewManagerForDLP(content::WebContents* web_contents,
                             RestrictionLevel restriction_level)
      : TestPrintViewManager(web_contents),
        restriction_level_(restriction_level) {
    PrintViewManager::SetReceiverImplForTesting(this);
  }
  TestPrintViewManagerForDLP(const TestPrintViewManagerForDLP&) = delete;
  TestPrintViewManagerForDLP& operator=(const TestPrintViewManagerForDLP&) =
      delete;
  ~TestPrintViewManagerForDLP() override {
    PrintViewManager::SetReceiverImplForTesting(nullptr);
  }

  PrintAllowance GetPrintAllowance() const { return allowance_; }

 private:
  void RejectPrintPreviewRequestIfRestricted(
      content::GlobalRenderFrameHostId rfh_id,
      base::OnceCallback<void(bool)> callback) override {
    switch (restriction_level_) {
      case RestrictionLevel::kNotSet:
      case RestrictionLevel::kWarnAllow:
        std::move(callback).Run(true);
        break;
      case RestrictionLevel::kBlock:
      case RestrictionLevel::kWarnCancel:
        std::move(callback).Run(false);
        break;
    }
  }

  void PrintPreviewRejectedForTesting() override {
    run_loop_->Quit();
    allowance_ = PrintAllowance::kDisallowed;
  }

  void PrintPreviewAllowedForTesting() override {
    run_loop_->Quit();
    allowance_ = PrintAllowance::kAllowed;
  }

  RestrictionLevel restriction_level_ = RestrictionLevel::kNotSet;
  PrintAllowance allowance_ = PrintAllowance::kUnknown;
};

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
    auto print_params = mojom::PrintPagesParams::New();
    print_params->params = mojom::PrintParams::New();
    std::move(callback).Run(std::move(print_params));

    for (auto& observer : GetObservers())
      observer.OnScriptedPrint();
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
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

class PrintBrowserTest : public InProcessBrowserTest {
 public:
  struct PrintParams {
    bool print_only_selection = false;
    int pages_per_sheet = 1;
  };

  PrintBrowserTest() = default;
  ~PrintBrowserTest() override = default;

  void SetUp() override {
    test_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
    PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());
    PrintingContext::SetPrintingContextFactoryForTest(
        &test_printing_context_factory_);

    num_expected_messages_ = 1;  // By default, only wait on one message.
    num_received_messages_ = 0;
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Safe to use `base::Unretained(this)` since this testing class
    // necessarily must outlive all interactions from the tests which will
    // run through the printing stack using derivatives of
    // `PrintViewManagerBase` and `PrintPreviewHandler`, which can trigger
    // this callback.
    SetShowPrintErrorDialogForTest(base::BindRepeating(
        &PrintBrowserTest::ShowPrintErrorDialog, base::Unretained(this)));

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    SetShowPrintErrorDialogForTest(base::NullCallback());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    PrintingContext::SetPrintingContextFactoryForTest(/*factory=*/nullptr);
    PrintBackend::SetPrintBackendForTesting(/*print_backend=*/nullptr);
  }

  void AddPrinter(const std::string& printer_name) {
    PrinterBasicInfo printer_info(
        printer_name,
        /*display_name=*/"test printer",
        /*printer_description=*/"A printer for testing.",
        /*printer_status=*/0,
        /*is_default=*/true, kTestDummyPrintInfoOptions);

    auto default_caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    default_caps->copies_max = kTestPrinterCapabilitiesMaxCopies;
    default_caps->dpis = kTestPrinterCapabilitiesDefaultDpis;
    default_caps->default_dpi = kTestPrinterCapabilitiesDpi;
    test_print_backend_->AddValidPrinter(
        printer_name, std::move(default_caps),
        std::make_unique<PrinterBasicInfo>(printer_info));
  }

  void SetPrinterNameForSubsequentContexts(const std::string& printer_name) {
    test_printing_context_factory_.SetPrinterNameForSubsequentContexts(
        printer_name);
  }

  void PrintAndWaitUntilPreviewIsReady() {
    const PrintParams kParams;
    PrintAndWaitUntilPreviewIsReady(kParams);
  }

  void PrintAndWaitUntilPreviewIsReady(const PrintParams& params) {
    PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false,
                                                params.pages_per_sheet);

    StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
               /*print_renderer=*/mojo::NullAssociatedRemote(),
               /*print_preview_disabled=*/false, params.print_only_selection);

    print_preview_observer.WaitUntilPreviewIsReady();

    set_rendered_page_count(print_preview_observer.rendered_page_count());
  }

  void PrintAndWaitUntilPreviewIsReadyAndLoaded() {
    const PrintParams kParams;
    PrintAndWaitUntilPreviewIsReadyAndLoaded(kParams);
  }

  void PrintAndWaitUntilPreviewIsReadyAndLoaded(const PrintParams& params) {
    PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true,
                                                params.pages_per_sheet);

    StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
               /*print_renderer=*/mojo::NullAssociatedRemote(),
               /*print_preview_disabled=*/false, params.print_only_selection);

    print_preview_observer.WaitUntilPreviewIsReady();

    set_rendered_page_count(print_preview_observer.rendered_page_count());
  }

  // The following are helper functions for having a wait loop in the test and
  // exit when all expected messages are received.
  void SetNumExpectedMessages(unsigned int num) {
    num_expected_messages_ = num;
  }

  void WaitUntilCallbackReceived() {
    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CheckForQuit() {
    if (++num_received_messages_ != num_expected_messages_)
      return;
    if (quit_callback_)
      std::move(quit_callback_).Run();
  }

  void CreateTestPrintRenderFrame(content::RenderFrameHost* frame_host,
                                  content::WebContents* web_contents) {
    frame_content_.emplace(
        frame_host, std::make_unique<TestPrintRenderFrame>(
                        frame_host, web_contents, kDefaultDocumentCookie,
                        base::BindRepeating(&PrintBrowserTest::CheckForQuit,
                                            base::Unretained(this))));
    OverrideBinderForTesting(frame_host);
  }

  static mojom::PrintFrameContentParamsPtr GetDefaultPrintFrameParams() {
    return mojom::PrintFrameContentParams::New(gfx::Rect(800, 600),
                                               kDefaultDocumentCookie);
  }

  const mojo::AssociatedRemote<mojom::PrintRenderFrame>& GetPrintRenderFrame(
      content::RenderFrameHost* rfh) {
    if (!remote_)
      rfh->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
    return remote_;
  }

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

 private:
  TestPrintRenderFrame* GetFrameContent(content::RenderFrameHost* host) const {
    auto iter = frame_content_.find(host);
    return iter != frame_content_.end() ? iter->second.get() : nullptr;
  }

  void OverrideBinderForTesting(content::RenderFrameHost* render_frame_host) {
    render_frame_host->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PrintRenderFrame::Name_,
            base::BindRepeating(
                &TestPrintRenderFrame::Bind,
                base::Unretained(GetFrameContent(render_frame_host))));
  }

  void ShowPrintErrorDialog() {
    ++error_dialog_shown_count_;
    CheckForQuit();
  }

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

class SitePerProcessPrintBrowserTest : public PrintBrowserTest {
 public:
  SitePerProcessPrintBrowserTest() = default;
  ~SitePerProcessPrintBrowserTest() override = default;

  // content::BrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }
};

class IsolateOriginsPrintBrowserTest : public PrintBrowserTest {
 public:
  static constexpr char kIsolatedSite[] = "b.com";

  IsolateOriginsPrintBrowserTest() = default;
  ~IsolateOriginsPrintBrowserTest() override = default;

  // content::BrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->Start());

    std::string origin_list =
        embedded_test_server()->GetURL(kIsolatedSite, "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

class BackForwardCachePrintBrowserTest : public PrintBrowserTest {
 public:
  BackForwardCachePrintBrowserTest() = default;
  BackForwardCachePrintBrowserTest(const BackForwardCachePrintBrowserTest&) =
      delete;
  BackForwardCachePrintBrowserTest& operator=(
      const BackForwardCachePrintBrowserTest&) = delete;
  ~BackForwardCachePrintBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{::features::kBackForwardCache,
          // Set a very long TTL before expiration (longer than the test
          // timeout) so tests that are expecting deletion don't pass when
          // they shouldn't.
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {::features::kBackForwardCacheMemoryControls});

    PrintBrowserTest::SetUpCommandLine(command_line);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  void ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature feature,
      base::Location location) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(feature);
    AddSampleToBuckets(&expected_blocklisted_features_, sample);

    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            "BackForwardCache.HistoryNavigationOutcome."
            "BlocklistedFeature"),
        testing::UnorderedElementsAreArray(expected_blocklisted_features_))
        << location.ToString();

    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            "BackForwardCache.AllSites.HistoryNavigationOutcome."
            "BlocklistedFeature"),
        testing::UnorderedElementsAreArray(expected_blocklisted_features_))
        << location.ToString();
  }

 private:
  void AddSampleToBuckets(std::vector<base::Bucket>* buckets,
                          base::HistogramBase::Sample sample) {
    auto it = base::ranges::find(*buckets, sample, &base::Bucket::min);
    if (it == buckets->end()) {
      buckets->push_back(base::Bucket(sample, 1));
    } else {
      it->count++;
    }
  }

  base::HistogramTester histogram_tester_;
  std::vector<base::Bucket> expected_blocklisted_features_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

constexpr char IsolateOriginsPrintBrowserTest::kIsolatedSite[];

class PrintExtensionBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  PrintExtensionBrowserTest() = default;
  ~PrintExtensionBrowserTest() override = default;

  void PrintAndWaitUntilPreviewIsReady() {
    PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);

    StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
               /*print_renderer=*/mojo::NullAssociatedRemote(),
               /*print_preview_disabled=*/false,
               /*has_selection=*/false);

    print_preview_observer.WaitUntilPreviewIsReady();
  }

  void LoadExtensionAndNavigateToOptionPage() {
    const extensions::Extension* extension = nullptr;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::FilePath test_data_dir;
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
      extension = LoadExtension(
          test_data_dir.AppendASCII("printing").AppendASCII("test_extension"));
      ASSERT_TRUE(extension);
    }

    GURL url(chrome::kChromeUIExtensionsURL);
    std::string query =
        base::StringPrintf("options=%s", extension->id().c_str());
    GURL::Replacements replacements;
    replacements.SetQueryStr(query);
    url = url.ReplaceComponents(replacements);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }
};

class SitePerProcessPrintExtensionBrowserTest
    : public PrintExtensionBrowserTest {
 public:
  // content::BrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }
};

// Printing only a selection containing iframes is partially supported.
// Iframes aren't currently displayed. This test passes whenever the print
// preview is rendered (i.e. no timeout in the test).
// This test shouldn't crash. See https://crbug.com/732780.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, SelectionContainsIframe) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/selection_iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const PrintParams kParams{.print_only_selection = true};
  PrintAndWaitUntilPreviewIsReady(kParams);
}

// https://crbug.com/1125972
// https://crbug.com/1131598
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, NoScrolling) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/with-scrollable.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  const char kExpression1[] = "iframe.contentWindow.scrollY";
  const char kExpression2[] = "scrollable.scrollTop";
  const char kExpression3[] = "shapeshifter.scrollTop";

  double old_scroll1 = content::EvalJs(contents, kExpression1).ExtractDouble();
  double old_scroll2 = content::EvalJs(contents, kExpression2).ExtractDouble();
  double old_scroll3 = content::EvalJs(contents, kExpression3).ExtractDouble();

  PrintAndWaitUntilPreviewIsReady();

  double new_scroll1 = content::EvalJs(contents, kExpression1).ExtractDouble();

  // TODO(crbug.com/1131598): Perform the corresponding EvalJs() calls here and
  // assign to new_scroll2 and new_scroll3, once the printing code has been
  // fixed to handle these cases. Right now, the scroll offset jumps.
  double new_scroll2 = old_scroll2;
  double new_scroll3 = old_scroll3;

  EXPECT_EQ(old_scroll1, new_scroll1);
  EXPECT_EQ(old_scroll2, new_scroll2);
  EXPECT_EQ(old_scroll3, new_scroll3);
}

// https://crbug.com/1131598
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, DISABLED_NoScrollingFrameset) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/frameset.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  const char kExpression[] =
      "document.getElementById('frame').contentWindow.scrollY";

  double old_scroll = content::EvalJs(contents, kExpression).ExtractDouble();

  PrintAndWaitUntilPreviewIsReady();

  double new_scroll = content::EvalJs(contents, kExpression).ExtractDouble();

  EXPECT_EQ(old_scroll, new_scroll);
}

// https://crbug.com/1125972
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, NoScrollingVerticalRl) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/vertical-rl.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  PrintAndWaitUntilPreviewIsReady();

  // Test that entering print preview didn't mess up the scroll position.
  EXPECT_EQ(
      0, content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         "window.scrollX"));
}

// https://crbug.com/1285208
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, LegacyLayoutEngineFallback) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/legacy-layout-engine-known-bug.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  const char kExpression[] = "target.offsetHeight";

  // The non-printed document should be laid out with LayoutNG. We're testing
  // this by looking for a known margin-collapsing / clearance bug in the legacy
  // engine, not present in LayoutNG. The height should be 0 if the bug isn't
  // present.

  double old_height = content::EvalJs(contents, kExpression).ExtractDouble();
  if (old_height != 0) {
    // LayoutNG seems to be disabled. There's nothing useful to test here then.
    return;
  }

  // Entering print preview may trigger legacy engine fallback, but this should
  // only be temporary.
  PrintAndWaitUntilPreviewIsReady();

  // The non-printed document should still be laid out with LayoutNG.
  double new_height = content::EvalJs(contents, kExpression).ExtractDouble();
  EXPECT_EQ(new_height, 0);
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest, LazyLoadedImagesFetched) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/lazy-loaded-image-offscreen.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  const char kExpression[] = "target.offsetHeight";

  double old_height = content::EvalJs(contents, kExpression).ExtractDouble();

  PrintAndWaitUntilPreviewIsReady();

  // The non-printed document should have loaded the image, which will have
  // a different height.
  double new_height = content::EvalJs(contents, kExpression).ExtractDouble();
  EXPECT_NE(old_height, new_height);
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest, LazyLoadedIframeFetched) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/lazy-loaded-iframe-offscreen.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  const char kExpression[] =
      "target.contentWindow.document.documentElement.clientHeight";

  double old_height = content::EvalJs(contents, kExpression).ExtractDouble();

  PrintAndWaitUntilPreviewIsReady();

  double new_height = content::EvalJs(contents, kExpression).ExtractDouble();

  EXPECT_NE(old_height, new_height);
}

// TODO(crbug.com/1305193)  Reenable after flakes have been resolved.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest,
                       DISABLED_LazyLoadedIframeFetchedCrossOrigin) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/lazy-loaded-iframe-offscreen-cross-origin.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  const char kExpression[] = "document.documentElement.clientHeight";

  double old_height =
      content::EvalJs(content::ChildFrameAt(contents, 0), kExpression)
          .ExtractDouble();

  PrintAndWaitUntilPreviewIsReady();

  double new_height =
      content::EvalJs(content::ChildFrameAt(contents, 0), kExpression)
          .ExtractDouble();

  EXPECT_NE(old_height, new_height);
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest, LazyLoadedImagesFetchedScriptedPrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/lazy-loaded-image-offscreen.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  const char kExpression[] = "target.offsetHeight";

  double old_height = content::EvalJs(contents, kExpression).ExtractDouble();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  TestPrintViewManager* print_view_manager =
      TestPrintViewManager::CreateForWebContents(web_contents);

  content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(),
                              "window.print();");
  print_view_manager->WaitUntilPreviewIsShownOrCancelled();

  // The non-printed document should have loaded the image, which will have
  // a different height.
  double new_height = content::EvalJs(contents, kExpression).ExtractDouble();
  EXPECT_NE(old_height, new_height);
}

// Before invoking print preview, page scale is changed to a different value.
// Test that when print preview is ready, in other words when printing is
// finished, the page scale factor gets reset to initial scale.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, ResetPageScaleAfterPrintPreview) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  contents->SetPageScale(1.5);

  PrintAndWaitUntilPreviewIsReady();

  double contents_page_scale_after_print =
      content::EvalJs(contents, "window.visualViewport.scale").ExtractDouble();

  constexpr double kContentsInitialScale = 1.0;
  EXPECT_EQ(kContentsInitialScale, contents_page_scale_after_print);
}

// Printing frame content for the main frame of a generic webpage.
// This test passes when the printed result is sent back and checked in
// TestPrintRenderFrame::OnDidPrintFrameContent().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintFrameContent) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = original_contents->GetPrimaryMainFrame();
  CreateTestPrintRenderFrame(rfh, original_contents);
  GetPrintRenderFrame(rfh)->PrintFrameContent(GetDefaultPrintFrameParams(),
                                              base::DoNothing());

  // The printed result will be received and checked in
  // TestPrintRenderFrame.
  WaitUntilCallbackReceived();
}

// Printing frame content for a cross-site iframe.
// This test passes when the iframe responds to the print message.
// The response is checked in TestPrintRenderFrame::OnDidPrintFrameContent().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintSubframeContent) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(
      embedded_test_server()->GetURL("/printing/content_with_iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* test_frame = ChildFrameAt(original_contents, 0);
  ASSERT_TRUE(test_frame);

  CreateTestPrintRenderFrame(test_frame, original_contents);
  GetPrintRenderFrame(test_frame)
      ->PrintFrameContent(GetDefaultPrintFrameParams(), base::DoNothing());

  // The printed result will be received and checked in
  // TestPrintRenderFrame.
  WaitUntilCallbackReceived();
}

// Printing frame content with a cross-site iframe which also has a cross-site
// iframe. The site reference chain is a.com --> b.com --> c.com.
// This test passes when both cross-site frames are printed and their
// responses which are checked in
// TestPrintRenderFrame::OnDidPrintFrameContent().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintSubframeChain) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/content_with_iframe_chain.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Create composite client so subframe print message can be forwarded.
  PrintCompositeClient::CreateForWebContents(original_contents);

  content::RenderFrameHost* main_frame =
      original_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);
  ASSERT_NE(child_frame, main_frame);
  bool oopif_enabled = child_frame->GetProcess() != main_frame->GetProcess();

  content::RenderFrameHost* grandchild_frame =
      content::ChildFrameAt(child_frame, 0);
  ASSERT_TRUE(grandchild_frame);
  ASSERT_NE(grandchild_frame, child_frame);
  if (oopif_enabled) {
    ASSERT_NE(grandchild_frame->GetProcess(), child_frame->GetProcess());
    ASSERT_NE(grandchild_frame->GetProcess(), main_frame->GetProcess());
  }

  CreateTestPrintRenderFrame(main_frame, original_contents);
  if (oopif_enabled) {
    CreateTestPrintRenderFrame(child_frame, original_contents);
    CreateTestPrintRenderFrame(grandchild_frame, original_contents);
  }

  GetPrintRenderFrame(main_frame)
      ->PrintFrameContent(GetDefaultPrintFrameParams(), base::DoNothing());

  // The printed result will be received and checked in
  // TestPrintRenderFrame.
  SetNumExpectedMessages(oopif_enabled ? 3 : 1);
  WaitUntilCallbackReceived();
}

// Printing frame content with a cross-site iframe who also has a cross site
// iframe, but this iframe resides in the same site as the main frame.
// The site reference loop is a.com --> b.com --> a.com.
// This test passes when both cross-site frames are printed and send back
// responses which are checked in
// TestPrintRenderFrame::OnDidPrintFrameContent().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintSubframeABA) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/printing/content_with_iframe_loop.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Create composite client so subframe print message can be forwarded.
  PrintCompositeClient::CreateForWebContents(original_contents);

  content::RenderFrameHost* main_frame =
      original_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);
  ASSERT_NE(child_frame, main_frame);
  bool oopif_enabled = main_frame->GetProcess() != child_frame->GetProcess();

  content::RenderFrameHost* grandchild_frame =
      content::ChildFrameAt(child_frame, 0);
  ASSERT_TRUE(grandchild_frame);
  ASSERT_NE(grandchild_frame, child_frame);
  // `grandchild_frame` is in the same site as `frame`, so whether OOPIF is
  // enabled, they will be in the same process.
  ASSERT_EQ(grandchild_frame->GetProcess(), main_frame->GetProcess());

  CreateTestPrintRenderFrame(main_frame, original_contents);
  if (oopif_enabled) {
    CreateTestPrintRenderFrame(child_frame, original_contents);
    CreateTestPrintRenderFrame(grandchild_frame, original_contents);
  }

  GetPrintRenderFrame(main_frame)
      ->PrintFrameContent(GetDefaultPrintFrameParams(), base::DoNothing());

  // The printed result will be received and checked in
  // TestPrintRenderFrame.
  SetNumExpectedMessages(oopif_enabled ? 3 : 1);
  WaitUntilCallbackReceived();
}

// Printing frame content with a cross-site iframe before creating
// PrintCompositor by the main frame.
// This test passes if PrintCompositeClient queues subframes when
// it doesn't have PrintCompositor and clears them after PrintCompositor is
// created.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest,
                       PrintSubframeContentBeforeCompositeClientCreation) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(
      embedded_test_server()->GetURL("/printing/content_with_iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // When OOPIF is not enabled, CompositorClient is not used.
  if (!IsOopifEnabled())
    return;

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame =
      original_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(main_frame);
  content::RenderFrameHost* test_frame = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(test_frame);
  ASSERT_NE(main_frame->GetProcess(), test_frame->GetProcess());

  CreateTestPrintRenderFrame(main_frame, original_contents);
  CreateTestPrintRenderFrame(test_frame, original_contents);
  SetNumExpectedMessages(2);

  // Print on the main frame.
  GetPrintRenderFrame(main_frame)
      ->PrintFrameContent(GetDefaultPrintFrameParams(), base::DoNothing());

  // The printed result will be received and checked in TestPrintRenderFrame.
  WaitUntilCallbackReceived();

  // As PrintFrameContent() with the main frame doesn't call
  // PrintCompositeClient::DoCompositeDocumentToPdf() on this test, when
  // PrintCompositeClient::OnDidPrintFrameContent() is called with the sub
  // frame, it doesn't have mojom::PrintCompositor.
  auto* client = PrintCompositeClient::FromWebContents(original_contents);
  ASSERT_FALSE(client->compositor_);

  // When there is no mojom::PrintCompositor, PrintCompositeClient queues
  // subframes and handles them when mojom::PrintCompositor is created.
  // `requested_subframes_` should have the requested subframes.
  ASSERT_EQ(1u, client->requested_subframes_.size());
  PrintCompositeClient::RequestedSubFrame* subframe_in_queue =
      client->requested_subframes_.begin()->get();
  ASSERT_EQ(kDefaultDocumentCookie, subframe_in_queue->document_cookie_);
  ASSERT_EQ(test_frame->GetGlobalId(), subframe_in_queue->rfh_id_);

  // Creates mojom::PrintCompositor.
  client->DoCompositeDocumentToPdf(
      kDefaultDocumentCookie, main_frame,
      *TestPrintRenderFrame::GetDefaultDidPrintContentParams(),
      base::DoNothing());
  ASSERT_TRUE(client->GetCompositeRequest(kDefaultDocumentCookie));
  // `requested_subframes_` should be empty.
  ASSERT_TRUE(client->requested_subframes_.empty());
}

// Printing preview a simple webpage when site per process is enabled.
// Test that the basic oopif printing should succeed. The test should not crash
// or timed out. There could be other reasons that cause the test fail, but the
// most obvious ones would be font access outage or web sandbox support being
// absent because we explicitly check these when pdf compositor service starts.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest, BasicPrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  PrintAndWaitUntilPreviewIsReady();
}

// Printing a web page with a dead subframe for site per process should succeed.
// This test passes whenever the print preview is rendered. This should not be
// a timed out test which indicates the print preview hung.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest,
                       SubframeUnavailableBeforePrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(
      embedded_test_server()->GetURL("/printing/content_with_iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* test_frame = ChildFrameAt(original_contents, 0);
  ASSERT_TRUE(test_frame);
  ASSERT_TRUE(test_frame->IsRenderFrameLive());
  // Wait for the renderer to be down.
  content::RenderProcessHostWatcher render_process_watcher(
      test_frame->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  // Shutdown the subframe.
  ASSERT_TRUE(test_frame->GetProcess()->Shutdown(0));
  render_process_watcher.Wait();
  ASSERT_FALSE(test_frame->IsRenderFrameLive());

  PrintAndWaitUntilPreviewIsReady();
}

// If a subframe dies during printing, the page printing should still succeed.
// This test passes whenever the print preview is rendered. This should not be
// a timed out test which indicates the print preview hung.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest,
                       SubframeUnavailableDuringPrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(
      embedded_test_server()->GetURL("/printing/content_with_iframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* subframe = ChildFrameAt(original_contents, 0);
  ASSERT_TRUE(subframe);
  auto* subframe_rph = subframe->GetProcess();

  KillPrintRenderFrame frame_content(subframe_rph);
  frame_content.OverrideBinderForTesting(subframe);

  // Waits for the renderer to be down.
  content::RenderProcessHostWatcher process_watcher(
      subframe_rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Adds the observer to get the status for the preview.
  PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);
  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false, /*has_selection*/ false);

  // Makes sure that `subframe_rph` is terminated.
  process_watcher.Wait();
  // Confirms that the preview pages are rendered.
  print_preview_observer.WaitUntilPreviewIsReady();
}

// Printing preview a web page with an iframe from an isolated origin.
// This test passes whenever the print preview is rendered. This should not be
// a timed out test which indicates the print preview hung or crash.
IN_PROC_BROWSER_TEST_F(IsolateOriginsPrintBrowserTest, PrintIsolatedSubframe) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/content_with_same_site_iframe.html"));
  GURL isolated_url(
      embedded_test_server()->GetURL(kIsolatedSite, "/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(NavigateIframeToURL(original_contents, "iframe", isolated_url));

  auto* main_frame = original_contents->GetPrimaryMainFrame();
  auto* subframe = ChildFrameAt(main_frame, 0);
  ASSERT_NE(main_frame->GetProcess(), subframe->GetProcess());

  PrintAndWaitUntilPreviewIsReady();
}

// Printing preview a webpage.
// Test that we use oopif printing by default when full site isolation is
// enabled.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, RegularPrinting) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_EQ(content::AreAllSitesIsolatedForTesting(), IsOopifEnabled());
}

#if BUILDFLAG(IS_CHROMEOS)
// Test that if user allows printing after being shown a warning due to DLP
// restrictions, the print preview is rendered.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, DLPWarnAllowed) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  // Set up the print view manager and DLP restrictions.
  TestPrintViewManagerForDLP* print_view_manager =
      TestPrintViewManagerForDLP::CreateForWebContents(
          web_contents,
          TestPrintViewManagerForDLP::RestrictionLevel::kWarnAllow);

  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kUnknown);
  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false,
             /*has_selection=*/false);
  print_view_manager->WaitUntilPreviewIsShownOrCancelled();
  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kAllowed);
}

// Test that if user cancels printing after being shown a warning due to DLP
// restrictions, the print preview is not rendered.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, DLPWarnCanceled) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  // Set up the print view manager and DLP restrictions.
  TestPrintViewManagerForDLP* print_view_manager =
      TestPrintViewManagerForDLP::CreateForWebContents(
          web_contents,
          TestPrintViewManagerForDLP::RestrictionLevel::kWarnCancel);

  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kUnknown);
  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false,
             /*has_selection=*/false);
  print_view_manager->WaitUntilPreviewIsShownOrCancelled();
  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kDisallowed);
}

// Test that if printing is blocked due to DLP restrictions, the print preview
// is not rendered.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, DLPBlocked) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  // Set up the print view manager and DLP restrictions.
  TestPrintViewManagerForDLP* print_view_manager =
      TestPrintViewManagerForDLP::CreateForWebContents(
          web_contents, TestPrintViewManagerForDLP::RestrictionLevel::kBlock);

  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kUnknown);
  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false,
             /*has_selection=*/false);
  print_view_manager->WaitUntilPreviewIsShownOrCancelled();
  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kDisallowed);
}

// Test that if user allows printing after being shown a warning due to DLP
// restrictions, the print preview is rendered when initiated by window.print().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, DLPWarnAllowedWithWindowDotPrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Set up the print view manager and DLP restrictions.
  TestPrintViewManagerForDLP* print_view_manager =
      TestPrintViewManagerForDLP::CreateForWebContents(
          web_contents,
          TestPrintViewManagerForDLP::RestrictionLevel::kWarnAllow);

  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kUnknown);
  content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(),
                              "window.print();");
  print_view_manager->WaitUntilPreviewIsShownOrCancelled();
  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kAllowed);
}

// Test that if user cancels printing after being shown a warning due to DLP
// restrictions, the print preview is not rendered when initiated by
// window.print().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, DLPWarnCanceledWithWindowDotPrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Set up the print view manager and DLP restrictions.
  TestPrintViewManagerForDLP* print_view_manager =
      TestPrintViewManagerForDLP::CreateForWebContents(
          web_contents,
          TestPrintViewManagerForDLP::RestrictionLevel::kWarnCancel);

  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kUnknown);
  content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(),
                              "window.print();");
  print_view_manager->WaitUntilPreviewIsShownOrCancelled();
  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kDisallowed);
}

// Test that if printing is blocked due to DLP restrictions, the print preview
// is not rendered when initiated by window.print().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, DLPBlockedWithWindowDotPrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Set up the print view manager and DLP restrictions.
  TestPrintViewManagerForDLP* print_view_manager =
      TestPrintViewManagerForDLP::CreateForWebContents(
          web_contents, TestPrintViewManagerForDLP::RestrictionLevel::kBlock);

  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kUnknown);
  content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(),
                              "window.print();");
  print_view_manager->WaitUntilPreviewIsShownOrCancelled();
  ASSERT_EQ(print_view_manager->GetPrintAllowance(),
            TestPrintViewManagerForDLP::PrintAllowance::kDisallowed);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Printing preview a webpage with isolate-origins enabled.
// Test that we will use oopif printing for this case.
IN_PROC_BROWSER_TEST_F(IsolateOriginsPrintBrowserTest, OopifPrinting) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(IsOopifEnabled());
}

IN_PROC_BROWSER_TEST_F(BackForwardCachePrintBrowserTest, DisableCaching) {
  ASSERT_TRUE(embedded_test_server()->Started());

  // 1) Navigate to A and trigger printing.
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/no-favicon.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* rfh_a = current_frame_host();
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  PrintAndWaitUntilPreviewIsReady();

  // 2) Navigate to B.
  // The first page is not cached because printing preview was open.
  GURL url_2(embedded_test_server()->GetURL(
      "b.com", "/back_forward_cache/no-favicon.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_2));
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Navigate back and checks the blocklisted feature is recorded in UMA.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kPrinting, FROM_HERE);
}

// Printing an extension option page.
// The test should not crash or timeout.
IN_PROC_BROWSER_TEST_F(PrintExtensionBrowserTest, PrintOptionPage) {
  LoadExtensionAndNavigateToOptionPage();
  PrintAndWaitUntilPreviewIsReady();
}

// Printing an extension option page with site per process is enabled.
// The test should not crash or timeout.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintExtensionBrowserTest,
                       PrintOptionPage) {
  LoadExtensionAndNavigateToOptionPage();
  PrintAndWaitUntilPreviewIsReady();
}

// Printing frame content for the main frame of a generic webpage with N-up
// printing. This is a regression test for https://crbug.com/937247
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintNup) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipagenup.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  TestPrintViewManager print_view_manager(web_contents);
  PrintViewManager::SetReceiverImplForTesting(&print_view_manager);

  // Override print parameters to do N-up, specify 4 pages per sheet.
  const PrintParams kParams{.pages_per_sheet = 4};
  PrintAndWaitUntilPreviewIsReady(kParams);

  PrintViewManager::SetReceiverImplForTesting(nullptr);

  // With 4 pages per sheet requested by `GetPrintParams()`, a 7 page input
  // will result in 2 pages in the print preview.
  EXPECT_EQ(rendered_page_count(), 2u);
}

// Site per process version of PrintBrowserTest.PrintNup.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest, PrintNup) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipagenup.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  TestPrintViewManager print_view_manager(web_contents);
  PrintViewManager::SetReceiverImplForTesting(&print_view_manager);

  // Override print parameters to do N-up, specify 4 pages per sheet.
  const PrintParams kParams{.pages_per_sheet = 4};
  PrintAndWaitUntilPreviewIsReady(kParams);

  PrintViewManager::SetReceiverImplForTesting(nullptr);

  // With 4 pages per sheet requested by `GetPrintParams()`, a 7 page input
  // will result in 2 pages in the print preview.
  EXPECT_EQ(rendered_page_count(), 2u);
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest, MultipagePrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  PrintAndWaitUntilPreviewIsReadyAndLoaded();

  EXPECT_EQ(rendered_page_count(), 3u);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest, MultipagePrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  PrintAndWaitUntilPreviewIsReadyAndLoaded();

  EXPECT_EQ(rendered_page_count(), 3u);
}

// Disabled due to flakiness: crbug.com/1311998
IN_PROC_BROWSER_TEST_F(PrintBrowserTest,
                       DISABLED_PDFPluginNotKeyboardFocusable) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false, /*has_selection=*/false);
  print_preview_observer.WaitUntilPreviewIsReady();

  content::WebContents* preview_dialog =
      print_preview_observer.GetPrintPreviewDialog();
  ASSERT_TRUE(preview_dialog);

  // The script will ensure we return the id of <zoom-out-button> when
  // focused. Focus the element after PDF plugin in tab order.
  const char kScript[] = R"(
    const button = document.getElementsByTagName('print-preview-app')[0]
                       .$['previewArea']
                       .shadowRoot.querySelector('iframe')
                       .contentDocument.querySelector('pdf-viewer-pp')
                       .shadowRoot.querySelector('#zoomToolbar')
                       .$['zoom-out-button'];
    button.addEventListener('focus', (e) => {
      window.domAutomationController.send(e.target.id);
    });

    const select_tag = document.getElementsByTagName('print-preview-app')[0]
                           .$['sidebar']
                           .$['destinationSettings']
                           .$['destinationSelect'];
    select_tag.addEventListener('focus', () => {
      window.domAutomationController.send(true);
    });
    select_tag.focus();)";
  bool success = false;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractBool(preview_dialog, kScript, &success));
  ASSERT_TRUE(success);

  // Simulate a <shift-tab> press and wait for a focus message.
  content::DOMMessageQueue msg_queue(preview_dialog);
  SimulateKeyPress(preview_dialog, ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, true, false, false);
  std::string reply;
  ASSERT_TRUE(msg_queue.WaitForMessage(&reply));
  // Pressing <shift-tab> should focus the last toolbar element
  // (zoom-out-button) instead of PDF plugin.
  EXPECT_EQ("\"zoom-out-button\"", reply);
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest, WindowDotPrint) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);
  content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(),
                              "window.print();");
  print_preview_observer.WaitUntilPreviewIsReady();
}

class PrintPrerenderBrowserTest : public PrintBrowserTest {
 public:
  PrintPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PrintPrerenderBrowserTest::web_contents,
                                base::Unretained(this))) {}

  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    cmd_line->AppendSwitch(switches::kDisablePrintPreview);
    PrintBrowserTest::SetUpCommandLine(cmd_line);
  }

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    PrintBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Test that print() is silently ignored.
// https://wicg.github.io/nav-speculation/prerendering.html#patch-modals
IN_PROC_BROWSER_TEST_F(PrintPrerenderBrowserTest, QuietBlockWithWindowPrint) {
  // Navigate to an initial page.
  const GURL kUrl(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));

  // Start a prerender.
  GURL prerender_url =
      embedded_test_server()->GetURL("/printing/prerendering.html");

  content::WebContentsConsoleObserver console_observer(web_contents());
  int prerender_id = prerender_helper_.AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_host =
      prerender_helper_.GetPrerenderedMainFrameHost(prerender_id);
  EXPECT_EQ(0u, console_observer.messages().size());

  // Try to print by JS during prerendering.
  EXPECT_EQ(true, content::ExecJs(prerender_host, "window.print();",
                                  content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, content::EvalJs(prerender_host, "firedBeforePrint"));
  EXPECT_EQ(false, content::EvalJs(prerender_host, "firedAfterPrint"));
  EXPECT_EQ(1u, console_observer.messages().size());
}

// Test that execCommand('print') is silently ignored.
// execCommand() is not specced, but
// https://wicg.github.io/nav-speculation/prerendering.html#patch-modals
// indicates the intent to silently ignore print APIs.
IN_PROC_BROWSER_TEST_F(PrintPrerenderBrowserTest,
                       QuietBlockWithDocumentExecCommand) {
  // Navigate to an initial page.
  const GURL kUrl(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));

  // Start a prerender.
  GURL prerender_url =
      embedded_test_server()->GetURL("/printing/prerendering.html");

  content::WebContentsConsoleObserver console_observer(web_contents());
  int prerender_id = prerender_helper_.AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_host =
      prerender_helper_.GetPrerenderedMainFrameHost(prerender_id);
  EXPECT_EQ(0u, console_observer.messages().size());

  // Try to print by JS during prerendering.
  EXPECT_EQ(false,
            content::EvalJs(prerender_host, "document.execCommand('print');"));
  EXPECT_EQ(false, content::EvalJs(prerender_host, "firedBeforePrint"));
  EXPECT_EQ(false, content::EvalJs(prerender_host, "firedAfterPrint"));
  EXPECT_EQ(1u, console_observer.messages().size());
}

class PrintFencedFrameBrowserTest : public PrintBrowserTest {
 public:
  PrintFencedFrameBrowserTest() {
    fenced_frame_helper_ =
        std::make_unique<content::test::FencedFrameTestHelper>();
  }
  ~PrintFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PrintBrowserTest::SetUpOnMainThread();
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  PrintFencedFrameBrowserTest(const PrintFencedFrameBrowserTest&) = delete;
  PrintFencedFrameBrowserTest& operator=(const PrintFencedFrameBrowserTest&) =
      delete;

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::FencedFrameTestHelper* fenced_frame_test_helper() {
    return fenced_frame_helper_.get();
  }

 protected:
  content::RenderFrameHost* CreateFencedFrame(
      content::RenderFrameHost* fenced_frame_parent,
      const GURL& url) {
    if (fenced_frame_helper_)
      return fenced_frame_helper_->CreateFencedFrame(fenced_frame_parent, url);

    // FencedFrameTestHelper only supports the MPArch version of fenced frames.
    // So need to maually create a fenced frame for the ShadowDOM version.
    content::TestNavigationManager navigation(web_contents(), url);
    constexpr char kAddFencedFrameScript[] = R"({
        const fenced_frame = document.createElement('fencedframe');
        fenced_frame.src = $1;
        document.body.appendChild(fenced_frame);
    })";
    EXPECT_TRUE(ExecJs(fenced_frame_parent,
                       content::JsReplace(kAddFencedFrameScript, url)));
    navigation.WaitForNavigationFinished();

    content::RenderFrameHost* new_frame = ChildFrameAt(fenced_frame_parent, 0);

    return new_frame;
  }

  void RunPrintTest(const std::string& print_command) {
    // Navigate to an initial page.
    const GURL url(https_server_.GetURL("/empty.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    // Load a fenced frame.
    GURL fenced_frame_url = https_server_.GetURL("/fenced_frames/title1.html");
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* fenced_frame_host = CreateFencedFrame(
        web_contents->GetPrimaryMainFrame(), fenced_frame_url);
    ASSERT_TRUE(fenced_frame_host);
    content::WebContentsConsoleObserver console_observer(web_contents);
    EXPECT_EQ(0u, console_observer.messages().size());

    constexpr char kAddListenersScript[] = R"(
        (async () => {
          let firedBeforePrint = false;
          let firedAfterPrint = false;
          window.addEventListener('beforeprint', () => {
            firedBeforePrint = true;
          });
          window.addEventListener('afterprint', () => {
            firedAfterPrint = true;
          });
          %s
          return 'beforeprint: ' + firedBeforePrint +
                 ', afterprint: ' + firedAfterPrint;
        })();
      )";
    const std::string test_script =
        base::StringPrintf(kAddListenersScript, print_command.c_str());

    EXPECT_EQ("beforeprint: false, afterprint: false",
              content::EvalJs(fenced_frame_host, test_script));
    ASSERT_TRUE(console_observer.Wait());
    ASSERT_EQ(1u, console_observer.messages().size());
    EXPECT_EQ(
        "Ignored call to 'print()'. The document is in a fenced frame tree.",
        console_observer.GetMessageAt(0));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::test::FencedFrameTestHelper> fenced_frame_helper_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(PrintFencedFrameBrowserTest, ScriptedPrint) {
  RunPrintTest("window.print();");
}

IN_PROC_BROWSER_TEST_F(PrintFencedFrameBrowserTest, DocumentExecCommand) {
  RunPrintTest("document.execCommand('print');");
}

// TODO(crbug.com/822505)  ChromeOS uses different testing setup that isn't
// hooked up to make use of `TestPrintingContext` yet.
#if !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
class TestPrintJobWorker : public PrintJobWorker {
 public:
  // Callbacks to run for overrides.
  struct PrintCallbacks {
    OnUseDefaultSettingsCallback did_use_default_settings_callback;
    OnGetSettingsWithUICallback did_get_settings_with_ui_callback;
  };

  TestPrintJobWorker(content::GlobalRenderFrameHostId rfh_id,
                     PrintCallbacks* callbacks)
      : PrintJobWorker(rfh_id), callbacks_(callbacks) {}
  TestPrintJobWorker(const TestPrintJobWorker&) = delete;
  TestPrintJobWorker& operator=(const TestPrintJobWorker&) = delete;
  ~TestPrintJobWorker() override = default;

 private:
  void UseDefaultSettings(SettingsCallback callback) override {
    DVLOG(1) << "Observed: invoke use default settings";
    PrintJobWorker::UseDefaultSettings(std::move(callback));
    callbacks_->did_use_default_settings_callback.Run();
  }

  void GetSettingsWithUI(uint32_t document_page_count,
                         bool has_selection,
                         bool is_scripted,
                         SettingsCallback callback) override {
    DVLOG(1) << "Observed: invoke get settings with UI";
    PrintJobWorker::GetSettingsWithUI(document_page_count, has_selection,
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
#if BUILDFLAG(IS_WIN)
    OnDidAskUserForSettingsCallback did_ask_user_for_settings_callback;
#endif
    OnDidStartPrintingCallback did_start_printing_callback;
#if BUILDFLAG(IS_WIN)
    OnDidRenderPrintedPageCallback did_render_printed_page_callback;
#endif
    OnDidRenderPrintedDocumentCallback did_render_printed_document_callback;
    OnDidDocumentDoneCallback did_document_done_callback;
    OnDidCancelCallback did_cancel_callback;
  };

  TestPrintJobWorkerOop(content::GlobalRenderFrameHostId rfh_id,
                        bool simulate_spooling_memory_errors,
                        PrintCallbacks* callbacks)
      : PrintJobWorkerOop(rfh_id, simulate_spooling_memory_errors),
        callbacks_(callbacks) {}
  TestPrintJobWorkerOop(const TestPrintJobWorkerOop&) = delete;
  TestPrintJobWorkerOop& operator=(const TestPrintJobWorkerOop&) = delete;
  ~TestPrintJobWorkerOop() override = default;

 private:
  void OnDidUseDefaultSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings) override {
    DVLOG(1) << "Observed: use default settings";
    mojom::ResultCode result = print_settings->is_result_code()
                                   ? print_settings->get_result_code()
                                   : mojom::ResultCode::kSuccess;
    callbacks_->error_check_callback.Run(result);
    PrintJobWorkerOop::OnDidUseDefaultSettings(std::move(callback),
                                               std::move(print_settings));
    callbacks_->did_use_default_settings_callback.Run(result);
  }

#if BUILDFLAG(IS_WIN)
  void OnDidAskUserForSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings) override {
    DVLOG(1) << "Observed: ask user for settings";
    mojom::ResultCode result = print_settings->is_result_code()
                                   ? print_settings->get_result_code()
                                   : mojom::ResultCode::kSuccess;
    callbacks_->error_check_callback.Run(result);
    PrintJobWorkerOop::OnDidAskUserForSettings(std::move(callback),
                                               std::move(print_settings));
    callbacks_->did_ask_user_for_settings_callback.Run(result);
  }
#endif  // BUILDFLAG(IS_WIN)

  void OnDidStartPrinting(mojom::ResultCode result) override {
    DVLOG(1) << "Observed: start printing of document";
    callbacks_->error_check_callback.Run(result);
    PrintJobWorkerOop::OnDidStartPrinting(result);
    callbacks_->did_start_printing_callback.Run(result, print_job());
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

  void OnDidCancel(scoped_refptr<PrintJob> job) override {
    DVLOG(1) << "Observed: cancel";
    // Must not use `std::move(job)`, as that could potentially cause the `job`
    // (and consequentially `this`) to be destroyed before
    // `did_cancel_callback` is run.
    PrintJobWorkerOop::OnDidCancel(job);
    callbacks_->did_cancel_callback.Run();
  }

  raw_ptr<PrintCallbacks> callbacks_;
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
#if BUILDFLAG(IS_WIN)
      test_print_job_worker_oop_callbacks_.did_ask_user_for_settings_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnDidAskUserForSettings,
              base::Unretained(this));
#endif
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
      test_print_job_worker_callbacks_.did_use_default_settings_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnUseDefaultSettings,
              base::Unretained(this));
      test_print_job_worker_callbacks_.did_get_settings_with_ui_callback =
          base::BindRepeating(
              &SystemAccessProcessPrintBrowserTestBase::OnGetSettingsWithUI,
              base::Unretained(this));
    }
    test_create_print_job_worker_callback_ = base::BindRepeating(
        &SystemAccessProcessPrintBrowserTestBase::CreatePrintJobWorker,
        base::Unretained(this), UseService());
    PrinterQuery::SetCreatePrintJobWorkerCallbackForTest(
        &test_create_print_job_worker_callback_);
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
    PrinterQuery::SetCreatePrintJobWorkerCallbackForTest(/*callback=*/nullptr);
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

  // PrintViewManagerBase::Observer:
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

  void SetUpPrintViewManager(content::WebContents* web_contents) {
    auto manager = std::make_unique<TestPrintViewManager>(
        web_contents,
        base::BindRepeating(
            &SystemAccessProcessPrintBrowserTestBase::OnCreatedPrintJob,
            base::Unretained(this)));
    manager->AddObserver(*this);
    web_contents->SetUserData(PrintViewManager::UserDataKey(),
                              std::move(manager));
  }

  void PrintAfterPreviewIsReadyAndLoaded() {
    // First invoke the Print Preview dialog with `StartPrint()`.
    PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
    StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
               /*print_renderer=*/mojo::NullAssociatedRemote(),
               /*print_preview_disabled=*/false,
               /*has_selection=*/false);
    print_preview_observer.WaitUntilPreviewIsReady();

    set_rendered_page_count(print_preview_observer.rendered_page_count());

    content::WebContents* preview_dialog =
        print_preview_observer.GetPrintPreviewDialog();
    ASSERT_TRUE(preview_dialog);

    // Print Preview is completely ready, can now initiate printing.
    // This script locates and clicks the Print button.
    const char kScript[] = R"(
      const button = document.getElementsByTagName('print-preview-app')[0]
                       .$['sidebar']
                       .shadowRoot.querySelector('print-preview-button-strip')
                       .shadowRoot.querySelector('.action-button');
      button.click();)";
    ASSERT_TRUE(content::ExecuteScript(preview_dialog, kScript));
    WaitUntilCallbackReceived();
  }

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
  std::unique_ptr<PrintJobWorker> CreatePrintJobWorker(
      bool use_service,
      content::GlobalRenderFrameHostId rfh_id) {
    if (use_service) {
      return std::make_unique<TestPrintJobWorkerOop>(
          rfh_id, simulate_spooling_memory_errors_,
          &test_print_job_worker_oop_callbacks_);
    }
    return std::make_unique<TestPrintJobWorker>(
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
    if (result == mojom::ResultCode::kAccessDenied)
      ResetForNoAccessDeniedErrors();
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

  void OnDidStartPrinting(mojom::ResultCode result, PrintJob* print_job) {
    start_printing_result_ = result;
    print_job_ = print_job;
    CheckForQuit();
  }

#if BUILDFLAG(IS_WIN)
  void OnDidRenderPrintedPage(uint32_t page_number, mojom::ResultCode result) {
    render_printed_page_result_ = result;
    if (result == mojom::ResultCode::kSuccess)
      render_printed_pages_count_++;
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
    if (!reset_errors_after_check_)
      return;

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
  CreatePrintJobWorkerCallback test_create_print_job_worker_callback_;
  bool did_use_default_settings_ = false;
  bool did_get_settings_with_ui_ = false;
  bool print_backend_service_use_detected_ = false;
  bool simulate_spooling_memory_errors_ = false;
  mojo::Remote<mojom::PrintBackendService> test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
  raw_ptr<PrintJob, DanglingUntriaged> print_job_ = nullptr;
  bool reset_errors_after_check_ = true;
  int did_print_document_count_ = 0;
  mojom::ResultCode use_default_settings_result_ = mojom::ResultCode::kFailed;
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  mojom::ResultCode ask_user_for_settings_result_ = mojom::ResultCode::kFailed;
#endif
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

class SystemAccessProcessSandboxedServicePrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase {
 public:
  SystemAccessProcessSandboxedServicePrintBrowserTest() = default;
  ~SystemAccessProcessSandboxedServicePrintBrowserTest() override = default;

  bool UseService() override { return true; }
  bool SandboxService() override { return true; }
};

#if BUILDFLAG(ENABLE_OOP_PRINTING)

class SystemAccessProcessServicePrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SystemAccessProcessServicePrintBrowserTest() = default;
  ~SystemAccessProcessServicePrintBrowserTest() override = default;

  bool UseService() override { return true; }
  bool SandboxService() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SystemAccessProcessServicePrintBrowserTest,
                         testing::Bool());

#endif

class SystemAccessProcessInBrowserPrintBrowserTest
    : public SystemAccessProcessPrintBrowserTestBase {
 public:
  SystemAccessProcessInBrowserPrintBrowserTest() = default;
  ~SystemAccessProcessInBrowserPrintBrowserTest() override = default;

  bool UseService() override { return false; }
  bool SandboxService() override { return false; }
};

enum class PrintBackendFeatureVariation {
  // `PrintBackend` calls occur from browser process.
  kInBrowserProcess,
  // Use OOP `PrintBackend`.  Attempt to have `PrintBackendService` be
  // sandboxed.
  kOopSandboxedService,
  // Use OOP `PrintBackend`.  Always use `PrintBackendService` unsandboxed.
  kOopUnsandboxedService,
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

  EXPECT_EQ(rendered_page_count(), 3u);

  ASSERT_TRUE(print_view_manager.snooped_settings());
  EXPECT_EQ(print_view_manager.snooped_settings()->copies(),
            kTestPrintSettingsCopies);
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
  // Collect just the keys to compare the info options vs. advanced settings.
  std::vector<std::string> advanced_setting_keys;
  std::vector<std::string> print_info_options_keys;
  const PrintSettings::AdvancedSettings& advanced_settings =
      print_view_manager.snooped_settings()->advanced_settings();
  for (const auto& advanced_setting : advanced_settings) {
    advanced_setting_keys.push_back(advanced_setting.first);
  }
  for (const auto& option : kTestDummyPrintInfoOptions) {
    print_info_options_keys.push_back(option.first);
  }
  EXPECT_THAT(advanced_setting_keys,
              testing::UnorderedElementsAreArray(print_info_options_keys));
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

IN_PROC_BROWSER_TEST_P(SystemAccessProcessServicePrintBrowserTest,
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
  // 1.  A print job is started.
  // 2.  Rendering for 1 page of document of content.
  // 3.  Completes with document done.
  // 4.  Wait for the one print job to be destroyed, to ensure printing
  //    finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/4);
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessServicePrintBrowserTest,
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
  // 1.  A print job is started.
  // 2.  First page is rendered.
  // 3.  Second page is rendered.
  // 4.  Third page is rendered.
  // 5.  Completes with document done.
  // 6.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  // TODO(crbug.com/1008222)  Include Windows coverage of
  // RenderPrintedDocument() once XPS print pipeline is added.
  SetNumExpectedMessages(/*num=*/6);
#else
  // The expected events for this are:
  // 1.  A print job is started.
  // 2.  Document is rendered.
  // 3.  Completes with document done.
  // 4.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/4);
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
  // 1.  A print job is started.
  // 2.  Spooling to send the render data will fail.  An error dialog is shown.
  // 3.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 4.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/4);

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
    // 1.  A print job is started, but that fails.
    // 2.  An error dialog is shown.
    // 3.  The print job is canceled.  The callback from the service could occur
    //     after the print job has been destroyed.
    // 4.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/4);
  }

  PrintAfterPreviewIsReadyAndLoaded();

  EXPECT_EQ(start_printing_result(), mojom::ResultCode::kFailed);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  // No tracking of cancel for in-browser tests, only for OOP.
  if (GetParam() != PrintBackendFeatureVariation::kInBrowserProcess)
    EXPECT_EQ(cancel_count(), 1);
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
  // 1.  A print job is started, but has an access-denied error.
  // 2.  A retry to start the print job with adjusted access will succeed.
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
  // 1.  A print job is started, but has an access-denied error.
  // 2.  A retry to start the print job with adjusted access will still fail.
  // 3.  An error dialog is shown.
  // 4.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 5.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/5);

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
  // 1.  A print job is started.
  // 2.  Rendering for 1 page of document of content fails with access denied.
  // 3.  An error dialog is shown.
  // 4.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 5.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/5);

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
  // 1.  Start the print job.
  // 2.  First page render callback shows success.
  // 3.  Second page render callback shows failure.  Will start failure
  //     processing to cancel the print job.
  // 4.  A printing error dialog is displayed.
  // 5.  Third page render callback will show it was canceled (due to prior
  //     failure).  This is disregarded by the browser, since the job has
  //     already been canceled.
  // 6.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 7.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/7);

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
  // 1.  A print job is started.
  // 2.  Rendering for 1 page of document of content fails with access denied.
  // 3.  An error dialog is shown.
  // 4.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
  // 5.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/5);

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
  // 1.  A print job is started.
  // 2.  Rendering for 1 page of document of content.
  // 3.  Document done results in an access-denied error.
  // 4.  An error dialog is shown.
  // 5.  The print job is canceled.  The callback from the service could occur
  //     after the print job has been destroyed.
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
  EXPECT_EQ(document_done_result(), mojom::ResultCode::kAccessDenied);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(cancel_count(), 1);
  EXPECT_EQ(print_job_destruction_count(), 1);
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
IN_PROC_BROWSER_TEST_P(SystemAccessProcessServicePrintBrowserTest,
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

#if BUILDFLAG(IS_WIN)
  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings.
  // 3.  A print job is started.
  // 4.  The print compositor will complete generating the document.
  // 5.  The document is rendered.
  // 6.  Receive document done notification.
  // 8.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  SetNumExpectedMessages(/*num=*/7);
#else
  // The expected events for this are:
  // 1.  Get the default settings.  Ask the user for settings; due to issues
  //     with displaying a system dialog from the utility process, there is no
  //     callback to capture the request for user supplied settings.
  // 2.  A print job is started.
  // 3.  The print compositor will complete generating the document.
  // 4.  The document is rendered.
  // 5.  Receive document done notification.
  // 6.  Wait for the one print job to be destroyed, to ensure printing
  //     finished cleanly before completing the test.
  // TODO(crbug.com/1374188)  Update this expectation once
  // `AskUserForSettings()` is able to be pushed OOP for Linux.
  SetNumExpectedMessages(/*num=*/6);
#endif

  StartBasicPrint(web_contents);

  WaitUntilCallbackReceived();

  EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kSuccess);
  // macOS and Linux currently have to invoke a system dialog from within the
  // browser process.  There is not a callback to capture the result in these
  // cases.
  // TODO(crbug.com/1374188)  Re-enable this check against
  // `ask_user_for_settings_result()` once `AskForUserSettings()` is able to be
  // pushed OOP for Linux.
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(ask_user_for_settings_result(), mojom::ResultCode::kSuccess);
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

  // The expected events for this are:
  // 1.  Get the default settings.
  // 2.  Ask the user for settings, which indicates to cancel the print
  //     request.  No further printing calls are made.
  // No print job is created because of such an early cancel.
  SetNumExpectedMessages(/*num=*/2);

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
#if BUILDFLAG(IS_WIN)
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
    // 1.  Gets default settings.
    // 2.  Asks user for settings.  This is invoked from the browser process,
    //     so there is no override to observe this.  Then a print job is
    //     started, which fails.
    // 3.  An error dialog is shown.
    // 4.  The print job is canceled.  The callback from the service could occur
    //     after the print job has been destroyed.
    // 5.  Wait for the one print job to be destroyed, to ensure printing
    //     finished cleanly before completing the test.
    // 6.  The print compositor will have started to generate the document.
    //     Wait until that is known to have completed, to ensure printing
    //     finished cleanly before completing the test.
    SetNumExpectedMessages(/*num=*/6);
#endif  // BUILDFLAG(IS_WIN)
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

IN_PROC_BROWSER_TEST_P(SystemAccessProcessServicePrintBrowserTest,
                       StartBasicPrintConcurrent) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test3.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  TestPrintViewManager* print_view_manager =
      TestPrintViewManager::CreateForWebContents(web_contents);

  // Pretend that a window has started a system print.
  absl::optional<uint32_t> client_id =
      PrintBackendServiceManager::GetInstance().RegisterQueryWithUiClient();
  ASSERT_TRUE(client_id.has_value());

  // Now initiate a system print that would exist concurrently with that.
  StartBasicPrint(web_contents);

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

  EXPECT_EQ(use_default_settings_result(), mojom::ResultCode::kFailed);
  EXPECT_EQ(error_dialog_shown_count(), 1u);
  EXPECT_EQ(did_print_document_count(), 0);
  EXPECT_EQ(print_job_construction_count(), 0);
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

#endif  //  BUILDFLAG(ENABLE_OOP_PRINTING)

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
struct ContentAnalysisTestCase {
  bool content_analysis_allows_print = false;
  bool oop_enabled = false;
};

class ContentAnalysisPrintBrowserTest
    : public PrintBrowserTest,
      public testing::WithParamInterface<ContentAnalysisTestCase> {
 public:
  ContentAnalysisPrintBrowserTest() {
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting(kFakeDmToken));
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
    if (oop_enabled()) {
      feature_list_.InitWithFeaturesAndParameters(
          {
              {features::kEnableOopPrintDrivers,
               {{features::kEnableOopPrintDriversJobPrint.name, "true"}}},
              {features::kEnablePrintContentAnalysis, {}},
          },
          {});
    } else {
      feature_list_.InitAndEnableFeature(features::kEnablePrintContentAnalysis);
    }

    test_printing_context_factory()->SetPrinterNameForSubsequentContexts(
        "printer_name");
    PrintBrowserTest::SetUp();
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
    PrintBrowserTest::SetUpOnMainThread();
  }

  bool content_analysis_allows_print() const {
    return GetParam().content_analysis_allows_print;
  }
  bool oop_enabled() { return GetParam().oop_enabled; }

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

 private:
  base::test::ScopedFeatureList feature_list_;
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
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1396386): Remove this when tests are fixed.
  if (oop_enabled())
    return;
#endif

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

  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
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
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false,
             /*has_selection=*/false);

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

  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false,
             /*has_selection=*/false);

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
    // TODO(crbug.com/1396386): Add back oop_enabled=true values when tests are
    // fixed.
    testing::Values(
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/true,
                                /*oop_enabled=*/false},
        ContentAnalysisTestCase{/*content_analysis_allows_print=*/false,
                                /*oop_enabled=*/false}));
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

#endif  // BUILDFLAG(ENABLE_PRINT_SCANNING)

}  // namespace printing
