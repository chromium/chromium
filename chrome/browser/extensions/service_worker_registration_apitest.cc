// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/script_result_queue.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

using service_worker_test_utils::TestServiceWorkerTaskQueueObserver;

namespace {

constexpr char kNTPTestExtensionId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

enum class BackgroundType {
  kPersistentPage,
  kLazyPage,
};

// Convenience method for checking true/false counts of boolean histograms.
void CheckBooleanHistogramCounts(const char* histogram_name,
                                 int true_count,
                                 int false_count,
                                 base::HistogramTester& histogram_tester) {
  histogram_tester.ExpectBucketCount(histogram_name,
                                     /*sample=*/true,
                                     /*expected_count=*/true_count);
  histogram_tester.ExpectBucketCount(histogram_name,
                                     /*sample=*/false,
                                     /*expected_count=*/false_count);
}

GURL new_tab_url() {
  return GURL("chrome://newtab");
}

// Allows test to wait for the attempt to register and unregister a worker to
// complete. It does not guarantee (un)registration attempt success.
class ExtensionRegistrationAndUnregistrationWaiter
    : public ServiceWorkerTaskQueue::TestObserver {
 public:
  explicit ExtensionRegistrationAndUnregistrationWaiter(
      const ExtensionId extension_id)
      : expected_extension_id_(extension_id) {
    ServiceWorkerTaskQueue::SetObserverForTest(this);
  }

  ~ExtensionRegistrationAndUnregistrationWaiter() override {
    ServiceWorkerTaskQueue::SetObserverForTest(nullptr);
  }
  ExtensionRegistrationAndUnregistrationWaiter(
      const ExtensionRegistrationAndUnregistrationWaiter&) = delete;
  ExtensionRegistrationAndUnregistrationWaiter& operator=(
      const ExtensionRegistrationAndUnregistrationWaiter&) = delete;

  void WaitForWorkerRegistrationAttemptCompleted() {
    SCOPED_TRACE("Waiting for worker registration attempt to complete");
    registration_attempt_runloop.Run();
  }

  void WaitForWorkerRegistrationAndUnRegistrationAttemptCompleted() {
    WaitForWorkerRegistrationAttemptCompleted();
    WaitForWorkerUnregistrationAttemptCompleted();
  }

 private:
  void WaitForWorkerUnregistrationAttemptCompleted() {
    SCOPED_TRACE("Waiting for worker unregistration attempt to complete");
    unregistration_attempt_runloop.Run();
  }

  void OnWorkerRegistered(const ExtensionId& extension_id) override {
    if (extension_id == expected_extension_id_) {
      registration_attempt_runloop.Quit();
    }
  }

  void WorkerUnregistered(const ExtensionId& extension_id) override {
    if (extension_id == expected_extension_id_) {
      unregistration_attempt_runloop.Quit();
    }
  }

  const ExtensionId expected_extension_id_;
  base::RunLoop registration_attempt_runloop;
  base::RunLoop unregistration_attempt_runloop;
};

}  // namespace

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

  // Returns the value of `self.currentVersion` in the background context of the
  // extension with the given `extension_id`.
  int GetVersionFlagFromBackgroundContext(const ExtensionId& extension_id) {
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

// Tests that when an extension has invalid syntax in it's background worker
// script the registration is not considered a failure due to it being user
// error.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       InvalidWorkerSyntaxFailsWorkerRegistration) {
  const ExtensionId test_extension_id("iegclhlplifhodhkoafiokenjoapiobj");
  static constexpr const char kKey[] =
      "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjzv7dI7Ygyh67VHE1DdidudpYf8P"
      "Ffv8iucWvzO+3xpF/Dm5xNo7aQhPNiEaNfHwJQ7lsp4gc+C+4bbaVewBFspTruoSJhZc5uEf"
      "qxwovJwN+v1/SUFXTXQmQBv6gs0qZB4gBbl4caNQBlqrFwAMNisnu1V6UROna8rOJQ90D7Nv"
      "7TCwoVPKBfVshpFjdDOTeBg4iLctO3S/06QYqaTDrwVceSyHkVkvzBY6tc6mnYX0RZu78J9i"
      "L8bdqwfllOhs69cqoHHgrLdI6JdOyiuh6pBP6vxMlzSKWJ3YTNjaQTPwfOYaLMuzdl0v+Ydz"
      "afIzV9zwe4Xiskk+5JNGt8b2rQIDAQAB";
  static constexpr char kManifest[] =
      R"({
           "name": "TestExtension",
           "manifest_version": 3,
           "version": "0.1",
           "key": "%s",
           "background": {"service_worker": "background.js"}
         })";
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(base::StringPrintf(kManifest, kKey));
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                          "invalid_js_syntax;");

  base::HistogramTester histogram_tester;
  ExtensionRegistrationAndUnregistrationWaiter registration_waiter(
      test_extension_id);

  ChromeTestExtensionLoader(profile()).LoadUnpackedExtensionAsync(
      extension_dir.UnpackedPath(), base::DoNothing());

  {
    SCOPED_TRACE("waiting for extension registration to finish");
    registration_waiter.WaitForWorkerRegistrationAttemptCompleted();
  }

  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.Registration_FailStatus",
      /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "Extensions.ServiceWorkerBackground.Registration_FailStatus",
      /*sample=*/blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed,
      /*expected_count=*/0);
}

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

  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.RegistrationTime",
      /*expected_count=*/1);
  // Confirm the time is a sane value (less than the maximum value).
  histogram_tester.ExpectBucketCount(
      "Extensions.ServiceWorkerBackground.RegistrationTime",
      /*sample=*/base::Seconds(10).InMilliseconds(),
      /*expected_count=*/0);
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

  EXPECT_EQ(base::Value(1), GetVersionFlagFromBackgroundContext(id));

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

  EXPECT_EQ(base::Value(2), GetVersionFlagFromBackgroundContext(id));
}

