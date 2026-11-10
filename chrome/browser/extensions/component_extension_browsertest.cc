// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/to_value_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
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
  // Writes a component extension with version `version` whose service worker
  // identifies itself as `worker_version`. The two are separate so tests can
  // change the worker code without changing the extension version.
  void WriteExtension(TestExtensionDir* dir, int version, int worker_version) {
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
    dir->WriteFile(
        FILE_PATH_LITERAL("sw.js"),
        base::StringPrintf(kBackgroundScriptTemplate, worker_version));
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
  WriteExtension(&test_dir_v1_, /*version=*/1, /*worker_version=*/1);

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
  WriteExtension(&test_dir_v2_, /*version=*/2, /*worker_version=*/2);

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

// PRE_ test: Installs the component extension. Verifies it runs.
IN_PROC_BROWSER_TEST_F(ComponentExtensionServiceWorkerUpdateBrowserTest,
                       PRE_UpdateWithoutVersionChange) {
  WriteExtension(&test_dir_v1_, /*version=*/1, /*worker_version=*/1);

  ExtensionTestMessageListener v1_ready("v1 ready");
  const Extension* extension =
      LoadExtension(test_dir_v1_.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension);

  const ExtensionId id = extension->id();
  ASSERT_EQ(kExtensionId, id);
  ASSERT_TRUE(v1_ready.WaitUntilSatisfied());

  EXPECT_EQ("1", extension->version().GetString());
  EXPECT_EQ(1, GetWorkerVersion(id));
}

// Main test: Installs new worker code without changing the extension version,
// as happens when a browser update ships changes to a component extension
// without a version bump. Verifies the new worker code runs.
// Regression test for crbug.com/521490632.
IN_PROC_BROWSER_TEST_F(ComponentExtensionServiceWorkerUpdateBrowserTest,
                       UpdateWithoutVersionChange) {
  ASSERT_FALSE(
      extension_registry()->enabled_extensions().GetByID(kExtensionId));

  // Pretend the browser version changed since the PRE_ test ran.
  base::AutoReset<const char*> browser_version_override =
      ExtensionRegistrar::OverrideBrowserVersionForTesting("1.0.0.0");

  // Same extension version as in the PRE_ test, new worker code.
  WriteExtension(&test_dir_v2_, /*version=*/1, /*worker_version=*/2);

  ExtensionTestMessageListener v2_ready("v2 ready");
  const Extension* extension =
      LoadExtension(test_dir_v2_.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension);

  const ExtensionId id = extension->id();
  EXPECT_EQ(kExtensionId, id);
  ASSERT_TRUE(v2_ready.WaitUntilSatisfied());

  EXPECT_EQ("1", extension->version().GetString());
  EXPECT_EQ(2, GetWorkerVersion(id));
}

class ComponentExtensionServiceWorkerRefreshSwitchBrowserTest
    : public ComponentExtensionServiceWorkerUpdateBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ComponentExtensionServiceWorkerUpdateBrowserTest::SetUpCommandLine(
        command_line);
    // The PRE_ test installs the worker without the switch; only the main
    // test runs with it.
    if (!content::IsPreTest()) {
      command_line->AppendSwitch(
          switches::kRefreshComponentExtensionServiceWorkers);
    }
  }
};

// PRE_ test: Installs the component extension. Verifies it runs.
IN_PROC_BROWSER_TEST_F(ComponentExtensionServiceWorkerRefreshSwitchBrowserTest,
                       PRE_RefreshSwitch) {
  WriteExtension(&test_dir_v1_, /*version=*/1, /*worker_version=*/1);

  ExtensionTestMessageListener v1_ready("v1 ready");
  const Extension* extension =
      LoadExtension(test_dir_v1_.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension);

  const ExtensionId id = extension->id();
  ASSERT_EQ(kExtensionId, id);
  ASSERT_TRUE(v1_ready.WaitUntilSatisfied());

  EXPECT_EQ("1", extension->version().GetString());
  EXPECT_EQ(1, GetWorkerVersion(id));
}

