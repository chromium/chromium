// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_document_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/test_extension_registry_observer.h"
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

  // Creates a new offscreen document with the given `extension`, `url`,
  // and `profile`, and waits for it to load.
  OffscreenDocumentHost* CreateDocumentAndWaitForLoad(
      const Extension& extension,
      const GURL& url,
      Profile& profile) {
    ExtensionHostTestHelper host_waiter(&profile);
    host_waiter.RestrictToType(mojom::ViewType::kOffscreenDocument);
    OffscreenDocumentHost* offscreen_document =
        OffscreenDocumentManager::Get(&profile)->CreateOffscreenDocument(
            extension, url);
    host_waiter.WaitForHostCompletedFirstLoad();

    return offscreen_document;
  }

  // Same as the above, defaulting to the on-the-record profile.
  OffscreenDocumentHost* CreateDocumentAndWaitForLoad(
      const Extension& extension,
      const GURL& url) {
    return CreateDocumentAndWaitForLoad(extension, url, *profile());
  }

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

// Tests creating offscreen documents for an incognito split-mode extension.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentManagerBrowserTest,
                       IncognitoOffscreenDocuments) {
  // `split` incognito mode is required in order to allow the extension to
  // have a separate process in incognito.
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "incognito": "split"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  {
    // Enable the extension in incognito. This results in an extension reload;
    // wait for that to finish and update the `extension` pointer.
    TestExtensionRegistryObserver registry_observer(
        ExtensionRegistry::Get(profile()), extension->id());
    util::SetIsIncognitoEnabled(extension->id(), browser()->profile(),
                                /*enabled=*/true);
    extension = registry_observer.WaitForExtensionLoaded();
  }

  ASSERT_TRUE(extension);
  ASSERT_TRUE(util::IsIncognitoEnabled(extension->id(), profile()));

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");

  // Create an on-the-record offscreen document.
  OffscreenDocumentHost* on_the_record_host =
      CreateDocumentAndWaitForLoad(*extension, offscreen_url);
  ASSERT_TRUE(on_the_record_host);
  // Ensure the on-the-record context is used.
  // Note: Throughout this test, we use
  // `OffscreenDocumentHost::host_contents()` to access the BrowserContext
  // instead of `OffscreenDocumentHost::browser_context()`; this is to ensure
  // that the WebContents is hosted properly.
  EXPECT_FALSE(on_the_record_host->host_contents()
                   ->GetBrowserContext()
                   ->IsOffTheRecord());

  // Create an incognito browser and an incognito offscreen document, and
  // validate that the proper context is used.
  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_browser);

  OffscreenDocumentHost* incognito_host = CreateDocumentAndWaitForLoad(
      *extension, offscreen_url, *incognito_browser->profile());
  ASSERT_TRUE(incognito_host);
  EXPECT_TRUE(
      incognito_host->host_contents()->GetBrowserContext()->IsOffTheRecord());

  // These should be separate offscreen documents and have separate profiles,
  // but the same original profile.
  EXPECT_NE(incognito_host, on_the_record_host);
  EXPECT_EQ(Profile::FromBrowserContext(
                on_the_record_host->host_contents()->GetBrowserContext()),
            Profile::FromBrowserContext(
                incognito_host->host_contents()->GetBrowserContext())
                ->GetOriginalProfile());

  // Ensure the offscreen documents are registered with the appropriate
  // context.
  EXPECT_EQ(on_the_record_host,
            OffscreenDocumentManager::Get(profile())
                ->GetOffscreenDocumentForExtension(*extension));
  EXPECT_EQ(incognito_host,
            OffscreenDocumentManager::Get(incognito_browser->profile())
                ->GetOffscreenDocumentForExtension(*extension));

  {
    // Shut down the incognito browser. The `incognito_host` should be
    // destroyed.
    ExtensionHostTestHelper host_waiter(incognito_browser->profile());
    host_waiter.RestrictToHost(incognito_host);
    CloseBrowserSynchronously(incognito_browser);
    host_waiter.WaitForHostDestroyed();
    // Note: `incognito_host` is destroyed at this point.
  }

  // The on-the-record document should remain.
  EXPECT_EQ(on_the_record_host,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));
}

// Tests the flow of closing an existing offscreen document through the
// manager.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentManagerBrowserTest,
                       ClosingDocumentThroughTheManager) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");

  OffscreenDocumentHost* offscreen_document =
      CreateDocumentAndWaitForLoad(*extension, offscreen_url);
  ASSERT_TRUE(offscreen_document);

  {
    ExtensionHostTestHelper host_waiter(profile());
    host_waiter.RestrictToHost(offscreen_document);
    offscreen_document_manager()->CloseOffscreenDocumentForExtension(
        *extension);
  }

  EXPECT_EQ(nullptr,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));
}

// Tests calling window.close() in an offscreen document closes it (through the
// manager).
IN_PROC_BROWSER_TEST_F(OffscreenDocumentManagerBrowserTest,
                       CallingWindowCloseInAnOffscreenDocumentClosesIt) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  OffscreenDocumentHost* offscreen_document = CreateDocumentAndWaitForLoad(
      *extension, extension->GetResourceURL("offscreen.html"));
  ASSERT_TRUE(offscreen_document);
  EXPECT_EQ(offscreen_document,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));

  {
    // Call window.close() from the offscreen document. This should cause the
    // manager to close the document, destroying the host.
    ExtensionHostTestHelper host_waiter(profile());
    host_waiter.RestrictToHost(offscreen_document);
    ASSERT_TRUE(content::ExecuteScript(offscreen_document->host_contents(),
                                       "window.close();"));
    host_waiter.WaitForHostDestroyed();
  }

  EXPECT_EQ(nullptr,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));
}

}  // namespace extensions
