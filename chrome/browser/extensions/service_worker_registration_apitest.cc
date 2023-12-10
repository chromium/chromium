// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/script_result_queue.h"
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// A helper class to wait for the service worker context to be initialized.
class WorkerInitializedWaiter : public ServiceWorkerTaskQueue::TestObserver {
 public:
  explicit WorkerInitializedWaiter(ExtensionId extension_id)
      : extension_id_(std::move(extension_id)) {
    ServiceWorkerTaskQueue::SetObserverForTest(this);
  }

  ~WorkerInitializedWaiter() override {
    ServiceWorkerTaskQueue::SetObserverForTest(nullptr);
  }

  void WaitForWorkerContextInitialized() { run_loop_.Run(); }

 private:
  // ServiceWorkerTaskQueue::TestObserver:
  void DidInitializeServiceWorkerContext(
      const ExtensionId& extension_id) override {
    if (extension_id == extension_id_) {
      run_loop_.Quit();
    }
  }

  const ExtensionId extension_id_;
  base::RunLoop run_loop_;
};

// Tests related to the registration state of extension background service
// workers.
class ServiceWorkerRegistrationApiTest : public ExtensionApiTest {
 public:
  ServiceWorkerRegistrationApiTest() = default;
  ~ServiceWorkerRegistrationApiTest() override = default;

  // Retrieves the registration state of the service worker for the given
  // `extension` from the //content layer.
  content::ServiceWorkerCapability GetServiceWorkerRegistrationState(
      const Extension& extension) {
    const GURL& root_scope = extension.url();
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFirstParty(extension.origin());
    base::test::TestFuture<content::ServiceWorkerCapability> future;
    content::ServiceWorkerContext* service_worker_context =
        util::GetStoragePartitionForExtensionId(extension.id(), profile())
            ->GetServiceWorkerContext();
    service_worker_context->CheckHasServiceWorker(root_scope, storage_key,
                                                  future.GetCallback());
    return future.Get();
  }

  // Returns true if the extension with the specified `extension_id` has an
  // active worker registered in the ProcessManager.
  bool HasActiveServiceWorker(const ExtensionId& extension_id) {
    ProcessManager* process_manager = ProcessManager::Get(profile());
    std::vector<WorkerId> worker_ids =
        process_manager->GetServiceWorkersForExtension(extension_id);
    if (worker_ids.size() > 1) {
      // We should never have more than one worker registered in the process
      // manager for a given extension.
      ADD_FAILURE() << "Muliple active worker IDs found for extension.";
      return false;
    }
    return worker_ids.size() == 1;
  }

  // Returns the value of `self.currentVersion` in the service worker context
  // of the extension with the given `extension_id`.
  int GetVersionFlagFromServiceWorker(const ExtensionId& extension_id) {
    static constexpr char kScript[] =
        R"(chrome.test.sendScriptResult(
               self.currentVersion ? self.currentVersion : -1);)";
    return BackgroundScriptExecutor::ExecuteScript(
               profile(), extension_id, kScript,
               BackgroundScriptExecutor::ResultCapture::kSendScriptResult)
        .GetInt();
  }
};

// TODO(devlin): There's overlap with service_worker_apitest.cc in this file,
// and other tests in that file that should go here so that it's less
// monolithic.

// Tests that a service worker registration is properly stored after extension
// installation, both at the content layer and in the cached state in the
// extensions layer.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       ServiceWorkerIsProperlyRegisteredAfterInstallation) {
  static constexpr char kManifest[] =
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackground[] = "// Blank";

  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  const Extension* extension = LoadExtension(
      extension_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);

  base::Version stored_version =
      task_queue->RetrieveRegisteredServiceWorkerVersion(extension->id());
  ASSERT_TRUE(stored_version.IsValid());
  EXPECT_EQ("0.1", stored_version.GetString());
  EXPECT_EQ(content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER,
            GetServiceWorkerRegistrationState(*extension));
}

