// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// Tests related to the installation and update of service worker-based
// extensions.
using ServiceWorkerInstallationBrowserTest = ExtensionBrowserTest;

// Tests that extensions are not updated while the service worker is running.
IN_PROC_BROWSER_TEST_F(ServiceWorkerInstallationBrowserTest,
                       PRE_ExtensionsAreNotUpdatedWhileWorkerIsActive) {
  // A script which will keep the service worker active by repeatedly calling
  // API functions.
  static constexpr char kScriptV1[] =
      R"(self.version = 1;
         self.timeout = setTimeout(async () => {
           if (self.forceActive) {
             await chrome.tabs.query({});
           }
         });
         chrome.test.sendMessage('v1 ready');)";
  static constexpr char kScriptV2[] =
      R"(self.version = 2;
         chrome.test.sendMessage('v2 ready');)";
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 3,
           "version": "%d",
           "background": {"service_worker": "background.js"}
         })";
  // Create two packed versions of the extension.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(kManifest, 1));
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kScriptV1);
  base::FilePath v1_crx = test_dir.Pack("v1.crx");

  test_dir.WriteManifest(base::StringPrintf(kManifest, 2));
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kScriptV2);
  base::FilePath v2_crx = test_dir.Pack("v2.crx");

  // Install version 1.
  ExtensionTestMessageListener v1_ready("v1 ready");
  const Extension* extension_v1 =
      InstallExtension(v1_crx, /*expected_change=*/1);
  ASSERT_TRUE(extension_v1);
  const ExtensionId id = extension_v1->id();
  ASSERT_TRUE(v1_ready.WaitUntilSatisfied());

  // There should be one running worker.
  ProcessManager* process_manager = ProcessManager::Get(profile());
  std::vector<WorkerId> worker_ids =
      process_manager->GetServiceWorkersForExtension(id);
  ASSERT_EQ(1u, worker_ids.size());
  WorkerId worker_id_v1 = worker_ids[0];

  // Update the extension to version 2, but don't force it to install
  // immediately.
  const Extension* extension_v2 =
      UpdateExtensionWaitForIdle(id, v2_crx, /*expected_change=*/0);

  // The extension should have installed. Wait for all systems to be notified
  // and any pending tasks to complete.
  ASSERT_TRUE(extension_v2);
  base::RunLoop().RunUntilIdle();

  // The *active* version of the extension should still be version 1, since the
  // extension was not idle at the time of the installation. Verify this with
  // the extension version...
  const Extension* active_extension =
      extension_registry()->enabled_extensions().GetByID(id);
  ASSERT_TRUE(active_extension);
  EXPECT_EQ("1", active_extension->version().GetString());

  // ... the worker id...
  worker_ids = process_manager->GetServiceWorkersForExtension(id);
  ASSERT_EQ(1u, worker_ids.size());
  WorkerId active_worker_id = worker_ids[0];
  EXPECT_EQ(worker_id_v1, active_worker_id);

  // ... and the script context for the worker.
  static constexpr char kGetVersion[] =
      "chrome.test.sendScriptResult(self.version);";
  EXPECT_EQ(1, BackgroundScriptExecutor::ExecuteScript(
                   profile(), id, kGetVersion,
                   BackgroundScriptExecutor::ResultCapture::kSendScriptResult));

  // Test continues below.
}
IN_PROC_BROWSER_TEST_F(ServiceWorkerInstallationBrowserTest,
                       ExtensionsAreNotUpdatedWhileWorkerIsActive) {
  // Now, the browser has restarted. Find our old test extension.
  scoped_refptr<const Extension> extension = nullptr;
  for (const auto& ext : extension_registry()->enabled_extensions()) {
    if (ext->name() == "Test Extension") {
      extension = ext;
      break;
    }
  }
  ASSERT_TRUE(extension);

  // The new version of the extension will be installed on startup. This may
  // or may not have happened by the time we finish test setup. Check the
  // version, and wait for the new version to be installed if it hasn't been.
  if (extension->version().GetString() == "1") {
    TestExtensionRegistryObserver test_observer(extension_registry(),
                                                extension->id());
    extension = test_observer.WaitForExtensionInstalled();
  }

  EXPECT_EQ("2", extension->version().GetString());
  ExtensionId id = extension->id();

  // Version 2's service worker may not be active yet; wait for it if needed.
  ProcessManager* process_manager = ProcessManager::Get(profile());
  std::vector<WorkerId> worker_ids =
      process_manager->GetServiceWorkersForExtension(id);
  if (worker_ids.empty()) {
    ASSERT_TRUE(ExtensionTestMessageListener("v2 ready").WaitUntilSatisfied());
  }
  worker_ids = process_manager->GetServiceWorkersForExtension(id);
  ASSERT_EQ(1u, worker_ids.size());

  // Verify the new service worker script took over.
  static constexpr char kGetVersion[] =
      "chrome.test.sendScriptResult(self.version);";
  EXPECT_EQ(2, BackgroundScriptExecutor::ExecuteScript(
                   profile(), id, kGetVersion,
                   BackgroundScriptExecutor::ResultCapture::kSendScriptResult));
}

}  // namespace extensions
