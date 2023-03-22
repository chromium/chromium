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
#include "extensions/browser/service_worker_task_queue.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

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

  auto get_version_flag = [this, id]() {
    static constexpr char kScript[] =
        R"(chrome.test.sendScriptResult(
               self.currentVersion ? self.currentVersion : -1);)";
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), id, kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  EXPECT_EQ(base::Value(1), get_version_flag());

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

  EXPECT_EQ(base::Value(2), get_version_flag());
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
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerRegistrationApiTest,
    DisablingOrUninstallingAnExtensionUnregistersTheServiceWorker) {
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

}  // namespace extensions
