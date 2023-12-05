// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

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
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/printing/test_print_preview_observer.h"
#include "chrome/browser/printing/test_print_view_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/printing_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "ui/base/ui_base_types.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#endif

namespace {

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

class PDFExtensionPrintingTest : public PDFExtensionTestBase,
                                 public printing::PrintJob::Observer,
                                 public testing::WithParamInterface<bool> {
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
    PDFExtensionTestBase::SetUp();
  }
  void SetUpOnMainThread() override {
    // Avoid getting blocked by modal print error dialogs. Must be called after
    // the UI thread is up and running.
    SetShowPrintErrorDialogForTest(base::DoNothing());
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
  void TearDownOnMainThread() override {
    PDFExtensionTestBase::TearDownOnMainThread();
    SetShowPrintErrorDialogForTest(base::NullCallback());
  }
  void TearDown() override {
    PDFExtensionTestBase::TearDown();
    printing::PrintingContext::SetPrintingContextFactoryForTest(nullptr);
    printing::PrintBackend::SetPrintBackendForTesting(nullptr);
  }
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (UseService()) {
      return {printing::features::kEnableOopPrintDrivers};
    }
#endif
    return {};
  }
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (!UseService()) {
      return {printing::features::kEnableOopPrintDrivers};
    }
#endif
    return {};
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

 private:
  bool UseService() const { return GetParam(); }

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

  scoped_refptr<printing::TestPrintBackend> test_print_backend_ =
      base::MakeRefCounted<printing::TestPrintBackend>();
  printing::BrowserPrintingContextFactoryForTest test_printing_context_factory_;
  bool observing_print_job_ = false;
  bool print_job_destroyed_ = false;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  base::WeakPtrFactory<PDFExtensionPrintingTest> weak_factory_{this};
};

IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest, BasicPrintCommand) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest);
  ASSERT_TRUE(frame);

  SetupPrintViewManagerForJobMonitoring(frame);
  chrome::BasicPrint(browser());
  WaitForPrintJobDestruction();
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest, PrintCommand) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest);
  ASSERT_TRUE(frame);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  chrome::Print(browser());
  print_observer.WaitUntilPreviewIsReady();
}

// TODO(crbug.com/1488085): Test is flaky.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_ContextMenuPrintCommandExtensionMainFrame \
  DISABLED_ContextMenuPrintCommandExtensionMainFrame
#else
#define MAYBE_ContextMenuPrintCommandExtensionMainFrame \
  ContextMenuPrintCommandExtensionMainFrame
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest,
                       MAYBE_ContextMenuPrintCommandExtensionMainFrame) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  ASSERT_TRUE(plugin_frame);

  content::RenderFrameHost* guest_main_frame = guest->GetGuestMainFrame();
  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(guest_main_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  guest_main_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitUntilPreviewIsReady();
  menu_interceptor.Wait();
}

// TODO(crbug.com/1344508): Test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(
    PDFExtensionPrintingTest,
    DISABLED_ContextMenuPrintCommandEmbeddedExtensionMainFrame) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  ASSERT_TRUE(plugin_frame);

  content::RenderFrameHost* guest_main_frame = guest->GetGuestMainFrame();
  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(guest_main_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  SimulateMouseClickAt(guest, blink::WebInputEvent::kNoModifiers,
                       blink::WebMouseEvent::Button::kLeft, {1, 1});
  guest_main_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitUntilPreviewIsReady();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest,
                       ContextMenuPrintCommandPluginFrame) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  ASSERT_TRUE(plugin_frame);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  SetInputFocusOnPlugin(guest);
  plugin_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitUntilPreviewIsReady();
  menu_interceptor.Wait();
}

// TODO(crbug.com/1330032): Fix flakiness.
IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest,
                       DISABLED_ContextMenuPrintCommandEmbeddedPluginFrame) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  ASSERT_TRUE(plugin_frame);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  SetInputFocusOnPlugin(guest);
  plugin_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitUntilPreviewIsReady();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_P(PDFExtensionPrintingTest, PrintButton) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest);
  ASSERT_TRUE(frame);

  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  constexpr char kClickPrintButtonScript[] = R"(
    viewer.shadowRoot.querySelector('#toolbar')
        .shadowRoot.querySelector('#print')
        .click();
  )";
  EXPECT_TRUE(ExecJs(guest->GetGuestMainFrame(), kClickPrintButtonScript));
  print_observer.WaitUntilPreviewIsReady();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionPrintingTest,
#if BUILDFLAG(ENABLE_OOP_PRINTING)
                         testing::Bool()
#else
                         testing::Values(false)
#endif
);

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

// TODO(https://crbug.com/1488085): Test is flaky.
// Note that MAYBE_ContextMenuPrintCommandExtensionMainFrame is already
// defined above.
IN_PROC_BROWSER_TEST_P(PDFExtensionBasicPrintingTest,
                       MAYBE_ContextMenuPrintCommandExtensionMainFrame) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  ASSERT_TRUE(plugin_frame);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  SetupPrintViewManagerForJobMonitoring(plugin_frame);
  SetInputFocusOnPlugin(guest);
  plugin_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  menu_interceptor.Wait();
  WaitForPrintJobDestruction();
}

INSTANTIATE_TEST_SUITE_P(All, PDFExtensionBasicPrintingTest, testing::Bool());