// Main test: Installs new worker code with the extension version and the
// browser version both unchanged, as happens when a developer rebuilds a
// locally modified component extension. Verifies that
// "--refresh-component-extension-service-workers" picks up the new worker code.
IN_PROC_BROWSER_TEST_F(ComponentExtensionServiceWorkerRefreshSwitchBrowserTest,
                       RefreshSwitch) {
  ASSERT_FALSE(
      extension_registry()->enabled_extensions().GetByID(kExtensionId));

  // Same extension version as in the PRE_ test, new worker code. The browser
  // version is deliberately not overridden, so only the switch can trigger
  // the worker refresh.
  WriteExtension(&test_dir_v2_, /*version=*/1, /*worker_version=*/2);

  ExtensionTestMessageListener v2_ready("v2 ready");
  const Extension* extension =
      LoadExtension(test_dir_v2_.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension);

  const ExtensionId id = extension->id();
  EXPECT_EQ(kExtensionId, id);
  ASSERT_TRUE(v2_ready.WaitUntilSatisfied());

  EXPECT_EQ("1", extension->version().GetString());
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

// Tests for the WebUI origins extensions may access; see
// GetAllowedWebUIOrigins().
class ComponentExtensionWebUIOriginsBrowserTest
    : public ComponentExtensionBrowserTest {
 protected:
  static constexpr char kImageHostPermission[] = "chrome://image/*";

  // Loads an extension that declares `host_permissions`, and navigates the
  // active tab to its page.
  const Extension* LoadAndNavigate(
      const std::vector<std::string>& host_permissions,
      bool load_as_component) {
    auto manifest =
        base::DictValue()
            .Set("name", "WebUI Origins Test")
            .Set("version", "1.0")
            .Set("manifest_version", 3)
            .Set("host_permissions", base::ToValueList(host_permissions));
    if (load_as_component) {
      manifest.Set("key", kExtensionKey);
    }

    auto dir = std::make_unique<TestExtensionDir>();
    dir->WriteManifest(manifest);
    dir->WriteFile(FILE_PATH_LITERAL("page.html"),
                   "<!doctype html><html><body></body></html>");
    // Non-component extensions get install warnings for chrome:// host
    // permissions they aren't allowed to request.
    const Extension* extension = LoadExtension(
        dir->UnpackedPath(), {.ignore_manifest_warnings = !load_as_component,
                              .load_as_component = load_as_component});
    test_dirs_.push_back(std::move(dir));
    if (!extension) {
      return nullptr;
    }
    if (!content::NavigateToURL(GetActiveWebContents(),
                                extension->GetResourceURL("page.html"))) {
      return nullptr;
    }
    return extension;
  }

  // Returns "loaded" or "error" for an <img> pointing at `url`.
  std::string LoadImage(const GURL& url) {
    return content::EvalJs(GetActiveWebContents(), content::JsReplace(R"(
          new Promise(resolve => {
            const img = new Image();
            img.onload = () => resolve('loaded');
            img.onerror = () => resolve('error');
            img.src = $1;
          }))",
                                                                      url))
        .ExtractString();
  }

  // Embeds `url` in a subframe and returns the subframe once its navigation
  // finishes.
  content::RenderFrameHost* EmbedSubframe(const GURL& url) {
    content::WebContents* web_contents = GetActiveWebContents();
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(content::ExecJs(web_contents, content::JsReplace(R"(
          const iframe = document.createElement('iframe');
          iframe.src = $1;
          document.body.appendChild(iframe);
        )",
                                                                 url)));
    observer.Wait();
    return content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  // Registers a chrome:// data source at `source_name` that serves `body` for
  // every path, and lets extensions embed it.
  void AddTestSource(const std::string& source_name, std::string body) {
    content::WebUIDataSource* source =
        content::WebUIDataSource::CreateAndAdd(profile(), source_name);
    source->SetRequestFilter(
        base::BindRepeating([](const std::string& path) { return true; }),
        base::BindRepeating(
            [](const std::string& body, const std::string& path,
               content::WebUIDataSource::GotDataCallback callback) {
              std::move(callback).Run(
                  base::MakeRefCounted<base::RefCountedString>(body));
            },
            std::move(body)));
    source->DisableDenyXFrameOptions();
  }

  // Registers a chrome://image/ data source that serves an SVG, standing in
  // for the SanitizedImageSource that the New Tab Page registers, and returns
  // the image's URL.
  GURL AddTestImageSource() {
    AddTestSource(
        chrome::kChromeUIImageHost,
        "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'/>");
    return GURL("chrome://image/test.svg");
  }

 private:
  std::vector<std::unique_ptr<TestExtensionDir>> test_dirs_;
};

// A component extension that declares the chrome://image/ host permission can
// load it.
IN_PROC_BROWSER_TEST_F(ComponentExtensionWebUIOriginsBrowserTest,
                       DeclaredImageHostLoads) {
  const GURL image_url = AddTestImageSource();
  ASSERT_TRUE(LoadAndNavigate({kImageHostPermission},
                              /*load_as_component=*/true));
  EXPECT_EQ("loaded", LoadImage(image_url));
}

// A component extension that doesn't declare it can't. (The renderer blocks the
// request; the host permission gate itself is covered by
// WebUIOriginAccessTest.HostPermissionOptIns*.)
IN_PROC_BROWSER_TEST_F(ComponentExtensionWebUIOriginsBrowserTest,
                       UndeclaredImageHostBlocked) {
  const GURL image_url = AddTestImageSource();
  ASSERT_TRUE(LoadAndNavigate({}, /*load_as_component=*/true));
  EXPECT_EQ("error", LoadImage(image_url));
}

// A non-component extension can't load chrome://image/, even if it declares it.
// (The parser drops the permission and the renderer blocks the request; the
// trust check itself is covered by
// WebUIOriginAccessTest.HostPermissionOptInsRequireTrust.)
IN_PROC_BROWSER_TEST_F(ComponentExtensionWebUIOriginsBrowserTest,
                       NonComponentCannotUseImageHost) {
  const GURL image_url = AddTestImageSource();
  ASSERT_TRUE(LoadAndNavigate({kImageHostPermission},
                              /*load_as_component=*/false));
  EXPECT_EQ("error", LoadImage(image_url));
}

// Component extension frames are granted navigations to chrome://theme/.
IN_PROC_BROWSER_TEST_F(ComponentExtensionWebUIOriginsBrowserTest,
                       ComponentCanEmbedTheme) {
  AddTestSource(chrome::kChromeUIThemeHost,
                "<!doctype html><html><body>ok</body></html>");
  ASSERT_TRUE(LoadAndNavigate({}, /*load_as_component=*/true));
  content::RenderFrameHost* child =
      EmbedSubframe(GURL(chrome::kChromeUIThemeURL));
  ASSERT_TRUE(child);
  EXPECT_EQ(GURL(chrome::kChromeUIThemeURL), child->GetLastCommittedURL());
  EXPECT_FALSE(child->IsErrorDocument());
}

}  // namespace extensions