// Test what happens when installing/loading an extension, but then before the
// install (and worker registration) completes disable the extension (which
// tries to unregister the worker). Observes a failed registration and
// unregistration.
IN_PROC_BROWSER_TEST_F(ServiceWorkerRegistrationApiTest,
                       AttemptInstallAndImmediateDisable) {
  const ExtensionId test_extension_id("iegclhlplifhodhkoafiokenjoapiobj");
  static constexpr const char kKey[] =
      "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjzv7dI7Ygyh67VHE1DdidudpYf8P"
      "Ffv8iucWvzO+3xpF/Dm5xNo7aQhPNiEaNfHwJQ7lsp4gc+C+4bbaVewBFspTruoSJhZc5uEf"
      "qxwovJwN+v1/SUFXTXQmQBv6gs0qZB4gBbl4caNQBlqrFwAMNisnu1V6UROna8rOJQ90D7Nv"
      "7TCwoVPKBfVshpFjdDOTeBg4iLctO3S/06QYqaTDrwVceSyHkVkvzBY6tc6mnYX0RZu78J9i"
      "L8bdqwfllOhs69cqoHHgrLdI6JdOyiuh6pBP6vxMlzSKWJ3YTNjaQTPwfOYaLMuzdl0v+Ydz"
      "afIzV9zwe4Xiskk+5JNGt8b2rQIDAQAB";
  static constexpr char kManifest[] =
      R"({
           "name": "TestExtension",
           "manifest_version": 3,
           "version": "0.1",
           "key": "%s",
           "background": {"service_worker": "background.js"}
         })";
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(base::StringPrintf(kManifest, kKey));
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");

  // Load the extension, but then disable it as soon as possible.
  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  ExtensionRegistrationAndUnregistrationWaiter extension_registration_waiter(
      test_extension_id);
  base::HistogramTester histogram_tester;
  std::optional<base::UnguessableToken> activation_token;
  ChromeTestExtensionLoader(profile()).LoadUnpackedExtensionAsync(
      extension_dir.UnpackedPath(),
      base::BindLambdaForTesting([&](const extensions::Extension* extension) {
        ASSERT_TRUE(extension);
        // Confirm that an activation token was generated so we can later check
        // for it to be removed.
        activation_token =
            task_queue->GetCurrentActivationToken(extension->id());
        ASSERT_TRUE(activation_token.has_value());

        // Disable the extension as soon as it's been added as enabled,
        // which causes the worker unregistration request to be sent.
        DisableExtension(extension->id());
      }));
  extension_registration_waiter
      .WaitForWorkerRegistrationAndUnRegistrationAttemptCompleted();

  // Registration considered success because we didn't allow the registration to
  // complete before we disabled the extension. We can't check the failure
  // status code because it's not consistent. I've seen
  // blink::ServiceWorkerStatusCode::kErrorDisallowed, kErrorNetwork, and
  // kErrorNotFound in manual testing.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);

  // Unregistration "succeeds"  because the registration above did not complete.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus",
      /*expected_count=*/0);

  // Confirm the activation token doesn't remain after disabling the
  // failed-to-register extension.
  activation_token = task_queue->GetCurrentActivationToken(test_extension_id);
  EXPECT_FALSE(activation_token.has_value());
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
  EXPECT_EQ(base::Value(1), GetVersionFlagFromBackgroundContext(id));

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

    base::test::TestFuture<std::optional<CrxInstallError>>
        installer_done_future;
    crx_installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const std::optional<CrxInstallError>&>());

    TestServiceWorkerTaskQueueObserver worker_waiter;

    crx_installer->InstallCrx(crx_v2);

    // Wait for the install to finish and for the (new) service worker context
    // to be initialized.
    std::optional<CrxInstallError> install_error = installer_done_future.Get();
    ASSERT_FALSE(install_error.has_value()) << install_error->message();
    worker_waiter.WaitForWorkerContextInitialized(id);
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
  EXPECT_EQ(base::Value(2), GetVersionFlagFromBackgroundContext(id));
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
// TODO(crbug.com/40268625): Flaky on multiple platforms.
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

