// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_test_base.h"

#include <memory>
#include <string>
#include <vector>

#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
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
#include "pdf/pdf_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point.h"

using ::content::WebContents;
using ::extensions::ExtensionsAPIClient;
using ::extensions::MimeHandlerViewGuest;
using ::guest_view::GuestViewManager;
using ::guest_view::TestGuestViewManager;
using ::pdf_extension_test_util::GetOnlyMimeHandlerView;

PDFExtensionTestBase::PDFExtensionTestBase() = default;

PDFExtensionTestBase::~PDFExtensionTestBase() = default;

void PDFExtensionTestBase::SetUpCommandLine(
    base::CommandLine* /*command_line*/) {
  feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                              GetDisabledFeatures());
}

void PDFExtensionTestBase::SetUpOnMainThread() {
  extensions::ExtensionApiTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  content::SetupCrossSiteRedirector(embedded_test_server());
  embedded_test_server()->StartAcceptingConnections();

  if (UseOopif()) {
    factory_ = std::make_unique<pdf::TestPdfViewerStreamManagerFactory>();
  } else {
    factory_ = std::make_unique<guest_view::TestGuestViewManagerFactory>();
  }
}

void PDFExtensionTestBase::TearDownOnMainThread() {
  factory_ = absl::monostate();
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
  return EnsureFullPagePDFHasLoadedWithValidFrameTree(
      GetActiveWebContents(),
      /*allow_multiple_frames=*/false);
}

// Same as LoadPDF(), but loads into a new tab.
testing::AssertionResult PDFExtensionTestBase::LoadPdfInNewTab(
    const GURL& url) {
  EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  return EnsureFullPagePDFHasLoadedWithValidFrameTree(
      GetActiveWebContents(), /*allow_multiple_frames=*/false);
}

testing::AssertionResult PDFExtensionTestBase::LoadPdfInFirstChild(
    const GURL& url) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  return EnsurePDFHasLoadedInFirstChildWithValidFrameTree(
      GetActiveWebContents());
}

testing::AssertionResult PDFExtensionTestBase::LoadPdfAllowMultipleFrames(
    const GURL& url) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  return EnsureFullPagePDFHasLoadedWithValidFrameTree(
      GetActiveWebContents(),
      /*allow_multiple_frames=*/true);
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

content::RenderFrameHost* PDFExtensionTestBase::LoadPdfGetExtensionHost(
    const GURL& url) {
  if (!LoadPdf(url)) {
    ADD_FAILURE() << "Failed to load PDF";
    return nullptr;
  }

  return GetOnlyPdfExtensionHostEnsureValid();
}

content::RenderFrameHost* PDFExtensionTestBase::LoadPdfInNewTabGetExtensionHost(
    const GURL& url) {
  if (!LoadPdfInNewTab(url)) {
    ADD_FAILURE() << "Failed to load PDF";
    return nullptr;
  }

  return GetOnlyPdfExtensionHostEnsureValid();
}

content::RenderFrameHost*
PDFExtensionTestBase::LoadPdfInFirstChildGetExtensionHost(const GURL& url) {
  if (!LoadPdfInFirstChild(url)) {
    ADD_FAILURE() << "Failed to load PDF";
    return nullptr;
  }

  std::vector<content::RenderFrameHost*> extension_hosts =
      pdf_extension_test_util::GetPdfExtensionHosts(GetActiveWebContents());
  if (extension_hosts.empty()) {
    return nullptr;
  }

  return extension_hosts[0];
}

void PDFExtensionTestBase::TestGetSelectedTextReply(
    content::RenderFrameHost* extension_host,
    bool expect_success) {
  static constexpr char kGetSelectedTextReplyScript[] = R"(
    new Promise(resolve => {
      window.addEventListener('message', function(event) {
        if (event.data == 'flush') {
          resolve(false);
        } else if (event.data.type == 'getSelectedTextReply') {
          resolve(true);
        }
      });
      document.getElementsByTagName("embed")[0].postMessage(
        {type: 'getSelectedText'});
    })
  )";

  // Reach into the extension host and hook into it such that it posts back a
  // 'flush' message after every getSelectedTextReply message sent.
  ASSERT_TRUE(content::ExecJs(extension_host,
                              "viewer.overrideSendScriptingMessageForTest();"));

  // Add an event listener for flush messages and request the selected text.
  // If there's a flush message received but not a getSelectedText message, then
  // the message didn't come through.
  EXPECT_EQ(expect_success,
            content::EvalJs(GetEmbedderWebContents()->GetPrimaryMainFrame(),
                            kGetSelectedTextReplyScript));
}

WebContents* PDFExtensionTestBase::GetActiveWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

content::WebContents* PDFExtensionTestBase::GetEmbedderWebContents() {
  content::WebContents* contents = GetActiveWebContents();

  // OOPIF PDF viewer only has a single `WebContents`.
  if (UseOopif()) {
    return contents;
  }

  MimeHandlerViewGuest* guest =
      pdf_extension_test_util::GetOnlyMimeHandlerView(contents);
  return guest ? guest->embedder_web_contents() : nullptr;
}

