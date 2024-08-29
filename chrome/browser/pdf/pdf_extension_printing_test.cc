// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/printing/browser_printing_context_factory_for_test.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/printing/test_print_preview_observer.h"
#include "chrome/browser/printing/test_print_view_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/context_menu_interceptor.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/pdf_features.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "ui/base/ui_base_types.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/test/gmock_callback_support.h"
#include "chrome/test/chromeos/printing/mock_local_printer_chromeos.h"
#include "chromeos/lacros/lacros_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
using testing::_;
using testing::AtMost;
using testing::NiceMock;
#endif

struct PDFExtensionPrintingTestPassToString {
  std::string operator()(
      const ::testing::TestParamInfo<std::tuple<bool, bool>>& i) const {
    return std::string(std::get<1>(i.param) ? "OOPIF_" : "GUESTVIEW_") +
           std::string(std::get<0>(i.param) ? "SERVICE" : "BROWSER");
  }
};

#if BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<KeyedService> BuildTestCupsPrintJobManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::TestCupsPrintJobManager>(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService> BuildFakeCupsPrintersManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::FakeCupsPrintersManager>();
}

void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
  ash::CupsPrintJobManagerFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildTestCupsPrintJobManager));
  ash::CupsPrintersManagerFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildFakeCupsPrintersManager));
}

#endif

}  // namespace

using ::content::WebContents;
using ::extensions::MimeHandlerViewGuest;
using ::pdf_extension_test_util::SetInputFocusOnPlugin;

class PDFExtensionPrintingTest
    : public PDFExtensionTestBase,
      public printing::PrintJob::Observer,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  PDFExtensionPrintingTest() = default;
  ~PDFExtensionPrintingTest() override = default;

  // PDFExtensionTestBase:
  void SetUp() override {
    // Avoid using a real PrintBackend / PrintingContext, as they can show modal
    // print dialogs.
    // Called here in SetUp() because it must be reset in TearDown(), as
    // resetting in TearDownOnMainThread() is too early. The MessagePump may
    // still process messages after TearDownOnMainThread(), which can trigger
    // PrintingContext calls.
    printing::PrintBackend::SetPrintBackendForTesting(
        test_print_backend_.get());
    printing::PrintingContext::SetPrintingContextFactoryForTest(
        &test_printing_context_factory_);

    // Tests assume that printing can be done to the print backend, so there
    // must be at least one printer available for that to make sense.
    constexpr char kTestPrinter[] = "printer1";
    AddPrinter(kTestPrinter);
    test_printing_context_factory_.SetPrinterNameForSubsequentContexts(
        kTestPrinter);

    PDFExtensionTestBase::SetUp();
  }
  void SetUpOnMainThread() override {
    // Avoid getting blocked by modal print error dialogs. Must be called after
    // the UI thread is up and running.
    SetShowPrintErrorDialogForTest(base::DoNothing());
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (UseService()) {
      print_backend_service_ =
          printing::PrintBackendServiceTestImpl::LaunchForTesting(
              test_remote_, test_print_backend_.get(), /*sandboxed=*/true);
    }
#endif
    PDFExtensionTestBase::SetUpOnMainThread();
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&OnWillCreateBrowserContextServices));
  }
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    PDFExtensionTestBase::CreatedBrowserMainParts(browser_main_parts);
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        local_printer_receiver_.BindNewPipeAndPassRemote());

    EXPECT_CALL(local_printer(), AddPrintServerObserver(_, _))
        .Times(AtMost(1))
        .WillOnce(base::test::RunOnceCallback<1>());
    EXPECT_CALL(local_printer(), GetPolicies(_))
        .Times(AtMost(1))
        .WillOnce(
            base::test::RunOnceCallback<0>(crosapi::mojom::Policies::New()));
    EXPECT_CALL(local_printer(), GetEulaUrl(_, _))
        .Times(AtMost(1))
        .WillOnce(base::test::RunOnceCallback<1>(GURL()));
  }
#endif
  void TearDownOnMainThread() override {
    PDFExtensionTestBase::TearDownOnMainThread();
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    printing::PrintBackendServiceManager::ResetForTesting();
#endif
    SetShowPrintErrorDialogForTest(base::NullCallback());
  }
  void TearDown() override {
    PDFExtensionTestBase::TearDown();
    printing::PrintingContext::SetPrintingContextFactoryForTest(nullptr);
    printing::PrintBackend::SetPrintBackendForTesting(nullptr);
  }
  bool UseOopif() const override { return std::get<1>(GetParam()); }
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        PDFExtensionTestBase::GetEnabledFeatures();
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (UseService()) {
      enabled.push_back({printing::features::kEnableOopPrintDrivers, {}});
    }