class ServiceWorkerExtensionUpdateOnBrowserRestartRegistrationApiTest
    : public ServiceWorkerRegistrationApiTest {
 protected:
  void SetUp() override {
    // Usually browser test loads about:blank in the default tab for the window,
    // but we want to ensure chrome://newtab is loaded instead. This ensures
    // that chrome://newtab is loaded as soon as possible which is important to
    // confirm the update works as expected.
    set_open_about_blank_on_browser_launch(false);

    // Create the observer now because the browser will be started after we call
    // `ServiceWorkerRegistrationApiTest::SetUp()`.
    browser_start_new_tab_observer_ =
        std::make_unique<ui_test_utils::UrlLoadObserver>(new_tab_url());

    ServiceWorkerRegistrationApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ServiceWorkerRegistrationApiTest::SetUpOnMainThread();

    {
      SCOPED_TRACE(
          "waiting for the initial new tab to open after browser start");
      browser_start_new_tab_observer_->Wait();
    }
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* main_parts) override {
    // These are meant to be used in `NTPUpdateAcrossBrowserRestart` (after
    // PRE_NTPUpdateAcrossBrowserRestart finishes). But we need them to be
    // created before the browser starts in the test so we define them here.
    v2_install_listener_ =
        std::make_unique<ExtensionTestMessageListener>("v2 installed");
    v2_update_histogram_tester_ = std::make_unique<base::HistogramTester>();
    ServiceWorkerRegistrationApiTest::CreatedBrowserMainParts(main_parts);
  }

  void TearDownOnMainThread() override {
    ServiceWorkerRegistrationApiTest::TearDownOnMainThread();

    // Prevent dangling pointer on test teardown.
    browser_start_new_tab_observer_.reset();
  }

  // Ensure any new tab that is opened defaults goes to chrome://newtab.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ServiceWorkerRegistrationApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kHomePage,
                                    chrome::kChromeUINewTabURL);
  }

  // Get the NTP javascript's version.
  content::EvalJsResult GetVersionOfNTPScript() {
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "self.currentVersion;");
  }

  // Request the version of the background context script from the perspective
  // of the NTP js.
  content::EvalJsResult GetBackgroundContextVersionFromNTPPage() {
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "getCurrentVersionOfBackgroundContext();");
  }

  // Observes that chrome://newtab loads on test start.
  std::unique_ptr<ui_test_utils::UrlLoadObserver>
      browser_start_new_tab_observer_;

  std::unique_ptr<ExtensionTestMessageListener> v2_install_listener_;
  std::unique_ptr<base::HistogramTester> v2_update_histogram_tester_;
};

