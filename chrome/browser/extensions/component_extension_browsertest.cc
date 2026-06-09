// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using ComponentExtensionBrowserTest = ExtensionBrowserTest;

#if BUILDFLAG(IS_CHROMEOS)
// Tests that MojoJS is enabled for component extensions that need it.
// Note the test currently only runs for ChromeOS because the test extension
// uses `mojoPrivate` to test and `mojoPrivate` is ChromeOS only.
IN_PROC_BROWSER_TEST_F(ComponentExtensionBrowserTest, MojoJS) {
  ResultCatcher result_catcher;

  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("service_worker/mojo"),
                    {.load_as_component = true});
  ASSERT_TRUE(extension);

  ASSERT_TRUE(result_catcher.GetNextResult());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

constexpr char kExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";
constexpr char kExtensionKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjzv7dI7Ygyh67VHE1DdidudpYf8P"
    "Ffv8iucWvzO+3xpF/Dm5xNo7aQhPNiEaNfHwJQ7lsp4gc+C+4bbaVewBFspTruoSJhZc5uEf"
    "qxwovJwN+v1/SUFXTXQmQBv6gs0qZB4gBbl4caNQBlqrFwAMNisnu1V6UROna8rOJQ90D7Nv"
    "7TCwoVPKBfVshpFjdDOTeBg4iLctO3S/06QYqaTDrwVceSyHkVkvzBY6tc6mnYX0RZu78J9i"
    "L8bdqwfllOhs69cqoHHgrLdI6JdOyiuh6pBP6vxMlzSKWJ3YTNjaQTPwfOYaLMuzdl0v+Ydz"
    "afIzV9zwe4Xiskk+5JNGt8b2rQIDAQAB";

constexpr char kChromeResourcesTestExtensionKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0w8POHuAG0xYalDMJbfxIxAQE+to"
    "cLYpRynAqvu8Ff+5nswm007lKNRPbA8tGuMGWVRIUlNvpBMCDrP7E6khObWfq0GN/NtkKPS7"
    "jQZXZxYEYvhX0vdqNEFdqnaRTAlZoV/LZiEK29lv8s6s0Fr/MZrEPyDC7vQOnqdRrKbJI21g"
    "XNIhYmaCkVyKvypBPMFL5z+G46rLfZFfF7Rw/MfM/LblVacwDFBSmre6rgei5e48jVQ3rzLu"
    "g3LxA81fzEObsCO0KER/qwWSQ8yK2bNRF/En6jwxbp39En8SliK+wbu0SCEJ9/aMqCw5Tpoc"
    "EbxQGW7o+FBup9twXD8oX+csXQIDAQAB";

// Tests updating a Service Worker-based component extension across a restart.
// This simulates a browser update where a component extension might change.
class ComponentExtensionServiceWorkerUpdateBrowserTest
    : public ComponentExtensionBrowserTest {
 public:
  void WriteExtension(TestExtensionDir* dir, int version) {
    constexpr char kManifestTemplate[] =
        R"({
         "name": "Component SW Update Test",
         "manifest_version": 3,
         "version": "%d",
         "background": {"service_worker": "sw.js"},
         "key": "%s"
       })";
    dir->WriteManifest(
        base::StringPrintf(kManifestTemplate, version, kExtensionKey));

    constexpr char kBackgroundScriptTemplate[] =
        R"(self.version = %d;
       chrome.test.sendMessage(`v${self.version} ready`);)";
    dir->WriteFile(FILE_PATH_LITERAL("sw.js"),
                   base::StringPrintf(kBackgroundScriptTemplate, version));
  }

  int GetWorkerVersion(const ExtensionId& id) {
    constexpr char kGetVersionScript[] =
        "chrome.test.sendScriptResult(self.version);";
    base::Value version = ExecuteScriptInBackgroundPage(id, kGetVersionScript);
    if (!version.is_int()) {
      ADD_FAILURE() << "Script did not return an integer. Value: " << version;
      return -1;
    }
    return version.GetInt();
  }

  TestExtensionDir test_dir_v1_;
  TestExtensionDir test_dir_v2_;
};

// PRE_ test: Installs V1 of the component extension. Verifies it runs.
IN_PROC_BROWSER_TEST_F(ComponentExtensionServiceWorkerUpdateBrowserTest,
                       PRE_Update) {
  WriteExtension(&test_dir_v1_, 1);

  // Load V1 of the component extension.
  ExtensionTestMessageListener v1_ready("v1 ready");
  const Extension* extension_v1 =
      LoadExtension(test_dir_v1_.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension_v1);

  // Ensure it has the correct ID and wait for it to start.
  const ExtensionId id = extension_v1->id();
  ASSERT_EQ(kExtensionId, id);
  ASSERT_TRUE(v1_ready.WaitUntilSatisfied());

  // Check service worker version.
  EXPECT_EQ("1", extension_v1->version().GetString());
  EXPECT_EQ(1, GetWorkerVersion(id));
}

// Main test: Installs V2 of the component extension. Verifies V2 runs.
IN_PROC_BROWSER_TEST_F(ComponentExtensionServiceWorkerUpdateBrowserTest,
                       Update) {
  ASSERT_FALSE(
      extension_registry()->enabled_extensions().GetByID(kExtensionId));
  WriteExtension(&test_dir_v2_, 2);

  // Load V2 of the component extension.
  ExtensionTestMessageListener v2_ready("v2 ready");
  const Extension* extension_v2 =
      LoadExtension(test_dir_v2_.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension_v2);

  // Ensure it has the correct ID (same as V1) and wait for it to start.
  const ExtensionId id = extension_v2->id();
  EXPECT_EQ(kExtensionId, id);
  ASSERT_TRUE(v2_ready.WaitUntilSatisfied());

  // Check service worker version.
  EXPECT_EQ("2", extension_v2->version().GetString());
  EXPECT_EQ(2, GetWorkerVersion(id));
}

