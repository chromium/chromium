// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_test_base.h"

#include <string>
#include <vector>

#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/pdf_frame_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point.h"

using ::content::WebContents;
using ::extensions::ExtensionsAPIClient;
using ::extensions::MimeHandlerViewGuest;
using ::guest_view::GuestViewManager;
using ::guest_view::TestGuestViewManager;
using ::pdf_extension_test_util::GetOnlyMimeHandlerView;

PDFExtensionTestBase::PDFExtensionTestBase() {
  GuestViewManager::set_factory_for_testing(&factory_);
}

void PDFExtensionTestBase::SetUpCommandLine(
    base::CommandLine* /*command_line*/) {
  feature_list_.InitWithFeatures(GetEnabledFeatures(), GetDisabledFeatures());
}

void PDFExtensionTestBase::SetUpOnMainThread() {
  extensions::ExtensionApiTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  content::SetupCrossSiteRedirector(embedded_test_server());
  embedded_test_server()->StartAcceptingConnections();
}

void PDFExtensionTestBase::TearDownOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  extensions::ExtensionApiTest::TearDownOnMainThread();
}

// Serve paths prefixed with _test_resources/ from chrome/test/data.
base::FilePath PDFExtensionTestBase::GetTestResourcesParentDir() {
  base::FilePath test_root_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
  return test_root_path;
}

bool PDFExtensionTestBase::PdfIsExpectedToLoad(const std::string& pdf_file) {
  const char* const kFailingPdfs[] = {
      // clang-format off
        "pdf/test-ranges.pdf",
        "pdf_private/accessibility_crash_1.pdf",
        "pdf_private/cfuzz5.pdf",
        "pdf_private/js.pdf",
        "pdf_private/segv-ecx.pdf",
        "pdf_private/tests.pdf",
      // clang-format on
  };
  for (const char* failing_pdf : kFailingPdfs) {
    if (failing_pdf == pdf_file) {
      return false;
    }
  }
  return true;
}

// Load the PDF at the given URL and ensure it has finished loading. Return
// true if it loads successfully or false if it fails. If it doesn't finish
// loading the test will hang. This is done from outside of the BrowserPlugin
// guest to ensure sending messages to/from the plugin works correctly from
// there, since the PdfScriptingApi relies on doing this as well.
testing::AssertionResult PDFExtensionTestBase::LoadPdf(const GURL& url) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* web_contents = GetActiveWebContents();
  return pdf_extension_test_util::EnsurePDFHasLoaded(web_contents);
}

// Same as LoadPDF(), but loads into a new tab.
testing::AssertionResult PDFExtensionTestBase::LoadPdfInNewTab(
    const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* web_contents = GetActiveWebContents();
  return pdf_extension_test_util::EnsurePDFHasLoaded(web_contents);
}

// Same as LoadPdf(), but also returns a pointer to the `MimeHandlerViewGuest`
// for the loaded PDF. Returns nullptr if the load fails.
MimeHandlerViewGuest* PDFExtensionTestBase::LoadPdfGetMimeHandlerView(
    const GURL& url) {
  if (!LoadPdf(url)) {
    return nullptr;
  }
  return GetOnlyMimeHandlerView(GetActiveWebContents());
}

// Same as LoadPdf(), but also returns a pointer to the `MimeHandlerViewGuest`
// for the loaded PDF in a new tab. Returns nullptr if the load fails.
MimeHandlerViewGuest* PDFExtensionTestBase::LoadPdfInNewTabGetMimeHandlerView(
    const GURL& url) {
  if (!LoadPdfInNewTab(url)) {
    return nullptr;
  }
  return GetOnlyMimeHandlerView(GetActiveWebContents());
}

void PDFExtensionTestBase::TestGetSelectedTextReply(const GURL& url,
                                                    bool expect_success) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(url);
  ASSERT_TRUE(guest);

  // Reach into the guest and hook into it such that it posts back a 'flush'
  // message after every getSelectedTextReply message sent.
  ASSERT_TRUE(
      content::ExecuteScript(guest->GetGuestMainFrame(),
                             "viewer.overrideSendScriptingMessageForTest();"));

  // Add an event listener for flush messages and request the selected text.
  // If we get a flush message without receiving getSelectedText we know that
  // the message didn't come through.
  ASSERT_EQ(
      expect_success,
      content::EvalJs(GetActiveWebContents(),
                      "new Promise(resolve => {"
                      "  window.addEventListener('message', function(event) {"
                      "    if (event.data == 'flush')"
                      "      resolve(false);"
                      "    if (event.data.type == 'getSelectedTextReply')"
                      "      resolve(true);"
                      "  });"
                      "  document.getElementsByTagName('embed')[0].postMessage("
                      "      {type: 'getSelectedText'});"
                      "});"));
}

WebContents* PDFExtensionTestBase::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

TestGuestViewManager* PDFExtensionTestBase::GetGuestViewManager(
    content::BrowserContext* profile) {
  if (!profile) {
    profile = browser()->profile();
  }
  // TODO(wjmaclean): Re-implement FromBrowserContext in the
  // TestGuestViewManager class to avoid all callers needing this cast.
  auto* manager = static_cast<TestGuestViewManager*>(
      TestGuestViewManager::FromBrowserContext(profile));
  // Test code may access the TestGuestViewManager before it would be created
  // during creation of the first guest.
  if (!manager) {
    manager =
        static_cast<TestGuestViewManager*>(GuestViewManager::CreateWithDelegate(
            profile, ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                         profile)));
  }
  return manager;
}

content::RenderFrameHost* PDFExtensionTestBase::GetPluginFrame(
    MimeHandlerViewGuest* guest) const {
  return pdf_frame_util::FindPdfChildFrame(guest->GetGuestMainFrame());
}

int PDFExtensionTestBase::CountPDFProcesses() {
  return pdf_extension_test_util::CountPdfPluginProcesses(browser());
}

void PDFExtensionTestBase::SimulateMouseClickAt(
    extensions::MimeHandlerViewGuest* guest,
    int modifiers,
    blink::WebMouseEvent::Button button,
    const gfx::Point& point_in_guest) {
  auto* guest_main_frame = guest->GetGuestMainFrame();
  content::WaitForHitTestData(guest_main_frame);

  const gfx::Point point_in_root_coords =
      guest_main_frame->GetView()->TransformPointToRootCoordSpace(
          point_in_guest);
  content::SimulateMouseClickAt(guest->embedder_web_contents(), modifiers,
                                button, point_in_root_coords);
}

std::vector<base::test::FeatureRef> PDFExtensionTestBase::GetEnabledFeatures()
    const {
  return {};
}

std::vector<base::test::FeatureRef> PDFExtensionTestBase::GetDisabledFeatures()
    const {
  return {};
}