// Tests that updating an extension that overrides the NTP updates successfully
// across browser restart when the updated is delayed until shutdown. This PRE_
// test installs v1, updates to v2, but v2 does not install since v1 is not
// idle. At the end of the test the browser shuts down with the v2 update as a
// delayed install.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerExtensionUpdateOnBrowserRestartRegistrationApiTest,
    PRE_NTPUpdateAcrossBrowserRestart) {
  // Browser should have started and opened a new tab to chrome://newtab.

  // Install v1 of the extension and verify its background context is running
  // the v1 version.
  base::FilePath crx_v1_path = PackExtension(test_data_dir_.AppendASCII(
      "service_worker/registration/ntp_extension/extension_version_1"));
  const Extension* extension_v1 = nullptr;
  {
    ExtensionTestMessageListener v1_install_listener_("v1 installed");
    extension_v1 = InstallExtension(crx_v1_path, /*expected_change=*/1);
    SCOPED_TRACE("waiting for version 1 of the extension to install");
    ASSERT_TRUE(v1_install_listener_.WaitUntilSatisfied());
    ASSERT_EQ("1", extension_v1->version().GetString());
  }
  ASSERT_TRUE(extension_v1);
  ASSERT_EQ(kNTPTestExtensionId, extension_v1->id());
  EXPECT_TRUE(HasActiveServiceWorker(extension_v1->id()));
  ASSERT_EQ(base::Value(1),
            GetVersionFlagFromBackgroundContext(extension_v1->id()));

  // Navigate current tab to new tab to engage v1 of the NTP extension to stay
  // non-idle.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_tab_url()));

  // Verify v1 of extension is responding to messages in the tab.
  std::u16string first_new_tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &first_new_tab_title);
  ASSERT_EQ(u"Custom NTP test v1", first_new_tab_title);

  ASSERT_EQ(base::Value(1), GetVersionOfNTPScript());
  {
    SCOPED_TRACE("waiting for v1 of background context to respond to NTP");
    ASSERT_EQ(base::Value(1), GetBackgroundContextVersionFromNTPPage());
  }

  // v1 of extension is still installed because extension is not idle.
  ASSERT_TRUE(HasActiveServiceWorker(extension_v1->id()));
  ASSERT_EQ(base::Value(1),
            GetVersionFlagFromBackgroundContext(extension_v1->id()));

  // Attempt install of v2 of the extension.
  base::FilePath crx_v2_path = PackExtension(test_data_dir_.AppendASCII(
      "service_worker/registration/ntp_extension/extension_version_2"));
  const Extension* extension_update = nullptr;
  {
    SCOPED_TRACE("waiting for update of v2 of extension");
    extension_update =
        UpdateExtensionWaitForIdle(kNTPTestExtensionId, crx_v2_path,
                                   /*expected_change=*/0);
  }
  ExtensionService* service = extension_service();
  ASSERT_TRUE(service);
  ASSERT_EQ(1u, service->delayed_installs()->size());
  // v2 won't install though since v1 isn't idle (NTP page is still open) yet so
  // we're given the original `extension_v1` object.
  ASSERT_TRUE(extension_update);
  ASSERT_EQ(extension_update, extension_v1);
  // Confirm v1 background context is still installed because extension is not
  // idle.
  ASSERT_TRUE(HasActiveServiceWorker(extension_v1->id()));
  ASSERT_EQ(base::Value(1),
            GetVersionFlagFromBackgroundContext(extension_v1->id()));

  // Confirm the existing NTP we opened is still responding as v1 of the
  // extension.
  std::u16string second_new_tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &second_new_tab_title);
  ASSERT_EQ(u"Custom NTP test v1", second_new_tab_title);
  ASSERT_EQ(base::Value(1), GetVersionOfNTPScript());
  {
    SCOPED_TRACE("waiting for v1 of background context to respond to NTP");
    ASSERT_EQ(base::Value(1), GetBackgroundContextVersionFromNTPPage());
  }

  // Navigate again to new tab so we can confirm v1 is still running and v2
  // hasn't taken over future new tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_tab_url()));
  std::u16string third_new_tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &third_new_tab_title);
  ASSERT_EQ(u"Custom NTP test v1", third_new_tab_title);
  ASSERT_EQ(base::Value(1), GetVersionOfNTPScript());
  {
    SCOPED_TRACE("waiting for v1 of background context to respond to NTP");
    ASSERT_EQ(base::Value(1), GetBackgroundContextVersionFromNTPPage());
  }

  // Shut down the browser (test ending causes browser to shut down).
}

// TODO(https://crbug.com/330066242): Flakes on chromeOS.
// ui_test_utils::NavigateToURL() causes: [focus_controller.cc(277)] Check
// failed: rules_->CanFocusWindow(window, nullptr).
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_NTPUpdateAcrossBrowserRestart \
  DISABLED_NTPUpdateAcrossBrowserRestart
#else
#define MAYBE_NTPUpdateAcrossBrowserRestart NTPUpdateAcrossBrowserRestart
#endif

