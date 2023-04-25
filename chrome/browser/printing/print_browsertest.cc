// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_browsertest.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/printing/browser_printing_context_factory_for_test.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/printing/test_print_preview_observer.h"
#include "chrome/browser/printing/test_print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
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
#include "chrome/browser/printing/printer_query_oop.h"
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

namespace {

constexpr int kTestPrinterCapabilitiesMaxCopies = 99;
const int kDefaultDocumentCookie = PrintSettings::NewCookie();

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
constexpr char kFakeDmToken[] = "fake-dm-token";
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

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

class PrintPreviewDoneObserver
    : public mojom::PrintRenderFrameInterceptorForTesting {
 public:
  PrintPreviewDoneObserver(content::RenderFrameHost* render_frame_host,
                           mojom::PrintRenderFrame* print_render_frame)
      : render_frame_host_(render_frame_host),
        print_render_frame_(print_render_frame) {
    OverrideBinderForTesting();
  }
  ~PrintPreviewDoneObserver() override {
    render_frame_host_->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(mojom::PrintRenderFrame::Name_,
                                   base::NullCallback());
  }

  void WaitForPrintPreviewDialogClosed() { run_loop_.Run(); }

  // mojom::PrintRenderFrameInterceptorForTesting:
  mojom::PrintRenderFrame* GetForwardingInterface() override {
    return print_render_frame_;
  }
  void OnPrintPreviewDialogClosed() override {
    GetForwardingInterface()->OnPrintPreviewDialogClosed();
    run_loop_.Quit();
  }

 private:
  void OverrideBinderForTesting() {
    // Safe to use base::Unretained() below because:
    // 1) Normally, Bind() will unregister the override after it gets called.
    // 2) If Bind() does not get called, the dtor will unregister the override.
    render_frame_host_->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PrintRenderFrame::Name_,
            base::BindRepeating(&PrintPreviewDoneObserver::Bind,
                                base::Unretained(this)));
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame>(
        std::move(handle)));

    // After the initial binding, reset the binder override. Otherwise,
    // PrintPreviewDoneObserver will also try to bind the remote passed by
    // SetPrintPreviewUI(), which will fail since `receiver_` is already bound.
    render_frame_host_->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(mojom::PrintRenderFrame::Name_,
                                   base::NullCallback());
  }

  raw_ptr<content::RenderFrameHost> const render_frame_host_;
  raw_ptr<mojom::PrintRenderFrame> const print_render_frame_;
  mojo::AssociatedReceiver<mojom::PrintRenderFrame> receiver_{this};
  base::RunLoop run_loop_;
};

}  // namespace

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
    if (!client) {
      return;
    }

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
  raw_ptr<content::RenderFrameHost> frame_host_;
  raw_ptr<content::WebContents> web_contents_;
  const int document_cookie_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::RepeatingClosure msg_callback_;
  mojo::AssociatedReceiver<mojom::PrintRenderFrame> receiver_{this};
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

PrintBrowserTest::PrintBrowserTest() = default;
PrintBrowserTest::~PrintBrowserTest() = default;

void PrintBrowserTest::SetUp() {
  test_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
  PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());
  PrintingContext::SetPrintingContextFactoryForTest(
      &test_printing_context_factory_);

  num_expected_messages_ = 1;  // By default, only wait on one message.
  num_received_messages_ = 0;
  InProcessBrowserTest::SetUp();
}

void PrintBrowserTest::SetUpOnMainThread() {
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

void PrintBrowserTest::TearDownOnMainThread() {
  // Remove map of objects pointing to //content objects before they go away.
  frame_content_.clear();

  SetShowPrintErrorDialogForTest(base::NullCallback());
  InProcessBrowserTest::TearDownOnMainThread();
}

void PrintBrowserTest::TearDown() {
  InProcessBrowserTest::TearDown();
  PrintingContext::SetPrintingContextFactoryForTest(/*factory=*/nullptr);
  PrintBackend::SetPrintBackendForTesting(/*print_backend=*/nullptr);
}

void PrintBrowserTest::AddPrinter(const std::string& printer_name) {
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
  default_caps->papers.push_back(kTestPaperLetter);
  default_caps->papers.push_back(kTestPaperLegal);
  test_print_backend_->AddValidPrinter(
      printer_name, std::move(default_caps),
      std::make_unique<PrinterBasicInfo>(printer_info));
}

void PrintBrowserTest::SetPrinterNameForSubsequentContexts(
    const std::string& printer_name) {
  test_printing_context_factory_.SetPrinterNameForSubsequentContexts(
      printer_name);
}

void PrintBrowserTest::PrintAndWaitUntilPreviewIsReady() {
  const PrintParams kParams;
  PrintAndWaitUntilPreviewIsReady(kParams);
}

void PrintBrowserTest::PrintAndWaitUntilPreviewIsReady(
    const PrintParams& params) {
  TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false,
                                                  params.pages_per_sheet);

  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false, params.print_only_selection);

  print_preview_observer.WaitUntilPreviewIsReady();

  set_rendered_page_count(print_preview_observer.rendered_page_count());
}

void PrintBrowserTest::PrintAndWaitUntilPreviewIsReadyAndLoaded() {
  const PrintParams kParams;
  PrintAndWaitUntilPreviewIsReadyAndLoaded(kParams);
}

