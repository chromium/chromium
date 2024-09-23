// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_BASE_H_
#define CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class MimeHandlerViewGuest;
}

namespace gfx {
class Point;
}

class PDFExtensionTestBase : public extensions::ExtensionApiTest {
 public:
  PDFExtensionTestBase();

  ~PDFExtensionTestBase() override;

  // extensions::ExtensionApiTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  base::FilePath GetTestResourcesParentDir() override;

  bool PdfIsExpectedToLoad(const std::string& pdf_file);

  // Load the PDF at the given URL and ensure it has finished loading. Return
  // true if it loads successfully or false if it fails. If it doesn't finish
  // loading the test will hang. This is done from outside of the BrowserPlugin
  // guest to ensure sending messages to/from the plugin works correctly from
  // there, since the PdfScriptingApi relies on doing this as well.
  testing::AssertionResult LoadPdf(const GURL& url);

  // Same as LoadPDF(), but loads into a new tab.
  testing::AssertionResult LoadPdfInNewTab(const GURL& url);

  // Same as `LoadPdf()` but loads URLs where the first child of the primary
  // main frame should be the embedder. This is a common case where an HTML page
  // only embeds a single PDF. For GuestView PDF viewer, the embedder must be an
  // embed element.
  testing::AssertionResult LoadPdfInFirstChild(const GURL& url);

  // Same as `LoadPdf()` but allows the PDF embedder host to have multiple
  // subframes. There are special cases where this can occur, such as
  // crbug.com/40671023.
  testing::AssertionResult LoadPdfAllowMultipleFrames(const GURL& url);

  // Same as LoadPdf(), but also returns a pointer to the `MimeHandlerViewGuest`
  // for the loaded PDF. Returns nullptr if the load fails.
  extensions::MimeHandlerViewGuest* LoadPdfGetMimeHandlerView(const GURL& url);

  // Same as LoadPdf(), but also returns a pointer to the `MimeHandlerViewGuest`
  // for the loaded PDF in a new tab. Returns nullptr if the load fails.
  extensions::MimeHandlerViewGuest* LoadPdfInNewTabGetMimeHandlerView(
      const GURL& url);

  // Same as `LoadPdf()`, but also returns a pointer to the extension host for
  // the loaded PDF. Returns nullptr if the load fails or getting the extension
  // host fails. The test will fail if the load fails.
  content::RenderFrameHost* LoadPdfGetExtensionHost(const GURL& url);

  // Same as `LoadPdfGetExtensionHost()`, but loads the PDF into a new tab.
  content::RenderFrameHost* LoadPdfInNewTabGetExtensionHost(const GURL& url);

  // Same as `LoadPdfInFirstChild()`, but also returns a pointer to the
  // extension host for the loaded PDF. Returns nullptr if the load fails or
  // getting the extension host fails. The test will fail if the load fails.
  content::RenderFrameHost* LoadPdfInFirstChildGetExtensionHost(
      const GURL& url);

  // Test if a page embedding a PDF can get selected text in the PDF. The test
  // will fail if the hook for sending flush messages for every getSelectedText
  // message fails to attach to `extension_host`. The test will fail if the
  // result of getting selected text does not match `expect_success`.
  void TestGetSelectedTextReply(content::RenderFrameHost* extension_host,
                                bool expect_success);

  content::WebContents* GetActiveWebContents();

  // For OOPIF PDF viewer, returns the active `WebContents`, as there is only a
  // single `WebContents`. For GuestView PDF viewer, returns the embedder
  // `WebContents`.
  content::WebContents* GetEmbedderWebContents();

 protected:
  guest_view::TestGuestViewManager* GetGuestViewManager();
  guest_view::TestGuestViewManager* GetGuestViewManagerForProfile(
      content::BrowserContext* profile);

  pdf::TestPdfViewerStreamManager* GetTestPdfViewerStreamManager(
      content::WebContents* contents);

  void CreateTestPdfViewerStreamManager(content::WebContents* contents);

  content::RenderFrameHost* GetOnlyPdfExtensionHostEnsureValid();

  int CountPDFProcesses() const;

  // Checks if the full page PDF loaded. The test will fail if it does not meet
  // the requirements of `ValidateFrameTree()`.
  testing::AssertionResult EnsureFullPagePDFHasLoadedWithValidFrameTree(
      content::WebContents* contents,
      bool allow_multiple_frames);

  // Check if the PDF loaded in the first child frame of `contents`. The test
  // will fail if it does not meet the requirements of `ValidateFrameTree()`.
  testing::AssertionResult EnsurePDFHasLoadedInFirstChildWithValidFrameTree(
      content::WebContents* contents);

  void SimulateMouseClickAt(content::RenderFrameHost* extension_host,
                            content::WebContents* contents,
                            int modifiers,
                            blink::WebMouseEvent::Button button,
                            const gfx::Point& point_in_extension);

  // Returns true if the test should use the OOPIF PDF viewer instead of the
  // GuestView PDF viewer.
  // TODO(crbug.com/40268279): Remove once only OOPIF PDF viewer is used.
  virtual bool UseOopif() const;

  // Hooks to set up feature flags.
  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const;
  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() const;

 private:
  // The test will fail if the frame tree does not have exactly one PDF
  // extension host and one PDF content host. For GuestView PDF viewer, the test
  // will also fail if there is not exactly one GuestView.
  void ValidateFrameTree(content::WebContents* contents);

  base::test::ScopedFeatureList feature_list_;
  absl::variant<absl::monostate,
                std::unique_ptr<guest_view::TestGuestViewManagerFactory>,
                std::unique_ptr<pdf::TestPdfViewerStreamManagerFactory>>
      factory_;
};

#endif  // CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_BASE_H_