// Continues the PRE_ test by testing that upon browser start v2 of the
// extension is installed.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerExtensionUpdateOnBrowserRestartRegistrationApiTest,
    MAYBE_NTPUpdateAcrossBrowserRestart) {
  // Browser should have started and opened a new tab to chrome://newtab.

  {
    SCOPED_TRACE(
        "waiting for install of extension to v2 after browser restart");
    // Verify new extension background context is now active installed.
    EXPECT_TRUE(v2_install_listener_->WaitUntilSatisfied());
  }

  // Confirm that v2 of extension is now installed.
  const Extension* extension_v2 =
      ExtensionRegistry::Get(profile())->GetExtensionById(
          kNTPTestExtensionId, ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension_v2);
  ASSERT_EQ("2", extension_v2->version().GetString());
  EXPECT_EQ(base::Value(2),
            GetVersionFlagFromBackgroundContext(extension_v2->id()));

  // Verify that v2 of extension is responding to NTP requests.
  std::u16string browser_start_new_tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &browser_start_new_tab_title);
  ASSERT_EQ(u"Custom NTP test v2", browser_start_new_tab_title);
  EXPECT_EQ(base::Value(2), GetVersionOfNTPScript());
  {
    SCOPED_TRACE("waiting for v2 of background context to respond to NTP");
    ASSERT_EQ(base::Value(2), GetBackgroundContextVersionFromNTPPage());
  }

  // Navigate to new tab page so we can confirm v2 is still running and v1
  // hasn't taken over future new tabs loads.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_tab_url()));
  std::u16string new_tab_title;
  ui_test_utils::GetCurrentTabTitle(browser(), &new_tab_title);
  ASSERT_EQ(u"Custom NTP test v2", new_tab_title);
  ASSERT_EQ(base::Value(2), GetVersionOfNTPScript());
  {
    SCOPED_TRACE("waiting for v2 of background context to respond to NTP");
    ASSERT_EQ(base::Value(2), GetBackgroundContextVersionFromNTPPage());
  }

  // TODO(crbug.com/353738494): Let's make it so that vN of extension is not
  // redundantly installed on start when delayed install of vN+1 is ready.

  // v1 worker is unregistered twice because on browser start v1 of extension is
  // installed first, then delayed installs are installed. This causes us to
  // deactivate v1 of the extension since it has a worker task queue (which
  // unregisters v1 of the worker), then we unregister again because v1 of the
  // extension is loaded.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState",
      /*true_count=*/2, /*false_count=*/0, *v2_update_histogram_tester_);
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "DeactivateExtension",
      /*true_count=*/1, /*false_count=*/0, *v2_update_histogram_tester_);
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "AddExtension",
      /*true_count=*/1, /*false_count=*/0, *v2_update_histogram_tester_);

  // Then v2 extension worker is registered.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState",
      /*true_count=*/1, /*false_count=*/0, *v2_update_histogram_tester_);
  v2_update_histogram_tester_->ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.Registration_FailStatus",
      /*expected_count=*/0);
}

// Registration and unregistration metrics tests.

// TODO(crbug.com/346732739): Add tests for extension updates from:
//   * non-sw background to sw background
//   * sw registered manually via web API to sw background
//   * sw background context to sw background context

class ServiceWorkerManifestVersionBrowserTest
    : public ExtensionBrowserTest,
      public testing::WithParamInterface<int> {
 public:
  void InstallMv2OrMv3Extension() {
    const char* test_extension_subpath;
    if (GetParam() == 2) {
      test_extension_subpath = "service_worker/registration/mv2_service_worker";
    } else if (GetParam() == 3) {
      test_extension_subpath = "service_worker/registration/mv3_service_worker";
    } else {
      FAIL() << "Invalid test parameter: \"" << GetParam()
             << "\" manifest version must be 2 or 3.";
    }
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII(test_extension_subpath),
                      {.wait_for_registration_stored = true});
    ASSERT_TRUE(extension);
    extension_ = extension;
  }

  void ReleaseExtension() { extension_ = nullptr; }

  const Extension* extension() const { return extension_.get(); }

 protected:
  void TearDownOnMainThread() override {
    extension_ = nullptr;
    ExtensionBrowserTest::TearDownOnMainThread();
  }

 private:
  raw_ptr<const Extension> extension_;
};

using ServiceWorkerRegistrationInstallMetricBrowserTest =
    ServiceWorkerManifestVersionBrowserTest;

// Tests that installing an extension emits metrics for registering the service
// worker.
IN_PROC_BROWSER_TEST_P(ServiceWorkerRegistrationInstallMetricBrowserTest,
                       ExtensionInstall) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(InstallMv2OrMv3Extension());

  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.Registration_FailStatus",
      /*expected_count=*/0);
}

// Tracks when a worker is registered in the //content layer.
class ServiceWorkerTaskQueueRegistrationObserver
    : public ServiceWorkerTaskQueue::TestObserver {
 public:
  explicit ServiceWorkerTaskQueueRegistrationObserver(
      const ExtensionId& extension_id)
      : extension_id_(extension_id) {}

  void WaitForWorkerUnregistered() { unregister_loop.Run(); }
  void WaitForWorkerRegistered() { register_loop.Run(); }

 private:
  void WorkerUnregistered(const ExtensionId& extension_id) override {
    if (extension_id == extension_id_) {
      unregister_loop.Quit();
    }
  }

  void OnWorkerRegistered(const ExtensionId& extension_id) override {
    if (extension_id == extension_id_) {
      register_loop.Quit();
    }
  }

  ExtensionId extension_id_;
  base::RunLoop unregister_loop;
  base::RunLoop register_loop;
};

