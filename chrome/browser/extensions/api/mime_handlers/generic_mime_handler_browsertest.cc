// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/mime_handler/mime_handler_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {
constexpr char kPdfMimeType[] = "application/pdf";
}  // namespace

class GenericMimeHandlerBrowserTest : public ExtensionApiTest {
 public:
  GenericMimeHandlerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::kApiMimeHandler},
        /*disabled_features=*/{});
  }

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/pdf");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedCurrentChannel channel_{version_info::Channel::UNKNOWN};
};

// Verifies that navigating to an application/pdf URL handled by a generic MIME
// handler extension loads the handler page in an OOPIF and that
// chrome.mimeHandler.getStreamInfo() returns correct stream metadata.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest, GetStreamInfo) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler"));
  ASSERT_TRUE(extension);

  // Verify the extension registered as a generic MIME handler.
  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  ASSERT_TRUE(handler);
  auto* registry = MimeHandlerRegistry::Get(profile());
  ASSERT_TRUE(registry);
  ASSERT_FALSE(handler->IsPluginExtension());
  ASSERT_EQ(extension->id(),
            registry->GetHandlerForMimeType("application/pdf"));

  // Set up ResultCatcher before navigation so it catches the extension's
  // chrome.test.succeed() call.
  ResultCatcher catcher;

  // Navigate to an application/pdf resource. The throttle should intercept
  // this and route it through the generic MIME handler's OOPIF path.
  GURL pdf_url = embedded_test_server()->GetURL("/test.pdf");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), pdf_url));

  // The handler.js in the extension calls chrome.test.succeed() after
  // verifying getStreamInfo fields and fetching the stream data.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Verifies that a generic MIME handler with `can_embed: false` is selected
// for top-level navigations but bypassed for embedded loads.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       EmbeddedLoadHonorsCanEmbed) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(extension->id(),
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/false));
  EXPECT_NE(extension->id(),
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/true));
}

// With the handler disabled via options, the navigation-path lookup
// (PluginUtils::GetExtensionIdForMimeType) must not return the
// extension id, and the resulting page must not contain an extension
// frame owned by this handler.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       DisabledHandlerFallsBackOnNavigation) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler"));
  ASSERT_TRUE(extension);

  // Before disable: extension is the active handler along both paths.
  auto* registry = MimeHandlerRegistry::Get(profile());
  ASSERT_TRUE(registry);
  ASSERT_EQ(extension->id(), registry->GetHandlerForMimeType(kPdfMimeType));
  ASSERT_EQ(extension->id(),
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/false));

  // Disable for the PDF MIME type.
  registry->SetEnabledForMimeType(extension->id(), kPdfMimeType, false);

  // Both lookup paths now skip this handler and fall through to the
  // built-in PDF extension, which is always registered for
  // application/pdf in this browsertest profile.
  EXPECT_EQ(extension_misc::kPdfExtensionId,
            registry->GetHandlerForMimeType(kPdfMimeType));
  EXPECT_EQ(extension_misc::kPdfExtensionId,
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/false));

  // Navigate. The extension must NOT own any frame in the resulting page.
  const GURL pdf_url = embedded_test_server()->GetURL("/test.pdf");
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, pdf_url));

  // Walk the frame tree. No frame should be owned by the extension.
  const std::string extension_host = extension->id();
  bool found_extension_frame = false;
  web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    const GURL& url = rfh->GetLastCommittedURL();
    if (url.SchemeIs(kExtensionScheme) && url.host() == extension_host) {
      found_extension_frame = true;
    }
  });
  EXPECT_FALSE(found_extension_frame);
}

// Full JS-to-navigation end-to-end: the test extension's handler.js
// calls chrome.mimeHandler.setMimeHandlerOptions({enabled:false}), then
// a subsequent navigation confirms the pref took effect through the
// navigation path.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerBrowserTest,
                       DisableViaApiFallsBackOnNextNavigation) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler"));
  ASSERT_TRUE(extension);

  auto* registry = MimeHandlerRegistry::Get(profile());
  ASSERT_TRUE(registry);
  ASSERT_EQ(extension->id(), registry->GetHandlerForMimeType(kPdfMimeType));

  // First navigation: /test.pdf?action=disable. handler.js branches
  // to calling chrome.mimeHandler.setMimeHandlerOptions and then
  // chrome.test.succeed(). Wait for ResultCatcher to observe success.
  ResultCatcher catcher;
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, embedded_test_server()->GetURL(
                                              "/test.pdf?action=disable")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // JS setMimeHandlerOptions has persisted the pref. Registry now
  // reports the handler disabled for the PDF MIME type.
  EXPECT_FALSE(registry->IsEnabledForMimeType(extension->id(), kPdfMimeType));

  // Both lookup paths now skip this handler and fall through to the
  // built-in PDF extension, which is always registered for
  // application/pdf in this browsertest profile.
  EXPECT_EQ(extension_misc::kPdfExtensionId,
            registry->GetHandlerForMimeType(kPdfMimeType));
  EXPECT_EQ(extension_misc::kPdfExtensionId,
            PluginUtils::GetExtensionIdForMimeType(profile(), kPdfMimeType,
                                                   /*embedded=*/false));

  // Second navigation: plain /test.pdf. The extension must NOT own any
  // frame (built-in PDF or native fallback handles the response).
  ASSERT_TRUE(
      NavigateToURL(web_contents, embedded_test_server()->GetURL("/test.pdf")));

  // Walk only the active (primary) frame tree. The first navigation's
  // document may still be retained in the BackForwardCache and would be
  // visited by WebContents::ForEachRenderFrameHost.
  const std::string extension_host = extension->id();
  bool found_extension_frame = false;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        const GURL& url = rfh->GetLastCommittedURL();
        if (url.SchemeIs(kExtensionScheme) && url.host() == extension_host) {
          found_extension_frame = true;
        }
      });
  EXPECT_FALSE(found_extension_frame);
}

}  // namespace extensions