// Tests that updating an unpacked extension properly updates the extension's
// service worker.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       UpdatingUnpackedExtensionUpdatesServiceWorker) {
  static constexpr char kManifest[] =
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackgroundV1[] = "self.currentVersion = 1;";
  static constexpr char kBackgroundV2[] =
      R"(self.currentVersion = 2;
         chrome.test.sendMessage('ready');)";

  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundV1);

  const Extension* extension = LoadExtension(
      extension_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  EXPECT_EQ(mojom::ManifestLocation::kUnpacked, extension->location());
  const ExtensionId id = extension->id();

  EXPECT_EQ(base::Value(1), GetVersionFlagFromServiceWorker(id));

  // Unlike `LoadExtension()`, `ReloadExtension()` doesn't automatically wait
  // for the service worker to be ready, so we need to wait for a message to
  // come in signaling it's complete.
  ExtensionTestMessageListener listener("ready");
  // Update the background script file and reload the extension. This results in
  // the extension effectively being updated.
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundV2);
  ReloadExtension(id);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  // Note: `extension` is unsafe to use here since the extension has been
  // reloaded.

  EXPECT_EQ(base::Value(2), GetVersionFlagFromServiceWorker(id));
}

// Tests updating an extension and installing it immediately while it has an
// active new tab page override and a new tab is open.
// Regression test for https://crbug.com/1498035.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       ImmediateUpdateWithNewTabPageOverrideActive) {
  // An extension manifest with a service worker and a new tab page override.
  // The new tab page override is important because:
  // * It commits to the extension origin and can be claimed by the service
  //   worker as a client.
  // * Unlike other chrome-extension:-scheme pages, we don't close the new
  //   tab page when the extension is unloaded, which means the client is
  //   still around when the worker is being re-registered.
  static constexpr char kManifestWithNtpV1[] =
      R"({
         "name": "Extension",
         "manifest_version": 3,
         "version": "0.1",
         "background": {"service_worker": "background.js"},
         "action": {},
         "chrome_url_overrides": {
           "newtab": "page.html"
         }
       })";

  static constexpr char kManifestWithNtpV2[] =
      R"({
         "name": "Extension",
         "manifest_version": 3,
         "version": "0.2",
         "action": {},
         "background": {"service_worker": "background.js"},
         "chrome_url_overrides": {
           "newtab": "page.html"
         }
       })";

  // A background script that sends a message once the service worker is
  // activated.
  constexpr char kBackgroundV1[] =
      R"(self.currentVersion = 1;
         // Wait for the service worker to be active and claim any clients.
         (async () => {
           if (self.serviceWorker.state != 'activated') {
             await new Promise(resolve => {
               self.addEventListener('activate', resolve);
             });
           }
           await clients.claim();
           chrome.test.sendMessage('v1 ready');
         })();)";
  constexpr char kBackgroundV2[] = R"(self.currentVersion = 2;)";

  static constexpr char kPageHtml[] = "<html>This is a page</html>";

  // Write and package the two versions of the extension.
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifestWithNtpV1);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundV1);
  extension_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);

  base::FilePath crx_v1 = extension_dir.Pack("v1.crx");

  extension_dir.WriteManifest(kManifestWithNtpV2);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundV2);
  base::FilePath crx_v2 = extension_dir.Pack("v2.crx");

  // Load the first version of the extension.
  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("v1 ready");
    extension = InstallExtension(crx_v1, 1);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  ASSERT_TRUE(extension);
  EXPECT_EQ(mojom::ManifestLocation::kInternal, extension->location());
  const ExtensionId id = extension->id();
  EXPECT_TRUE(HasActiveServiceWorker(id));

  // Open a new tab. The extension overrides the NTP, so this is the extension's
  // page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_EQ(
      "This is a page",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.body.innerText;"));

  // Verify the service worker is at v1.
  EXPECT_EQ(base::Value(1), GetVersionFlagFromServiceWorker(id));

  {
    // Install v2. This will result in the extension updating. We set
    // `install_immediately` to true so that the system won't wait for the
    // extension to be idle to unload the old version and start the new one
    // (since there's an active NTP that the extension overrides, it would
    // never be idle and it's important for the test case to update the
    // extension while there's an active client of the service worker).
    // This also mimics update behavior if a user clicks "Update" in the
    // chrome://extensions page.
    scoped_refptr<CrxInstaller> crx_installer =
        CrxInstaller::Create(extension_service(), /*prompt=*/nullptr);
    crx_installer->set_error_on_unsupported_requirements(true);
    crx_installer->set_off_store_install_allow_reason(
        CrxInstaller::OffStoreInstallAllowedFromSettingsPage);
    crx_installer->set_install_immediately(true);

    base::test::TestFuture<absl::optional<CrxInstallError>>
        installer_done_future;
    crx_installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const absl::optional<CrxInstallError>&>());

    WorkerInitializedWaiter worker_waiter(id);

    crx_installer->InstallCrx(crx_v2);

    // Wait for the install to finish and for the (new) service worker context
    // to be initialized.
    std::optional<CrxInstallError> install_error = installer_done_future.Get();
    ASSERT_FALSE(install_error.has_value()) << install_error->message();
    worker_waiter.WaitForWorkerContextInitialized();
  }

  // Grab the new version of the extension (the old one was replaced and is
  // unsafe to use).
  extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id);
  ASSERT_TRUE(extension);

  EXPECT_EQ(mojom::ManifestLocation::kInternal, extension->location());
  EXPECT_EQ("0.2", extension->version().GetString());
  EXPECT_EQ(id, extension->id());
  EXPECT_TRUE(HasActiveServiceWorker(id));

  // The service worker context should be that of the new version.
  EXPECT_EQ(base::Value(2), GetVersionFlagFromServiceWorker(id));
}