void PrintBrowserTest::PrintAndWaitUntilPreviewIsReadyAndLoaded(
    const PrintParams& params) {
  TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true,
                                                  params.pages_per_sheet);

  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false, params.print_only_selection);

  print_preview_observer.WaitUntilPreviewIsReady();

  set_rendered_page_count(print_preview_observer.rendered_page_count());
}

  // The following are helper functions for having a wait loop in the test and
  // exit when all expected messages are received.
void PrintBrowserTest::SetNumExpectedMessages(unsigned int num) {
  num_expected_messages_ = num;
}

void PrintBrowserTest::ResetNumReceivedMessages() {
  num_received_messages_ = 0;
}

void PrintBrowserTest::WaitUntilCallbackReceived() {
  base::RunLoop run_loop;
  quit_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void PrintBrowserTest::CheckForQuit() {
  if (++num_received_messages_ != num_expected_messages_) {
    return;
  }
  if (quit_callback_) {
    std::move(quit_callback_).Run();
  }
}

void PrintBrowserTest::CreateTestPrintRenderFrame(
    content::RenderFrameHost* frame_host,
    content::WebContents* web_contents) {
  frame_content_.emplace(
      frame_host, std::make_unique<TestPrintRenderFrame>(
                      frame_host, web_contents, kDefaultDocumentCookie,
                      base::BindRepeating(&PrintBrowserTest::CheckForQuit,
                                          base::Unretained(this))));
  OverrideBinderForTesting(frame_host);
}

// static
mojom::PrintFrameContentParamsPtr
PrintBrowserTest::GetDefaultPrintFrameParams() {
  return mojom::PrintFrameContentParams::New(gfx::Rect(800, 600),
                                             kDefaultDocumentCookie);
}

const mojo::AssociatedRemote<mojom::PrintRenderFrame>&
PrintBrowserTest::GetPrintRenderFrame(content::RenderFrameHost* rfh) {
  if (!remote_) {
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }
  return remote_;
}

TestPrintRenderFrame* PrintBrowserTest::GetFrameContent(
    content::RenderFrameHost* host) const {
  auto iter = frame_content_.find(host);
  return iter != frame_content_.end() ? iter->second.get() : nullptr;
}

void PrintBrowserTest::OverrideBinderForTesting(
    content::RenderFrameHost* render_frame_host) {
  render_frame_host->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
      mojom::PrintRenderFrame::Name_,
      base::BindRepeating(
          &TestPrintRenderFrame::Bind,
          base::Unretained(GetFrameContent(render_frame_host))));
}

void PrintBrowserTest::ShowPrintErrorDialog() {
  ++error_dialog_shown_count_;
  CheckForQuit();
}

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
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());

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
    TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);

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
  TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);
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
// TODO(crbug.com/1371776): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, DISABLED_PrintNup) {
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
// TODO(crbug.com/1371776): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest, DISABLED_PrintNup) {
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

  TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false, /*has_selection=*/false);
  content::WebContents* preview_dialog =
      print_preview_observer.WaitUntilPreviewIsReadyAndReturnPreviewDialog();
  ASSERT_TRUE(preview_dialog);

  // The script will ensure we return the id of <zoom-out-button> when
  // focused. Focus the element after PDF plugin in tab order.
  const char kScript[] = R"(
    new Promise(resolve => {
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
        resolve(true);
      });
      select_tag.focus();
    });
    )";
  ASSERT_EQ(true, content::EvalJs(preview_dialog, kScript));

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

  TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);
  content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(),
                              "window.print();");
  print_preview_observer.WaitUntilPreviewIsReady();
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PdfWindowDotPrint) {
  // Do not add any other printers; will default to "Save as PDF".

  // Load test page and check the initial state.
  const GURL kUrl(embedded_test_server()->GetURL("/printing/hello_world.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
  content::ExecuteScriptAsync(web_contents->GetPrimaryMainFrame(),
                              "window.print();");
  print_preview_observer.WaitUntilPreviewIsReady();
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest,
                       WindowDotPrintTriggersBeforeAfterEvents) {
  // Load test page and check the initial state.
  const GURL kUrl(
      embedded_test_server()->GetURL("/printing/on_before_after_events.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  EXPECT_EQ(false, content::EvalJs(rfh, "firedBeforePrint"));
  EXPECT_EQ(false, content::EvalJs(rfh, "firedAfterPrint"));

  // Set up the PrintPreviewDoneObserver early, as it needs to intercept Mojo
  // IPCs before they start.
  PrintPreviewDoneObserver done_observer(rfh, GetPrintRenderFrame(rfh).get());

  // Load Print Preview and make sure the beforeprint event fired.
  TestPrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);
  content::ExecuteScriptAsync(rfh, "window.print();");
  print_preview_observer.WaitUntilPreviewIsReady();
  EXPECT_EQ(true, content::EvalJs(rfh, "firedBeforePrint"));
  EXPECT_EQ(false, content::EvalJs(rfh, "firedAfterPrint"));

  // Close the Print Preview dialog and make sure the afterprint event fired.
  auto* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  ASSERT_TRUE(web_contents_modal_dialog_manager);
  web_contents_modal_dialog_manager->CloseAllDialogs();
  done_observer.WaitForPrintPreviewDialogClosed();
  EXPECT_EQ(true, content::EvalJs(rfh, "firedBeforePrint"));
  EXPECT_EQ(true, content::EvalJs(rfh, "firedAfterPrint"));
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
    EXPECT_TRUE(navigation.WaitForNavigationFinished());

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
