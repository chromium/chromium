// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_document_manager.h"

#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/offscreen/lifetime_enforcer_factories.h"
#include "extensions/browser/api/offscreen/offscreen_document_lifetime_enforcer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/offscreen.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

// A programmable lifetime enforcer.
class TestLifetimeEnforcer : public OffscreenDocumentLifetimeEnforcer {
 public:
  TestLifetimeEnforcer(OffscreenDocumentHost* offscreen_document,
                       TerminationCallback termination_callback,
                       NotifyInactiveCallback notify_inactive_callback)
      : OffscreenDocumentLifetimeEnforcer(offscreen_document,
                                          std::move(termination_callback),
                                          std::move(notify_inactive_callback)) {
  }
  ~TestLifetimeEnforcer() override = default;

  void CallTerminate() { TerminateDocument(); }
  void CallNotifyInactive() {
    DCHECK(!is_active_);
    NotifyInactive();
  }

  void SetActive(bool is_active) { is_active_ = is_active; }

 private:
  bool IsActive() override { return is_active_; }

  bool is_active_ = true;
};

// A test-only factory method to create and populate a test-only lifetime
// enforcer.
std::unique_ptr<OffscreenDocumentLifetimeEnforcer> CreateTestLifetimeEnforcer(
    TestLifetimeEnforcer** lifetime_enforcer_out,
    OffscreenDocumentHost* offscreen_document,
    OffscreenDocumentLifetimeEnforcer::TerminationCallback termination_callback,
    OffscreenDocumentLifetimeEnforcer::NotifyInactiveCallback
        notify_inactive_callback) {
  auto enforcer = std::make_unique<TestLifetimeEnforcer>(
      offscreen_document, std::move(termination_callback),
      std::move(notify_inactive_callback));
  *lifetime_enforcer_out = enforcer.get();
  return enforcer;
}

}  // namespace

class OffscreenDocumentManagerBrowserTest : public ExtensionApiTest {
 public:
  OffscreenDocumentManagerBrowserTest() = default;
  ~OffscreenDocumentManagerBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Add the kOffscreenDocumentTesting switch to allow the use of the
    // `TESTING` reason in offscreen document creation.
    command_line->AppendSwitch(switches::kOffscreenDocumentTesting);
  }

  // Creates a new offscreen document with the given `extension`, `url`,
  // `reasons`, and `profile`, and waits for it to load.
  OffscreenDocumentHost* CreateDocumentAndWaitForLoad(
      const Extension& extension,
      const GURL& url,
      std::set<api::offscreen::Reason> reasons,
      Profile& profile) {
    ExtensionHostTestHelper host_waiter(&profile);
    host_waiter.RestrictToType(mojom::ViewType::kOffscreenDocument);
    OffscreenDocumentHost* offscreen_document =
        OffscreenDocumentManager::Get(&profile)->CreateOffscreenDocument(
            extension, url, reasons);
    host_waiter.WaitForHostCompletedFirstLoad();

    return offscreen_document;
  }

  // Same as above, defaulting to a single reason of Reason::kTesting.
  OffscreenDocumentHost* CreateDocumentAndWaitForLoad(
      const Extension& extension,
      const GURL& url,
      Profile& profile) {
    return CreateDocumentAndWaitForLoad(
        extension, url, {api::offscreen::Reason::kTesting}, profile);
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

  OffscreenDocumentHost* offscreen_document = CreateDocumentAndWaitForLoad(
      *extension, extension->GetResourceURL("offscreen.html"));

  {
    // Check the document loaded properly. Note: general capabilities of
    // offscreen documents are exercised more in the OffscreenDocumentHost
    // tests, but this helps sanity check that the manager created it properly.
    static constexpr char kScript[] =
        R"({
             let div = document.getElementById('signal');
             div ? div.innerText : '<no div>';
           })";
    EXPECT_EQ("Hello, World",
              content::EvalJs(offscreen_document->host_contents(), kScript));
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
    ASSERT_TRUE(content::ExecJs(offscreen_document->host_contents(),
                                "window.close();"));
    host_waiter.WaitForHostDestroyed();
  }

  EXPECT_EQ(nullptr,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));
}

// Tests that lifetime enforcers can terminate an offscreen document (such as if
// a hard limit is reached).
IN_PROC_BROWSER_TEST_F(OffscreenDocumentManagerBrowserTest,
                       LifetimeEnforcement_Terminate) {
  // Override the factory method for the testing reason to use our own
  // TestLifetimeEnforcer.
  TestLifetimeEnforcer* lifetime_enforcer = nullptr;
  LifetimeEnforcerFactories::TestingOverride factory_override;
  factory_override.map().emplace(
      api::offscreen::Reason::kTesting,
      base::BindRepeating(&CreateTestLifetimeEnforcer, &lifetime_enforcer));

  // Load an extension and create an offscreen document.
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

  // The lifetime enforcer should have been created. Call the termination
  // callback; the offscreen document should be closed.
  ASSERT_TRUE(lifetime_enforcer);
  lifetime_enforcer->CallTerminate();

  // Note: `offscreen_document` is now unsafe to use.

  EXPECT_EQ(nullptr,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));
}