#endif
    return enabled;
  }
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    std::vector<base::test::FeatureRef> disabled =
        PDFExtensionTestBase::GetDisabledFeatures();
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (!UseService()) {
      disabled.push_back(printing::features::kEnableOopPrintDrivers);
    }
#endif
    return disabled;
  }

  void AddPrinter(const std::string& printer_name) {
    printing::PrinterBasicInfo printer_info(
        printer_name,
        /*display_name=*/"test printer",
        /*printer_description=*/"A printer for testing.",
        /*printer_status=*/0,
        /*is_default=*/true, printing::test::kPrintInfoOptions);

    auto default_caps =
        std::make_unique<printing::PrinterSemanticCapsAndDefaults>();
    default_caps->copies_max = 1;
    default_caps->dpis = printing::test::kPrinterCapabilitiesDefaultDpis;
    default_caps->default_dpi = printing::test::kPrinterCapabilitiesDpi;
    default_caps->papers.push_back(printing::test::kPaperLetter);
    default_caps->papers.push_back(printing::test::kPaperLegal);
    test_print_backend_->AddValidPrinter(
        printer_name, std::move(default_caps),
        std::make_unique<printing::PrinterBasicInfo>(printer_info));
  }

  void SetupPrintViewManagerForJobMonitoring(content::RenderFrameHost* frame) {
    auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
    auto manager = std::make_unique<printing::TestPrintViewManager>(
        web_contents,
        base::BindRepeating(&PDFExtensionPrintingTest::OnCreatedPrintJob,
                            weak_factory_.GetWeakPtr()));
    web_contents->SetUserData(printing::PrintViewManager::UserDataKey(),
                              std::move(manager));
  }

  void WaitForPrintJobDestruction() {
    if (!print_job_destroyed_) {
      base::RunLoop run_loop;
      base::AutoReset<raw_ptr<base::RunLoop>> auto_reset(&run_loop_, &run_loop);
      run_loop.Run();
      EXPECT_TRUE(print_job_destroyed_);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  NiceMock<MockLocalPrinter>& local_printer() { return local_printer_; }
#endif

 private:
  bool UseService() const { return std::get<0>(GetParam()); }

  void OnCreatedPrintJob(printing::PrintJob* print_job) {
    EXPECT_FALSE(observing_print_job_);
    print_job->AddObserver(*this);
    observing_print_job_ = true;
  }

  // PrintJob::Observer:
  void OnDestruction() override {
    EXPECT_FALSE(print_job_destroyed_);
    if (run_loop_) {
      run_loop_->Quit();
    }
    print_job_destroyed_ = true;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::CallbackListSubscription create_services_subscription_;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  NiceMock<MockLocalPrinter> local_printer_;
  mojo::Receiver<crosapi::mojom::LocalPrinter> local_printer_receiver_{
      &local_printer_};
#endif

  scoped_refptr<printing::TestPrintBackend> test_print_backend_ =
      base::MakeRefCounted<printing::TestPrintBackend>();
  printing::BrowserPrintingContextFactoryForTest test_printing_context_factory_;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  mojo::Remote<printing::mojom::PrintBackendService> test_remote_;
  std::unique_ptr<printing::PrintBackendServiceTestImpl> print_backend_service_;
#endif
  bool observing_print_job_ = false;
  bool print_job_destroyed_ = false;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  base::WeakPtrFactory<PDFExtensionPrintingTest> weak_factory_{this};
};

IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest, BasicPrintCommand) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Acknowledge print job creation so that the mojo callback doesn't hang.
  EXPECT_CALL(local_printer(), CreatePrintJob(_, _))
      .WillOnce(base::test::RunOnceCallback<1>());
#endif

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));
  content::RenderFrameHost* plugin_frame =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(GetActiveWebContents());
  ASSERT_TRUE(plugin_frame);

  SetupPrintViewManagerForJobMonitoring(plugin_frame);
  chrome::BasicPrint(browser());
  WaitForPrintJobDestruction();
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest, PrintCommand) {
  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  chrome::Print(browser());
  print_observer.WaitUntilPreviewIsReady();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest,
                       ContextMenuPrintCommandExtensionMainFrame) {
  content::RenderFrameHost* extension_host =
      LoadPdfGetExtensionHost(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(extension_host);

  content::WebContents* embedder_web_contents = GetEmbedderWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(extension_host);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  SimulateMouseClickAt(extension_host, embedder_web_contents,
                       blink::WebInputEvent::kNoModifiers,
                       blink::WebMouseEvent::Button::kRight, {1, 1});
  print_observer.WaitUntilPreviewIsReady();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest,
                       ContextMenuPrintCommandEmbeddedExtensionMainFrame) {
  content::RenderFrameHost* extension_host =
      LoadPdfInFirstChildGetExtensionHost(
          embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  ASSERT_TRUE(extension_host);

  content::WebContents* embedder_web_contents = GetEmbedderWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(extension_host);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  SimulateMouseClickAt(extension_host, embedder_web_contents,
                       blink::WebInputEvent::kNoModifiers,
                       blink::WebMouseEvent::Button::kRight, {1, 1});
  print_observer.WaitUntilPreviewIsReady();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest,
                       ContextMenuPrintCommandPluginFrame) {
  content::RenderFrameHost* extension_host =
      LoadPdfGetExtensionHost(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(extension_host);

  content::RenderFrameHost* plugin_frame =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(GetActiveWebContents());
  ASSERT_TRUE(plugin_frame);

  content::WebContents* embedder_web_contents = GetEmbedderWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  SetInputFocusOnPlugin(extension_host, embedder_web_contents);
  plugin_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitUntilPreviewIsReady();
  menu_interceptor.Wait();
}

// TODO(crbug.com/40842943): Fix flakiness.
IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest,
                       DISABLED_ContextMenuPrintCommandEmbeddedPluginFrame) {
  content::RenderFrameHost* extension_host =
      LoadPdfInFirstChildGetExtensionHost(
          embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  ASSERT_TRUE(extension_host);

  content::RenderFrameHost* plugin_frame =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(GetActiveWebContents());
  ASSERT_TRUE(plugin_frame);

  content::WebContents* embedder_web_contents = GetEmbedderWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  SetInputFocusOnPlugin(extension_host, embedder_web_contents);
  plugin_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitUntilPreviewIsReady();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest, PrintButton) {
  content::RenderFrameHost* extension_host =
      LoadPdfGetExtensionHost(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(extension_host);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  constexpr char kClickPrintButtonScript[] = R"(
    viewer.shadowRoot.querySelector('#toolbar')
        .shadowRoot.querySelector('#print')
        .click();
  )";
  EXPECT_TRUE(ExecJs(extension_host, kClickPrintButtonScript));
  print_observer.WaitUntilPreviewIsReady();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionPrintingTest,
                         testing::Combine(
#if BUILDFLAG(ENABLE_OOP_PRINTING)
                             testing::Bool(),
#else
                             testing::Values(false),
#endif
                             testing::Bool()),
                         PDFExtensionPrintingTestPassToString());

class PDFExtensionBasicPrintingTest : public PDFExtensionPrintingTest {
 public:
  PDFExtensionBasicPrintingTest() = default;
  ~PDFExtensionBasicPrintingTest() override = default;

  // PDFExtensionPrintingTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisablePrintPreview);
    PDFExtensionPrintingTest::SetUpCommandLine(command_line);
  }
};

// TODO(crbug.com/40283511): Test is flaky.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ContextMenuPrintCommandExtensionMainFrame \
  DISABLED_ContextMenuPrintCommandExtensionMainFrame
#else
#define MAYBE_ContextMenuPrintCommandExtensionMainFrame \
  ContextMenuPrintCommandExtensionMainFrame
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionBasicPrintingTest,
                       MAYBE_ContextMenuPrintCommandExtensionMainFrame) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Acknowledge print job creation so that the mojo callback doesn't hang.
  EXPECT_CALL(local_printer(), CreatePrintJob(_, _))
      .WillOnce(base::test::RunOnceCallback<1>());
#endif

  content::RenderFrameHost* extension_host =
      LoadPdfGetExtensionHost(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(extension_host);

  content::RenderFrameHost* plugin_frame =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(GetActiveWebContents());
  ASSERT_TRUE(plugin_frame);

  content::WebContents* embedder_web_contents = GetEmbedderWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  SetupPrintViewManagerForJobMonitoring(plugin_frame);
  SetInputFocusOnPlugin(extension_host, embedder_web_contents);
  SimulateMouseClickAt(plugin_frame, embedder_web_contents,
                       blink::WebInputEvent::kNoModifiers,
                       blink::WebMouseEvent::Button::kRight, {1, 1});
  menu_interceptor.Wait();
  WaitForPrintJobDestruction();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionBasicPrintingTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         PDFExtensionPrintingTestPassToString());
