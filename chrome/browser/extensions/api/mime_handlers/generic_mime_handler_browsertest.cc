// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/mime_handler/mime_handler_registry.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

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
  auto* registry =
      MimeHandlerRegistry::Get(chrome_test_utils::GetProfile(this));
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

}  // namespace extensions
