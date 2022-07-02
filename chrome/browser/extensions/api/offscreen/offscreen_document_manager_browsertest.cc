// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_document_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class OffscreenDocumentManagerBrowserTest : public ExtensionApiTest {
 public:
  OffscreenDocumentManagerBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsOffscreenDocuments);
  }
  ~OffscreenDocumentManagerBrowserTest() override = default;

  OffscreenDocumentManager* offscreen_document_manager() {
    return OffscreenDocumentManager::Get(profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the flow of the OffscreenDocumentManager creating a new offscreen
// document for an extension.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentManagerBrowserTest,
                       CreateOffscreenDocument) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  static constexpr char kOffscreenDocumentHtml[] =
      R"(<html>
           <body>
             <div id="signal">Hello, World</div>
           </body>
         </html>)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     kOffscreenDocumentHtml);

  // Note: We wrap `extension` in a refptr because we'll unload it later in the
  // test and need to make sure the object isn't deleted.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // To start, the manager should not have any offscreen documents registered.
  EXPECT_EQ(nullptr,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));

  OffscreenDocumentHost* offscreen_document = nullptr;
  {
    // Instruct the manager to create a new offscreen document and wait for it
    // to load.
    ExtensionHostTestHelper host_waiter(profile());
    host_waiter.RestrictToType(mojom::ViewType::kOffscreenDocument);
    offscreen_document = offscreen_document_manager()->CreateOffscreenDocument(
        *extension, extension->GetResourceURL("offscreen.html"));
    ASSERT_TRUE(offscreen_document);
    host_waiter.WaitForHostCompletedFirstLoad();
  }

  {
    // Check the document loaded properly. Note: general capabilities of
    // offscreen documents are exercised more in the OffscreenDocumentHost
    // tests, but this helps sanity check that the manager created it properly.
    static constexpr char kScript[] =
        R"({
             let div = document.getElementById('signal');
             domAutomationController.send(div ? div.innerText : '<no div>');
           })";
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        offscreen_document->host_contents(), kScript, &result));
    EXPECT_EQ("Hello, World", result);
  }

  // The manager should now have a record of a document for the extension.
  EXPECT_EQ(offscreen_document,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));

  {
    // Disable the extension. This causes it to unload, and the offscreen
    // document should be closed.
    ExtensionHostTestHelper host_waiter(profile());
    host_waiter.RestrictToHost(offscreen_document);
    extension_service()->DisableExtension(extension->id(),
                                          disable_reason::DISABLE_USER_ACTION);
    host_waiter.WaitForHostDestroyed();
    // Note: `offscreen_document` is destroyed at this point.
  }

  // There should no longer be a document for the extension.
  EXPECT_EQ(nullptr,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));
}

}  // namespace extensions