// Tests that uninstalling an extension emits metrics for unregistering the
// service worker.
IN_PROC_BROWSER_TEST_P(ServiceWorkerRegistrationInstallMetricBrowserTest,
                       ExtensionUninstall) {
  ASSERT_NO_FATAL_FAILURE(InstallMv2OrMv3Extension());

  base::HistogramTester histogram_tester;
  // Uninstall extension and wait for the unregistration metrics to have been
  // emitted.
  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  ServiceWorkerTaskQueueRegistrationObserver register_observer(
      extension()->id());
  task_queue->SetObserverForTest(&register_observer);
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  const ExtensionId extension_id = extension()->id();
  // Uninstalling frees `extension_` so we must free it here to prevent dangling
  // ptr between the uninstall and until the test is torn down.
  ReleaseExtension();
  system->extension_service()->UninstallExtension(
      extension_id, UNINSTALL_REASON_FOR_TESTING, nullptr);
  {
    SCOPED_TRACE(
        "waiting for worker to be unregistered after uninstalling extension");
    register_observer.WaitForWorkerUnregistered();
  }

  // Expected unregistration metrics for disable.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "DeactivateExtension",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus_"
      "DeactivateExtension",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus_"
      "AddExtension",
      /*expected_count=*/0);

  // We didn't update the extension.
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "AddExtension",
      /*expected_count=*/0);
}

using ServiceWorkerRegistrationRestartMetricBrowserTest =
    ServiceWorkerManifestVersionBrowserTest;

// Tests that restarting an extension emits metrics for unregistering and
// registering the service worker.
//
// TODO(crbug.com/349683323): Fix flakiness
IN_PROC_BROWSER_TEST_P(ServiceWorkerRegistrationRestartMetricBrowserTest,
                       DISABLED_ExtensionRestart) {
  ASSERT_NO_FATAL_FAILURE(InstallMv2OrMv3Extension());

  base::HistogramTester histogram_tester;
  // Disable and then re-enable the extension.
  ServiceWorkerTaskQueue* task_queue = ServiceWorkerTaskQueue::Get(profile());
  ASSERT_TRUE(task_queue);
  ServiceWorkerTaskQueueRegistrationObserver register_observer(
      extension()->id());
  task_queue->SetObserverForTest(&register_observer);
  ExtensionSystem* system = ExtensionSystem::Get(profile());

  // Disable extension and wait for worker to be unregistered.
  system->extension_service()->DisableExtension(
      extension()->id(), disable_reason::DISABLE_USER_ACTION);
  {
    SCOPED_TRACE(
        "waiting for worker to be unregistered after disabling extension");
    register_observer.WaitForWorkerUnregistered();
  }

  // Enable extension and wait for registration metric should have been emitted.
  system->extension_service()->EnableExtension(extension()->id());
  {
    SCOPED_TRACE(
        "waiting for worker to be registered after enabling extension");
    register_observer.WaitForWorkerRegistered();
  }

  // Expected unregistration and registration metrics for disable and then
  // enable for restart.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "DeactivateExtension",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus_"
      "DeactivateExtension",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus_"
      "AddExtension",
      /*expected_count=*/0);
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);

  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.Registration_FailStatus",
      /*expected_count=*/0);
  // We didn't update the extension.
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "AddExtension",
      /*expected_count=*/0);
}

class MV2BackgroundsToMV3WorkerRegistrationMetricBrowserTest
    : public ServiceWorkerRegistrationApiTest,
      public testing::WithParamInterface<BackgroundType> {
 protected:
  const char* GetMV2ManifestBackgroundSectionForBackgroundType() {
    switch (GetParam()) {
      case BackgroundType::kLazyPage:
        return R"(
        "background": {
           "scripts": ["background.js"],
           "persistent": false
        }
      )";
      case BackgroundType::kPersistentPage:
        return R"(
        "background": {
           "scripts": ["background.js"],
           "persistent": true
        }
      )";
    }
  }
};