TestGuestViewManager* PDFExtensionTestBase::GetGuestViewManager() {
  return GetGuestViewManagerForProfile(nullptr);
}

TestGuestViewManager* PDFExtensionTestBase::GetGuestViewManagerForProfile(
    content::BrowserContext* profile) {
  if (!profile) {
    profile = browser()->profile();
  }
  return absl::get<std::unique_ptr<guest_view::TestGuestViewManagerFactory>>(
             factory_)
      ->GetOrCreateTestGuestViewManager(
          profile,
          ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
}

pdf::TestPdfViewerStreamManager*
PDFExtensionTestBase::GetTestPdfViewerStreamManager(
    content::WebContents* contents) {
  return absl::get<std::unique_ptr<pdf::TestPdfViewerStreamManagerFactory>>(
             factory_)
      ->GetTestPdfViewerStreamManager(contents);
}

void PDFExtensionTestBase::CreateTestPdfViewerStreamManager(
    content::WebContents* contents) {
  absl::get<std::unique_ptr<pdf::TestPdfViewerStreamManagerFactory>>(factory_)
      ->CreatePdfViewerStreamManager(contents);
}

content::RenderFrameHost*
PDFExtensionTestBase::GetOnlyPdfExtensionHostEnsureValid() {
  auto* web_contents = GetActiveWebContents();
  content::RenderFrameHost* extension_host =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents);

  if (!UseOopif()) {
    auto* guest_view = GetGuestViewManager()->GetLastGuestViewCreated();
    if (!guest_view) {
      return nullptr;
    }
    EXPECT_EQ(guest_view->GetGuestMainFrame(), extension_host);
    EXPECT_NE(web_contents->GetPrimaryMainFrame(), extension_host);
  }
  return extension_host;
}

int PDFExtensionTestBase::CountPDFProcesses() const {
  return pdf_extension_test_util::CountPdfPluginProcesses(browser());
}

testing::AssertionResult
PDFExtensionTestBase::EnsureFullPagePDFHasLoadedWithValidFrameTree(
    content::WebContents* contents,
    bool allow_multiple_frames) {
  testing::AssertionResult result = testing::AssertionFailure();
  if (UseOopif()) {
    auto* manager = GetTestPdfViewerStreamManager(contents);
    content::RenderFrameHost* embedder_host = contents->GetPrimaryMainFrame();
    result = allow_multiple_frames
                 ? manager->WaitUntilPdfLoadedAllowMultipleFrames(embedder_host)
                 : manager->WaitUntilPdfLoaded(embedder_host);
  } else {
    result = pdf_extension_test_util::EnsurePDFHasLoaded(contents);
  }

  ValidateFrameTree(contents);

  return result;
}

testing::AssertionResult
PDFExtensionTestBase::EnsurePDFHasLoadedInFirstChildWithValidFrameTree(
    content::WebContents* contents) {
  testing::AssertionResult result =
      UseOopif() ? GetTestPdfViewerStreamManager(contents)
                       ->WaitUntilPdfLoadedInFirstChild()
                 : pdf_extension_test_util::EnsurePDFHasLoaded(contents);

  ValidateFrameTree(contents);

  return result;
}

void PDFExtensionTestBase::SimulateMouseClickAt(
    content::RenderFrameHost* extension_host,
    content::WebContents* contents,
    int modifiers,
    blink::WebMouseEvent::Button button,
    const gfx::Point& point_in_extension) {
  content::WaitForHitTestData(extension_host);

  const gfx::Point point_in_root_coords =
      extension_host->GetView()->TransformPointToRootCoordSpace(
          point_in_extension);
  content::SimulateMouseClickAt(contents, modifiers, button,
                                point_in_root_coords);
}

bool PDFExtensionTestBase::UseOopif() const {
  return false;
}

std::vector<base::test::FeatureRefAndParams>
PDFExtensionTestBase::GetEnabledFeatures() const {
  std::vector<base::test::FeatureRefAndParams> enabled;
  if (UseOopif()) {
    enabled.push_back({chrome_pdf::features::kPdfOopif, {}});
  }
  return enabled;
}

std::vector<base::test::FeatureRef> PDFExtensionTestBase::GetDisabledFeatures()
    const {
  std::vector<base::test::FeatureRef> disabled;
  if (!UseOopif()) {
    disabled.push_back(chrome_pdf::features::kPdfOopif);
  }
  return disabled;
}

void PDFExtensionTestBase::ValidateFrameTree(content::WebContents* contents) {
  // Ensure the frame tree contains a PDF extension host and a PDF plugin frame.
  EXPECT_TRUE(pdf_extension_test_util::GetOnlyPdfExtensionHost(contents));
  EXPECT_TRUE(pdf_extension_test_util::GetOnlyPdfPluginFrame(contents));

  // For GuestView PDF viewer, ensure there's an
  // `extensions::MimeHandlerViewGuest`.
  if (!UseOopif()) {
    EXPECT_TRUE(GetOnlyMimeHandlerView(contents));
  }
}
