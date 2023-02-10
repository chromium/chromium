// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_BASE_H_
#define CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_BASE_H_

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  // Same as LoadPdf(), but also returns a pointer to the `MimeHandlerViewGuest`
  // for the loaded PDF. Returns nullptr if the load fails.
  extensions::MimeHandlerViewGuest* LoadPdfGetMimeHandlerView(const GURL& url);

  // Same as LoadPdf(), but also returns a pointer to the `MimeHandlerViewGuest`
  // for the loaded PDF in a new tab. Returns nullptr if the load fails.
  extensions::MimeHandlerViewGuest* LoadPdfInNewTabGetMimeHandlerView(
      const GURL& url);

  void TestGetSelectedTextReply(const GURL& url, bool expect_success);

  content::WebContents* GetActiveWebContents();

 protected:
  guest_view::TestGuestViewManager* GetGuestViewManager(
      content::BrowserContext* profile = nullptr);

  content::RenderFrameHost* GetPluginFrame(
      extensions::MimeHandlerViewGuest* guest) const;

  int CountPDFProcesses();

  void SimulateMouseClickAt(extensions::MimeHandlerViewGuest* guest,
                            int modifiers,
                            blink::WebMouseEvent::Button button,
                            const gfx::Point& point_in_guest);

  // Hooks to set up feature flags.
  virtual std::vector<base::test::FeatureRef> GetEnabledFeatures() const;
  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() const;

 private:
  base::test::ScopedFeatureList feature_list_;
  guest_view::TestGuestViewManagerFactory factory_;
};

#endif  // CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_BASE_H_