// Tests that MV2 extensions of all background types, when updated, emit the
// metrics for previous worker unregistration and new worker registration.
IN_PROC_BROWSER_TEST_P(MV2BackgroundsToMV3WorkerRegistrationMetricBrowserTest,
                       ExtensionUpdate) {
  static constexpr char kManifestMv2Template[] =
      R"({
         "name": "MV2 extension with non-SW background",
         "version": "1",
         "manifest_version": 2,
         %s
       })";

  static constexpr char kManifestMv3[] =
      R"({
         "name": "MV3 extension with service worker",
         "version": "2",
         "manifest_version": 3,
         "background": {
           "service_worker": "background.js"
         }
       })";

  constexpr char kBackgroundTemplate[] =
      R"(
        self.currentVersion = %d;

        chrome.runtime.onInstalled.addListener((details) => {
          chrome.test.sendMessage('v' + self.currentVersion + ' ready');
        });
      )";

  const char* ManifestMv2BackgroundSection =
      GetMV2ManifestBackgroundSectionForBackgroundType();

  // Write and package the first version of the extension.
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(
      base::StringPrintf(kManifestMv2Template, ManifestMv2BackgroundSection));
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundTemplate, /*self.currentVersion=*/1));
  base::FilePath crx_v1_path = extension_dir.Pack("v1.crx");

  // Install the MV2 extension.
  const Extension* extension_v1 = nullptr;
  {
    ExtensionTestMessageListener listener("v1 ready");
    extension_v1 = InstallExtension(crx_v1_path, /*expected_change=*/1);
    SCOPED_TRACE("waiting for extension to be installed");
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  ASSERT_TRUE(extension_v1);
  EXPECT_EQ("1", extension_v1->version().GetString());
  const ExtensionId extension_v1_id = extension_v1->id();

  // Verify the first version of the extension is at v1.
  EXPECT_EQ(base::Value(1),
            GetVersionFlagFromBackgroundContext(extension_v1_id));

  // Write and package the second version of the extension.
  extension_dir.WriteManifest(kManifestMv3);
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundTemplate, /*self.currentVersion=*/2));
  base::FilePath crx_v2_path = extension_dir.Pack("v2.crx");

  // Update to the second (MV3) version of the extension with a worker.
  const Extension* extension_v2 = nullptr;
  base::HistogramTester histogram_tester;  // Monitors metrics during update.
  {
    ExtensionTestMessageListener listener("v2 ready");
    // `extension_v1` will be unsafe to use after update.
    extension_v2 =
        UpdateExtension(extension_v1_id, crx_v2_path, /*expected_change=*/0);
    SCOPED_TRACE("waiting for extension to be updated");
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  ASSERT_TRUE(extension_v2);
  EXPECT_EQ("2", extension_v2->version().GetString());
  EXPECT_EQ(extension_v1_id, extension_v2->id());
  ASSERT_TRUE(HasActiveServiceWorker(extension_v2->id()));

  // The service worker context should be that of the new version.
  EXPECT_EQ(base::Value(2),
            GetVersionFlagFromBackgroundContext(extension_v2->id()));

  // First the old worker registration is unregistered. It is unregistered
  // twice: once when removing the extension (ServiceWorkerTaskQueue) and then
  // (redundantly) again before adding the new version of the extension. The
  // redundant removal is meant to handle the case where a
  // non-ServiceWorkerTaskQueue-tracked worker is registered for the extension
  // (example: an MV2 extension that registered a worker via the web API).

  // When updating from an MV2 worker we try to unregister the previous worker
  // version first.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "DeactivateExtension",
      /*expected_count=*/0);
  // We unsuccessfully try to unregister it again to handle workers that are
  // registered via the web API. This is an expected failure.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "AddExtension",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus4",
      /*expected_count=*/0);

  // Then the new worker registration is registered.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.Registration_FailStatus",
      /*expected_count=*/0);
}

class WorkerBackgroundToWorkerBackgroundRegistrationMetricTest
    : public ServiceWorkerRegistrationApiTest,
      public testing::WithParamInterface<std::pair<int, int>> {};