class ComponentExtensionWorkerChromeResourcesBrowserTest
    : public ComponentExtensionBrowserTest {
 public:
  ComponentExtensionWorkerChromeResourcesBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kComponentExtensionAllowWorkerChromeResources);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ComponentExtensionWorkerChromeResourcesBrowserTest,
                       FetchChromeResources) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(R"({
    "name": "Component Extension Worker Resources Test",
    "manifest_version": 3,
    "version": "1.0",
    "host_permissions": ["chrome://resources/*"],
    "content_security_policy": {
      "extension_pages": "script-src 'self' chrome://resources;"
    },
    "key": "%s"
  })",
                                            kChromeResourcesTestExtensionKey));

  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), R"(
    <!DOCTYPE html>
    <script src="page.js"></script>
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), R"(
    const worker = new Worker('worker.js', {type: 'module'});
    worker.onmessage = (e) => {
      chrome.test.sendMessage(e.data);
    };
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), R"(
    import {isMac} from 'chrome://resources/js/platform.js';
    postMessage(typeof isMac === 'boolean' ?
                  'worker: success' :
                  'worker: failed');
  )");

  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener worker_listener("worker: success");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            extension->GetResourceURL("page.html")));
  ASSERT_TRUE(worker_listener.WaitUntilSatisfied());
}

// Verifies split-mode extensions cleanly deactivate OTR queues during unload.
IN_PROC_BROWSER_TEST_F(ComponentExtensionBrowserTest,
                       SplitModeMessagingOnUnload) {
  // Activate an OTR (Incognito) profile.
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile);

  TestExtensionDir target_dir;
  target_dir.WriteManifest(base::StringPrintf(R"({
    "name": "Target",
    "manifest_version": 3,
    "version": "1.0",
    "incognito": "split",
    "background": {
      "service_worker": "background.js"
    },
    "key": "%s"
  })",
                                              kExtensionKey));

  // Append chrome.extension.inIncognitoContext to the message so C++ can
  // distinguish between the Primary and Incognito worker check-ins.
  target_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.runtime.onMessageExternal.addListener(
        (message, sender, sendResponse) => {
          sendResponse('pong');
        });

    chrome.test.sendMessage(
        'target_ready_' + chrome.extension.inIncognitoContext);
  )");

  // Listen specifically for the Incognito target checking in.
  ExtensionTestMessageListener target_incognito_ready("target_ready_true");

  // Load target as a component. Component extensions are inherently
  // incognito-enabled by default.
  const Extension* target =
      LoadExtension(target_dir.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(target);

  // Block until the Incognito worker explicitly proves it is alive and running.
  ASSERT_TRUE(target_incognito_ready.WaitUntilSatisfied());

  // Get Incognito workers for the target extension.
  std::vector<extensions::WorkerId> workers =
      extensions::ProcessManager::Get(incognito_profile)
          ->GetServiceWorkersForExtension(target->id());
  ASSERT_FALSE(workers.empty());

  // Make target dormant in the Incognito profile.
  content::RenderProcessHost* target_process =
      content::RenderProcessHost::FromID(workers[0].render_process_id);
  ASSERT_TRUE(target_process);
  content::RenderProcessHostWatcher process_watcher(
      target_process,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  target_process->Shutdown(0);
  process_watcher.Wait();

  TestExtensionDir sender_dir;
  sender_dir.WriteManifest(R"({
    "name": "Sender",
    "manifest_version": 3,
    "version": "1.0",
    "incognito": "split",
    "background": {
      "service_worker": "sender.js"
    }
  })");

  sender_dir.WriteFile(FILE_PATH_LITERAL("sender.js"),
                       base::StringPrintf(R"(
    const message = 'sender_ready_' + chrome.extension.inIncognitoContext;
    chrome.test.sendMessage(message, (reply) => {
      if (reply === 'go') {
        chrome.runtime.sendMessage('%s', 'ping', () => {
          let ignoredError = chrome.runtime.lastError;
        });

        chrome.test.sendMessage(
          'message_sent_' + chrome.extension.inIncognitoContext);
      }
    });
  )",
                                          target->id().c_str()));

  // Listen specifically for the Incognito sender.
  ExtensionTestMessageListener sender_incognito_ready(
      "sender_ready_true", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener sender_incognito_sent("message_sent_true");

  // Load the non-component sender and explicitly set it to incognito since
  // component extensions have incognito enabled by default.
  const Extension* sender = LoadExtension(sender_dir.UnpackedPath());
  ASSERT_TRUE(sender);
  extensions::util::SetIsIncognitoEnabled(sender->id(), profile(), true);

  // Wait for the Incognito sender to initialize.
  ASSERT_TRUE(sender_incognito_ready.WaitUntilSatisfied());

  // Fire the message from the Incognito sender to queue a task in the
  // Incognito ServiceWorkerTaskQueue.
  sender_incognito_ready.Reply("go");
  ASSERT_TRUE(sender_incognito_sent.WaitUntilSatisfied());

  // Unload the extension from the Primary profile.
  // DeactivateTaskQueueForExtension will fail to check
  // IsExtensionIncognitoEnabled because `extension` is already null. Therefore,
  // the primary queue is cleared, but the Incognito queue is not.
  UnloadExtension(target->id());
}

}  // namespace extensions