// Tests that updating an unpacked extension properly updates the extension's
// service worker.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       UpdatingPackedExtensionUpdatesServiceWorker) {
  static constexpr char kManifestV1[] =
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kManifestV2[] =
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.2",
           "background": {"service_worker": "background.js"}
         })";
  // The `InstallExtension()` and `UpdateExtension()` methods don't wait for
  // the service worker to be ready, so each background script needs a message
  // to indicate it's done.
  static constexpr char kBackgroundV1[] =
      R"(self.currentVersion = 1;
         chrome.test.sendMessage('ready');)";
  static constexpr char kBackgroundV2[] =
      R"(self.currentVersion = 2;
         chrome.test.sendMessage('ready');)";

  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifestV1);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundV1);

  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = InstallExtension(extension_dir.Pack(), 1);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    ASSERT_TRUE(extension);
    EXPECT_EQ(mojom::ManifestLocation::kInternal, extension->location());
  }
  const ExtensionId id = extension->id();

  auto get_version_flag = [this, id]() {
    static constexpr char kScript[] =
        R"(chrome.test.sendScriptResult(
               self.currentVersion ? self.currentVersion : -1);)";
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), id, kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  EXPECT_EQ(base::Value(1), get_version_flag());

  // Update the background script file, re-pack the extension, and update the
  // installation. The service worker should remain registered and be properly
  // updated.
  extension_dir.WriteManifest(kManifestV2);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundV2);
  {
    ExtensionTestMessageListener listener("ready");
    extension = UpdateExtension(id, extension_dir.Pack(), 0);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ(mojom::ManifestLocation::kInternal, extension->location());
    EXPECT_EQ("0.2", extension->version().GetString());
    EXPECT_EQ(id, extension->id());
  }

  EXPECT_EQ(base::Value(2), get_version_flag());
}