// Tests that extensions of either manifest type can update to a worker from a
// previous worker version and emit metrics for unregistering the previous
// worker and registering the new worker version.
IN_PROC_BROWSER_TEST_P(WorkerBackgroundToWorkerBackgroundRegistrationMetricTest,
                       ExtensionUpdate) {
  const char kManifestV1Template[] =
      R"({
         "name": "Version 1 extension with service worker",
         "version": "1",
         "manifest_version": %d,
         "background": {
           "service_worker": "background.js"
         }
       })";

  static constexpr char kManifestV2Template[] =
      R"({
         "name": "Version 2 extension with service worker",
         "version": "2",
         "manifest_version": %d,
         "background": {
           "service_worker": "background.js"
         }
       })";

  constexpr char kBackgroundTemplate[] =
      R"(
        self.currentVersion = %d;

        chrome.runtime.onInstalled.addListener((details) => {
          chrome.test.sendMessage('v' + self.currentVersion + ' ready');
        });
      )";

  // Write and package the first version of the extension.
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(base::StringPrintf(
      kManifestV1Template, /*manifest_version*/ GetParam().first));
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundTemplate, /*self.currentVersion=*/1));
  base::FilePath crx_v1_path = extension_dir.Pack("v1.crx");

  // Install the first version of the extension.
  const Extension* extension_v1 = nullptr;
  {
    ExtensionTestMessageListener listener("v1 ready");
    extension_v1 = InstallExtension(crx_v1_path, /*expected_change=*/1);
    SCOPED_TRACE("waiting for version 1 of the extension to install");
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  ASSERT_TRUE(extension_v1);
  EXPECT_EQ("1", extension_v1->version().GetString());
  const ExtensionId extension_v1_id = extension_v1->id();
  ASSERT_TRUE(HasActiveServiceWorker(extension_v1_id));

  // Verify the service worker is at v1.
  EXPECT_EQ(base::Value(1),
            GetVersionFlagFromBackgroundContext(extension_v1_id));

  // Write and package the first version of the extension.
  extension_dir.WriteManifest(base::StringPrintf(
      kManifestV2Template, /*manifest_version*/ GetParam().second));
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundTemplate, /*self.currentVersion=*/2));
  base::FilePath crx_v2_path = extension_dir.Pack("v2.crx");

  // Update to the second version of the extension.
  const Extension* extension_v2 = nullptr;
  base::HistogramTester histogram_tester;  // Monitors metrics during update.
  {
    ExtensionTestMessageListener listener("v2 ready");
    // `extension_v1` will be unsafe to use after update.
    extension_v2 =
        UpdateExtension(extension_v1_id, crx_v2_path, /*expected_change=*/0);
    SCOPED_TRACE("waiting for updated version 2 of the extension to install");
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  ASSERT_TRUE(extension_v2);
  EXPECT_EQ("2", extension_v2->version().GetString());
  EXPECT_EQ(extension_v1_id, extension_v2->id());
  EXPECT_TRUE(HasActiveServiceWorker(extension_v2->id()));

  // The service worker context should be that of the new version.
  EXPECT_EQ(base::Value(2),
            GetVersionFlagFromBackgroundContext(extension_v2->id()));

  // First the old worker registration is unregistered. It is unregistered
  // twice: once when removing the extension (ServiceWorkerTaskQueue) and then
  // redundantly again (but curiously it succeeds) before adding the new version
  // of the extension.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState",
      /*true_count=*/2, /*false_count=*/0, histogram_tester);
  // And it's unregistered due to the MV2 service worker being deactivated.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "DeactivateExtension",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  // We redundantly attempt to unregister it again to handle workers that are
  // registered via the web API.
  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationState_"
      "AddExtension",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus_"
      "DeactivateExtension",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.WorkerUnregistrationFailureStatus_"
      "AddExtension",
      /*expected_count=*/0);

  CheckBooleanHistogramCounts(
      "Extensions.ServiceWorkerBackground.WorkerRegistrationState",
      /*true_count=*/1, /*false_count=*/0, histogram_tester);
  histogram_tester.ExpectTotalCount(
      "Extensions.ServiceWorkerBackground.Registration_FailStatus",
      /*expected_count=*/0);
}

INSTANTIATE_TEST_SUITE_P(MV2,
                         ServiceWorkerRegistrationInstallMetricBrowserTest,
                         testing::Values(2));
INSTANTIATE_TEST_SUITE_P(MV3,
                         ServiceWorkerRegistrationInstallMetricBrowserTest,
                         testing::Values(3));

INSTANTIATE_TEST_SUITE_P(MV2,
                         ServiceWorkerRegistrationRestartMetricBrowserTest,
                         testing::Values(2));
INSTANTIATE_TEST_SUITE_P(MV3,
                         ServiceWorkerRegistrationRestartMetricBrowserTest,
                         testing::Values(3));

INSTANTIATE_TEST_SUITE_P(MV2EventPageToMV3Worker,
                         MV2BackgroundsToMV3WorkerRegistrationMetricBrowserTest,
                         testing::Values(BackgroundType::kLazyPage));

INSTANTIATE_TEST_SUITE_P(MV2PersistentPageToMV3Worker,
                         MV2BackgroundsToMV3WorkerRegistrationMetricBrowserTest,
                         testing::Values(BackgroundType::kPersistentPage));

INSTANTIATE_TEST_SUITE_P(
    Mv2ToMv2,
    WorkerBackgroundToWorkerBackgroundRegistrationMetricTest,
    testing::Values(std::pair<int, int>(2, 2)));

INSTANTIATE_TEST_SUITE_P(
    Mv2ToMv3,
    WorkerBackgroundToWorkerBackgroundRegistrationMetricTest,
    testing::Values(std::pair<int, int>(2, 3)));

INSTANTIATE_TEST_SUITE_P(
    Mv3ToMv3,
    WorkerBackgroundToWorkerBackgroundRegistrationMetricTest,
    testing::Values(std::pair<int, int>(3, 3)));

}  // namespace extensions
