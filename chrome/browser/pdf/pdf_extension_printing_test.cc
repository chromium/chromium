// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#include "ui/base/ui_base_types.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif

using ::content::WebContents;
using ::extensions::MimeHandlerViewGuest;
using ::pdf_extension_test_util::SetInputFocusOnPlugin;

namespace {

class PrintObserver : public printing::PrintViewManagerBase::Observer {
 public:
  explicit PrintObserver(content::RenderFrameHost* rfh)
      : print_view_manager_(PrintViewManagerImpl::FromWebContents(
            content::WebContents::FromRenderFrameHost(rfh))),
        rfh_(rfh) {
    print_view_manager_->AddObserver(*this);
  }

  ~PrintObserver() override { print_view_manager_->RemoveObserver(*this); }

  // printing::PrintViewManagerBase::Observer:
  void OnPrintNow(const content::RenderFrameHost* rfh) override {
    EXPECT_FALSE(print_now_called_);
    EXPECT_FALSE(print_preview_called_);
    EXPECT_EQ(rfh, rfh_);
    run_loop_.Quit();
    print_now_called_ = true;
  }
  void OnPrintPreview(const content::RenderFrameHost* rfh) override {
    EXPECT_FALSE(print_preview_called_);
    EXPECT_FALSE(print_now_called_);
    EXPECT_EQ(rfh, rfh_);
    run_loop_.Quit();
    print_preview_called_ = true;
  }

  void WaitForPrintNow() {
    WaitIfNotAlreadyPrinted();
    EXPECT_TRUE(print_now_called_);
    EXPECT_FALSE(print_preview_called_);
  }

  void WaitForPrintPreview() {
    WaitIfNotAlreadyPrinted();
    EXPECT_TRUE(print_preview_called_);
    EXPECT_FALSE(print_now_called_);
  }

 private:
  void WaitIfNotAlreadyPrinted() {
    if (!print_now_called_ && !print_preview_called_) {
      run_loop_.Run();
    }
  }

  bool print_now_called_ = false;
  bool print_preview_called_ = false;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  using PrintViewManagerImpl = printing::PrintViewManager;
#else
  using PrintViewManagerImpl = printing::PrintViewManagerBasic;
#endif
  const raw_ptr<PrintViewManagerImpl> print_view_manager_;
  const raw_ptr<const content::RenderFrameHost, FlakyDanglingUntriaged> rfh_;
  base::RunLoop run_loop_;
};

}  // namespace

class PDFExtensionPrintingTest : public PDFExtensionTestBase {
 public:
  PDFExtensionPrintingTest() = default;
  ~PDFExtensionPrintingTest() override = default;

  // PDFExtensionTestBase:
  void SetUpOnMainThread() override {
    // Avoid getting blocked by modal print error dialogs.
    SetShowPrintErrorDialogForTest(base::DoNothing());
    PDFExtensionTestBase::SetUpOnMainThread();
  }
  void TearDownOnMainThread() override {
    SetShowPrintErrorDialogForTest(base::NullCallback());
    PDFExtensionTestBase::TearDownOnMainThread();
  }
};

// Flaky. See http://crbug.com/1415194
IN_PROC_BROWSER_TEST_F(PDFExtensionPrintingTest, DISABLED_BasicPrintCommand) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest);
  ASSERT_TRUE(frame);

  PrintObserver print_observer(frame);
  chrome::BasicPrint(browser());
  print_observer.WaitForPrintNow();
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_F(PDFExtensionPrintingTest, PrintCommand) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest);
  ASSERT_TRUE(frame);

  PrintObserver print_observer(frame);
  chrome::Print(browser());
  print_observer.WaitForPrintPreview();
}

IN_PROC_BROWSER_TEST_F(PDFExtensionPrintingTest,
                       ContextMenuPrintCommandExtensionMainFrame) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  ASSERT_TRUE(plugin_frame);

  content::RenderFrameHost* guest_main_frame = guest->GetGuestMainFrame();
  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(guest_main_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  PrintObserver print_observer(plugin_frame);
  guest_main_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitForPrintPreview();
  menu_interceptor.Wait();
}

// TODO(crbug.com/1344508): Test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(
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

  PrintObserver print_observer(plugin_frame);
  SimulateMouseClickAt(guest, blink::WebInputEvent::kNoModifiers,
                       blink::WebMouseEvent::Button::kLeft, {1, 1});
  guest_main_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitForPrintPreview();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_F(PDFExtensionPrintingTest,
                       ContextMenuPrintCommandPluginFrame) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  ASSERT_TRUE(plugin_frame);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  PrintObserver print_observer(plugin_frame);
  SetInputFocusOnPlugin(guest);
  plugin_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitForPrintPreview();
  menu_interceptor.Wait();
}

// TODO(crbug.com/1330032): Fix flakiness.
IN_PROC_BROWSER_TEST_F(PDFExtensionPrintingTest,
                       DISABLED_ContextMenuPrintCommandEmbeddedPluginFrame) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  ASSERT_TRUE(plugin_frame);

  // Makes sure that the correct frame invoked the context menu.
  content::ContextMenuInterceptor menu_interceptor(plugin_frame);

  // Executes the print command as soon as the context menu is shown.
  ContextMenuNotificationObserver context_menu_observer(IDC_PRINT);

  PrintObserver print_observer(plugin_frame);
  SetInputFocusOnPlugin(guest);
  plugin_frame->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      {1, 1}, ui::MENU_SOURCE_MOUSE);
  print_observer.WaitForPrintPreview();
  menu_interceptor.Wait();
}

IN_PROC_BROWSER_TEST_F(PDFExtensionPrintingTest, PrintButton) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::RenderFrameHost* frame = GetPluginFrame(guest);
  ASSERT_TRUE(frame);

  PrintObserver print_observer(frame);
  constexpr char kClickPrintButtonScript[] = R"(
    viewer.shadowRoot.querySelector('#toolbar')
        .shadowRoot.querySelector('#print')
        .click();
  )";
  EXPECT_TRUE(ExecJs(guest->GetGuestMainFrame(), kClickPrintButtonScript));
  print_observer.WaitForPrintPreview();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