// Tests that the service worker is properly unregistered when the extension is
// disabled or uninstalled.
// TODO(crbug.com/1446468): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerRegistrationApiTest,
    DISABLED_DisablingOrUninstallingAnExtensionUnregistersTheServiceWorker) {
  static constexpr char kManifest[] =
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackground[] = "chrome.test.sendMessage('ready');";

  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  // `LoadExtension()` waits for the service worker to be ready; no need to
  // listen to the "ready" message.
  const Extension* extension = LoadExtension(
      extension_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  // Disable the extension. The service worker should be unregistered.
  DisableExtension(extension->id());
  EXPECT_EQ(content::ServiceWorkerCapability::NO_SERVICE_WORKER,
            GetServiceWorkerRegistrationState(*extension));

  // Re-enable the extension. The service worker should be re-registered.
  ExtensionTestMessageListener listener("ready");
  EnableExtension(extension->id());
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ(content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER,
            GetServiceWorkerRegistrationState(*extension));

  // Next, uninstall the extension. The worker should be unregistered again.
  // We need to grab a reference to the extension here so that the object
  // doesn't get deleted.
  scoped_refptr<const Extension> extension_ref = extension;
  UninstallExtension(extension->id());
  EXPECT_EQ(content::ServiceWorkerCapability::NO_SERVICE_WORKER,
            GetServiceWorkerRegistrationState(*extension_ref));
}

// Verifies that a service worker registration associated with an extension's
// manifest cannot be removed via the `chrome.browsingData` API.
// Regression test for https://crbug.com/1392498.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       RegistrationCannotBeRemovedByBrowsingDataAPI) {
  // Load two extensions: one with a service worker-based background context and
  // a second with access to the browsingData API.
  static constexpr char kServiceWorkerManifest[] =
      R"({
           "name": "Service Worker Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kServiceWorkerBackground[] =
      R"(chrome.tabs.onCreated.addListener(tab => {
           chrome.test.sendMessage('received event');
         });)";

  TestExtensionDir service_worker_extension_dir;
  service_worker_extension_dir.WriteManifest(kServiceWorkerManifest);
  service_worker_extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                                         kServiceWorkerBackground);

  static constexpr char kBrowsingDataManifest[] =
      R"({
           "name": "Browsing Data Remover",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["browsingData"]
         })";
  static constexpr char kClearDataJs[] =
      R"(chrome.test.runTests([
           async function clearServiceWorkers() {
             // From the extension's perspective, this call should succeed (it
             // will remove any service workers for extensions that aren't the
             // root-scoped background service worker).
             await chrome.browsingData.removeServiceWorkers(
                 {originTypes: {extension: true}});
             chrome.test.succeed();
           },
         ]);)";

  TestExtensionDir browsing_data_extension_dir;
  browsing_data_extension_dir.WriteManifest(kBrowsingDataManifest);
  browsing_data_extension_dir.WriteFile(
      FILE_PATH_LITERAL("clear_data.html"),
      R"(<html><script src="clear_data.js"></script></html>)");
  browsing_data_extension_dir.WriteFile(FILE_PATH_LITERAL("clear_data.js"),
                                        kClearDataJs);

  const Extension* service_worker_extension =
      LoadExtension(service_worker_extension_dir.UnpackedPath(),
                    {.wait_for_registration_stored = true});
  ASSERT_TRUE(service_worker_extension);

  const Extension* browsing_data_extension =
      LoadExtension(browsing_data_extension_dir.UnpackedPath());
  ASSERT_TRUE(browsing_data_extension);

  auto open_new_tab = [this](const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  };

  // Verify the initial state. The service worker-based extension should have a
  // worker registered...
  EXPECT_EQ(content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER,
            GetServiceWorkerRegistrationState(*service_worker_extension));

  const GURL about_blank("about:blank");

  // ... And the worker should be able to receive incoming events.
  {
    ExtensionTestMessageListener listener("received event");
    open_new_tab(about_blank);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Open a page to the browsing data extension, which will trigger a call to
  // the browsingData API to remove registered service workers for extensions.
  {
    ResultCatcher result_catcher;
    open_new_tab(browsing_data_extension->GetResourceURL("clear_data.html"));
    EXPECT_TRUE(result_catcher.GetNextResult());
  }

  // The removal above should *not* have resulted in the background service
  // worker for the extension being removed (which would put the extension into
  // a broken state). The only way to remove a service worker from an extension
  // manifest is to uninstall the extension.
  // The worker should still be registered, and should still receive new events.
  EXPECT_EQ(content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER,
            GetServiceWorkerRegistrationState(*service_worker_extension));

  {
    ExtensionTestMessageListener listener("received event");
    open_new_tab(about_blank);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }
}