// Tests that the offscreen document is terminated when all the lifetime
// enforcers (currently only ever one) notify that the document is inactive.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentManagerBrowserTest,
                       LifetimeEnforcement_NotifyInactive) {
  // Override the factory method for the testing reason to use our own
  // TestLifetimeEnforcer.
  TestLifetimeEnforcer* lifetime_enforcer = nullptr;
  LifetimeEnforcerFactories::TestingOverride factory_override;
  factory_override.map().emplace(
      api::offscreen::Reason::kTesting,
      base::BindRepeating(&CreateTestLifetimeEnforcer, &lifetime_enforcer));

  // Load an extension and create an offscreen document.
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

  // The lifetime enforcer should have been created.
  ASSERT_TRUE(lifetime_enforcer);

  // Set the document to be inactive and notify. The document should be closed.
  lifetime_enforcer->SetActive(false);
  lifetime_enforcer->CallNotifyInactive();

  // Note: `offscreen_document` and `lifetime_enforcer` are now unsafe to use.

  EXPECT_EQ(nullptr,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));
}

// Tests that when multiple reasons are provided, a lifetime enforcer is
// created for each, and the offscreen document is only terminated once all
// lifetime enforcers indicate the document is inactive.
IN_PROC_BROWSER_TEST_F(
    OffscreenDocumentManagerBrowserTest,
    LifetimeEnforcement_DocumentIsNotTerminatedUntilAllInactive) {
  // Override the factory method for both the dom parsing and blobs reasons to
  // use our own TestLifetimeEnforcer.
  TestLifetimeEnforcer* dom_parser_enforcer = nullptr;
  LifetimeEnforcerFactories::TestingOverride factory_override;
  factory_override.map().emplace(
      api::offscreen::Reason::kDomParser,
      base::BindRepeating(&CreateTestLifetimeEnforcer, &dom_parser_enforcer));
  TestLifetimeEnforcer* blobs_enforcer = nullptr;
  factory_override.map().emplace(
      api::offscreen::Reason::kBlobs,
      base::BindRepeating(&CreateTestLifetimeEnforcer, &blobs_enforcer));

  // Load an extension an create an offscreen document.
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

  // Create a new document for both the blob and dom parser reasons.
  OffscreenDocumentHost* offscreen_document = CreateDocumentAndWaitForLoad(
      *extension, extension->GetResourceURL("offscreen.html"),
      {api::offscreen::Reason::kBlobs, api::offscreen::Reason::kDomParser},
      *profile());
  ASSERT_TRUE(offscreen_document);
  EXPECT_EQ(offscreen_document,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));

  // Each lifetime enforcer should have been created.
  ASSERT_TRUE(dom_parser_enforcer);
  ASSERT_TRUE(blobs_enforcer);

  // Set the dom parser enforcer to be inactive. Note that the blob enforcer is
  // still active.
  dom_parser_enforcer->SetActive(false);
  dom_parser_enforcer->CallNotifyInactive();

  // The document should still be around, since the blob enforcer is still
  // active.
  EXPECT_EQ(offscreen_document,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));

  // Re-activate and deactivate the dom parser enforcer (to verify it's okay for
  // it to cycle between states multiple times).
  // Note: Technically, the SetActive() calls here aren't necessary, but it
  // better indicates the real scenario.
  dom_parser_enforcer->SetActive(true);
  dom_parser_enforcer->SetActive(false);
  dom_parser_enforcer->CallNotifyInactive();

  // The document should still be active.
  EXPECT_EQ(offscreen_document,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));

  // Switch the active enforcers, first making the dom parser enforcer active,
  // then making the blob enforcer inactive.
  dom_parser_enforcer->SetActive(true);
  blobs_enforcer->SetActive(false);
  blobs_enforcer->CallNotifyInactive();

  // As above, the document should still be around, since a lifetime enforcer
  // is still active (this time, the dom parser enforcer).
  EXPECT_EQ(offscreen_document,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));

  // Finally, re-mark the dom parser as inactive.
  dom_parser_enforcer->SetActive(false);
  dom_parser_enforcer->CallNotifyInactive();

  // Note: `offscreen_document`, `dom_parser_enforcer`, and `blobs_enforcer`
  // are all now unsafe to use!

  // Now, the document should be closed.
  EXPECT_EQ(nullptr,
            offscreen_document_manager()->GetOffscreenDocumentForExtension(
                *extension));
}

}  // namespace extensions