// Tests that modifying local files for an unpacked extension does not result
// in the service worker being seen as "updated" (which would result in a
// "waiting" service worker, violating expectations in the extensions system).
// https://crbug.com/1271154.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       ModifyingLocalFilesForUnpackedExtensions) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const double kUpdateDelayInMilliseconds =
      content::ServiceWorkerContext::GetUpdateDelay().InMillisecondsF();
  // Assert that whatever our update delay is, it's less than 5 seconds. If it
  // were more, the test would risk timing out. If we ever need to exceed this
  // in practice, we could introduce a test setter for a different amount of
  // time.
  ASSERT_GE(5000, kUpdateDelayInMilliseconds);

  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["storage"]
         })";
  static constexpr char kBackgroundTemplate[] =
      R"(chrome.storage.local.onChanged.addListener((changes) => {
           // Send a notification of the storage changing back to C++ after
           // a delay long enough for the update check on the worker to trigger.
           // This notification includes the "version" of the background script
           // and the value of the storage bit.
           setTimeout(() => {
             chrome.test.sendScriptResult(
                 `storage changed version %d: count ${changes.count.newValue}`);
            }, %f + 100);
         });)";
  // The following is a page that, when visited, sets a new (incrementing)
  // value in the extension's storage. This should trigger the listener in the
  // background service worker.
  static constexpr char kPageHtml[] =
      R"(<html><script src="page.js"></script></html>)";
  static constexpr char kPageJs[] =
      R"((async () => {
           let {count} = await chrome.storage.local.get({count: 0});
           ++count;
           await chrome.storage.local.set({count});
         })();)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundTemplate, 1, kUpdateDelayInMilliseconds));
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);

  // Load the test extension. It's important it be unpacked, since packed
  // extensions would normally be subject to content verification.
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  EXPECT_EQ(extension->path(), test_dir.UnpackedPath());
  EXPECT_EQ(mojom::ManifestLocation::kUnpacked, extension->location());

  const GURL page_url = extension->GetResourceURL("page.html");
  auto open_tab_and_get_result = [this, page_url]() {
    ScriptResultQueue result_queue;
    // Open the page in a new tab. We use a new tab here since any tabs open to
    // an extension page will be closed later in the test when the extension
    // reloads, and we need to make sure there's at least one tab left in the
    // browser.
    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    return result_queue.GetNextResult();
  };

  // Visit the page. The service worker listener should fire the first time.
  EXPECT_EQ("storage changed version 1: count 1", open_tab_and_get_result());

  // Stop the service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  // Verify any pending tasks from stopping fully finish.
  base::RunLoop().RunUntilIdle();

  // Rewrite the extension service worker and update the "version" flag in the
  // background service worker.
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundTemplate, 2, kUpdateDelayInMilliseconds));

  // Visit the page again. This should reawaken the extension service worker.
  EXPECT_EQ("storage changed version 1: count 2", open_tab_and_get_result());

  // Run any pending tasks. This ensures that the update check, if one were
  // going to happen, does.
  content::RunAllTasksUntilIdle();

  // Visit a third time. As above, the old version of the worker should be
  // running.
  EXPECT_EQ("storage changed version 1: count 3", open_tab_and_get_result());

  // Reload the extension from disk.
  ExtensionId extension_id = extension->id();
  ReloadExtension(extension->id());
  extension = extension_registry()->enabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  ExtensionBackgroundPageWaiter(profile(), *extension)
      .WaitForBackgroundInitialized();

  // Visit the page a fourth time. Now, the new service worker file should
  // be used, since the extension was reloaded from disk.
  EXPECT_EQ("storage changed version 2: count 4", open_tab_and_get_result());
}

}  // namespace extensions
