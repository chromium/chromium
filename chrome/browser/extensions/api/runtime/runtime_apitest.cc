// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api/offscreen/offscreen_document_manager.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_constants.h"

namespace extensions {

using ContextType = ExtensionBrowserTest::ContextType;

class RuntimeApiTest : public ExtensionApiTest,
                       public testing::WithParamInterface<ContextType> {
 public:
  RuntimeApiTest() : ExtensionApiTest(GetParam()) {}
  ~RuntimeApiTest() override = default;
  RuntimeApiTest(const RuntimeApiTest&) = delete;
  RuntimeApiTest& operator=(const RuntimeApiTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         RuntimeApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         RuntimeApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Tests the privileged components of chrome.runtime.
IN_PROC_BROWSER_TEST_P(RuntimeApiTest, ChromeRuntimePrivileged) {
  ASSERT_TRUE(RunExtensionTest("runtime/privileged")) << message_;
}

// Tests the unprivileged components of chrome.runtime.
IN_PROC_BROWSER_TEST_P(RuntimeApiTest, ChromeRuntimeUnprivileged) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("runtime/content_script")));

  // The content script runs on this page.
  ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_P(RuntimeApiTest, ChromeRuntimeUninstallURL) {
  // Auto-confirm the uninstall dialog.
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ExtensionTestMessageListener ready_listener("ready");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("runtime")
                                .AppendASCII("uninstall_url")
                                .AppendASCII("sets_uninstall_url")));
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(RunExtensionTest("runtime/uninstall_url")) << message_;
}

IN_PROC_BROWSER_TEST_P(RuntimeApiTest, GetPlatformInfo) {
  ASSERT_TRUE(RunExtensionTest("runtime/get_platform_info")) << message_;
}

namespace {

const char kUninstallUrl[] = "http://www.google.com/";

std::string GetActiveUrl(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetLastCommittedURL()
      .spec();
}

class RuntimeAPIUpdateTest : public ExtensionApiTest {
 public:
  RuntimeAPIUpdateTest() = default;

  RuntimeAPIUpdateTest(const RuntimeAPIUpdateTest&) = delete;
  RuntimeAPIUpdateTest& operator=(const RuntimeAPIUpdateTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  struct ExtensionCRXData {
    std::string unpacked_relative_path;
    base::FilePath crx_path;
    explicit ExtensionCRXData(const std::string& unpacked_relative_path)
        : unpacked_relative_path(unpacked_relative_path) {}
  };

  void SetUpCRX(const std::string& root_dir,
                const std::string& pem_filename,
                std::vector<ExtensionCRXData>* crx_data_list) {
    const base::FilePath test_dir = test_data_dir_.AppendASCII(root_dir);
    const base::FilePath pem_path = test_dir.AppendASCII(pem_filename);
    for (ExtensionCRXData& crx_data : *crx_data_list) {
      crx_data.crx_path = PackExtensionWithOptions(
          test_dir.AppendASCII(crx_data.unpacked_relative_path),
          scoped_temp_dir_.GetPath().AppendASCII(
              crx_data.unpacked_relative_path + ".crx"),
          pem_path, base::FilePath());
    }
  }

  bool CrashEnabledExtension(const ExtensionId& extension_id) {
    ExtensionHost* background_host =
        ProcessManager::Get(browser()->profile())
            ->GetBackgroundHostForExtension(extension_id);
    if (!background_host) {
      return false;
    }
    content::CrashTab(background_host->host_contents());
    return true;
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeOpenOptionsPage) {
  ASSERT_TRUE(RunExtensionTest("runtime/open_options_page"));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeOpenOptionsPageError) {
  ASSERT_TRUE(RunExtensionTest("runtime/open_options_page_error"));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeGetPlatformInfo) {
  base::Value::Dict dict =
      api_test_utils::ToDict(api_test_utils::RunFunctionAndReturnSingleResult(
          new RuntimeGetPlatformInfoFunction(), "[]", profile()));
  EXPECT_TRUE(dict.contains("os"));
  EXPECT_TRUE(dict.contains("arch"));
  EXPECT_TRUE(dict.contains("nacl_arch"));
}

// Tests chrome.runtime.getPackageDirectory with an app.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       ChromeRuntimeGetPackageDirectoryEntryApp) {
  ASSERT_TRUE(RunExtensionTest("api_test/runtime/get_package_directory/app",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Tests chrome.runtime.getPackageDirectory with an MV2 extension.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ChromeRuntimeGetPackageDirectoryEntryMV2Extension) {
  ASSERT_TRUE(RunExtensionTest("runtime/get_package_directory/extension",
                               {.extension_url = "test/test.html"}))
      << message_;
}

// Tests chrome.runtime.getPackageDirectory with an MV3 extension. Note: we use
// an html page in this test as getPackageDirectory isn't exposed on service
// workers.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ChromeRuntimeGetPackageDirectoryEntryMV3Extension) {
  SetCustomArg("run_promise_test");
  ASSERT_TRUE(RunExtensionTest("runtime/get_package_directory/extension",
                               {.extension_url = "test/test.html"},
                               {.load_as_manifest_version_3 = true}))
      << message_;
}

// Tests that an extension calling chrome.runtime.reload() repeatedly
// will eventually be terminated.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExtensionTerminatedForRapidReloads) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  static constexpr char kManifest[] = R"(
      {
        "name": "reload",
        "version": "1.0",
        "background": {
          "scripts": ["background.js"]
        },
        "manifest_version": 2
      })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                "chrome.test.sendMessage('ready');");

  // Use a packed extension, since this is the scenario we are interested in
  // testing. Unpacked extensions are allowed more reloads within the allotted
  // time, to avoid interfering with the developer work flow.
  const Extension* extension = LoadExtension(dir.Pack());
  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();

  // The current limit for fast reload is 5, so the loop limit of 10
  // be enough to trigger termination. If the extension manages to
  // reload itself that often without being terminated, the test fails
  // anyway.
  for (int i = 0; i < RuntimeAPI::kFastReloadCount + 1; i++) {
    ExtensionTestMessageListener ready_listener_reload("ready");
    TestExtensionRegistryObserver unload_observer(registry, extension_id);
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension_id, "chrome.runtime.reload();"));
    unload_observer.WaitForExtensionUnloaded();
    base::RunLoop().RunUntilIdle();

    if (registry->terminated_extensions().GetByID(extension_id)) {
      break;
    } else {
      EXPECT_TRUE(ready_listener_reload.WaitUntilSatisfied());
    }
  }
  ASSERT_TRUE(registry->terminated_extensions().GetByID(extension_id));
}

// Tests chrome.runtime.reload
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeReload) {
  static constexpr char kManifest[] = R"(
      {
        "name": "reload",
        "version": "1.0",
        "background": {
          "scripts": ["background.js"]
        },
        "manifest_version": 2
      })";

  static constexpr char kScript[] = R"(
    chrome.test.sendMessage('ready', function(response) {
      if (response == 'reload') {
        chrome.runtime.reload();
      } else if (response == 'done') {
        chrome.test.notifyPass();
      }
    });
  )";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), kScript);

  // This listener will respond to the initial load of the extension
  // and tell the script to do the reload.
  ExtensionTestMessageListener ready_listener_reload("ready",
                                                     ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();
  EXPECT_TRUE(ready_listener_reload.WaitUntilSatisfied());

  // This listener will respond to the ready message from the
  // reloaded extension and tell the script to finish the test.
  ExtensionTestMessageListener ready_listener_done("ready",
                                                   ReplyBehavior::kWillReply);
  ResultCatcher reload_catcher;
  ready_listener_reload.Reply("reload");
  EXPECT_TRUE(ready_listener_done.WaitUntilSatisfied());
  ready_listener_done.Reply("done");
  EXPECT_TRUE(reload_catcher.GetNextResult());
}

// Tests sending messages from a webpage in the extension using
// chrome.runtime.sendMessage and responding to those from the extension's
// service worker in a chrome.runtime.onMessage listener.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeSendMessage) {
  ASSERT_TRUE(
      RunExtensionTest("runtime/send_message", {.extension_url = "test.html"}));
}

// Simple test for chrome.runtime.getBackgroundPage with a persistent background
// page.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeGetBackgroundPage) {
  static constexpr char kManifest[] = R"(
      {
        "name": "getBackgroundPage",
        "version": "1.0",
        "background": {
          "scripts": ["background.js"]
        },
        "manifest_version": 2
      })";

  static constexpr char kBackground[] = "window.backgroundExists = true;";
  static constexpr char kTestPage[] = R"(<script src="test.js"></script>)";
  static constexpr char kTestJS[] = R"(
    chrome.test.runTests([
      function getBackgroundPage() {
        chrome.runtime.getBackgroundPage((page) => {
          chrome.test.assertTrue(page.backgroundExists);
          chrome.test.succeed();
        });
      }
    ]);
  )";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);
  dir.WriteFile(FILE_PATH_LITERAL("test.html"), kTestPage);
  dir.WriteFile(FILE_PATH_LITERAL("test.js"), kTestJS);

  ASSERT_TRUE(RunExtensionTest(dir.UnpackedPath(),
                               {.extension_url = "test.html"},
                               /*load_options=*/{}));
}

// Simple test for chrome.runtime.getBackgroundPage with an MV3 service worker
// extension, which should return an error due to there being no background
// page.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeGetBackgroundPageMV3) {
  static constexpr char kManifest[] = R"(
      {
        "name": "getBackgroundPage",
        "version": "1.0",
        "background": {
          "service_worker": "worker.js"
        },
        "manifest_version": 3
      })";

  static constexpr char kWorker[] = "// We're just expecting an error";
  static constexpr char kTestPage[] = R"(<script src="test.js"></script>)";
  static constexpr char kTestJS[] = R"(
    chrome.test.runTests([
      function getBackgroundPage() {
        chrome.runtime.getBackgroundPage((page) => {
          chrome.test.assertEq(undefined, page);
          chrome.test.assertLastError('You do not have a background page.');
          chrome.test.succeed();
        });
      },

      async function getBackGroundPagePromise() {
        await chrome.test.assertPromiseRejects(
            chrome.runtime.getBackgroundPage(),
            'Error: You do not have a background page.');
        chrome.test.succeed();
      }
    ]);
  )";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);
  dir.WriteFile(FILE_PATH_LITERAL("test.html"), kTestPage);
  dir.WriteFile(FILE_PATH_LITERAL("test.js"), kTestJS);

  ASSERT_TRUE(RunExtensionTest(
      dir.UnpackedPath(), {.extension_url = "test.html"}, /*load_options=*/{}));
}

// Simple test for chrome.runtime.requestUpdateCheck using promises and
// callbacks. The actual behaviors and responses are more thoroughly tested in
// chrome_runtime_api_delegate_unittest.cc
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, RuntimeRequestUpdateCheck) {
  static constexpr char kManifest[] = R"(
      {
        "name": "requestUpdateCheck",
        "version": "1.0",
        "background": {
          "service_worker": "worker.js"
        },
        "manifest_version": 3
      })";

  static constexpr char kWorker[] = R"(
    chrome.test.runTests([
      // Note: when called with a callback, the callback will receive two
      // parameters, but when called with a promise they will come back as
      // parameters on a single object.
      function noUpdateCallback() {
        chrome.runtime.requestUpdateCheck((status, details) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq('no_update', status);
          chrome.test.assertEq({version: ''}, details);

          // Another call soon after will be throttled.
          chrome.runtime.requestUpdateCheck((status, details) => {
            chrome.test.assertNoLastError();
            chrome.test.assertEq('throttled', status);
            chrome.test.assertEq({version: ''}, details);
            chrome.test.succeed();
          });
        });
      },

      async function noUpdate() {
        // Advance the throttle clock so the requests in the previous test don't
        // result in this getting a throttled response.
        await chrome.test.sendMessage('Advance');
        let result = await chrome.runtime.requestUpdateCheck();
        chrome.test.assertEq({status:'no_update', version: ''}, result);

        result = await chrome.runtime.requestUpdateCheck();
        chrome.test.assertEq({status:'throttled', version: ''}, result);
        chrome.test.succeed();
      }
    ]);
  )";
  base::SimpleTestTickClock clock;
  ChromeRuntimeAPIDelegate::set_tick_clock_for_tests(&clock);

  ExtensionTestMessageListener message_listener("Advance",
                                                ReplyBehavior::kWillReply);

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);
  // In the test environment we need this to be a packed extension.
  base::FilePath crx_path = PackExtension(dir.UnpackedPath());
  ASSERT_FALSE(crx_path.empty());

  auto OnMessage = [&](const std::string& message) {
    // Advance the clock past the point it will be throttled.
    clock.Advance(base::Days(1));
    message_listener.Reply("");
  };
  message_listener.SetOnSatisfied(base::BindLambdaForTesting(OnMessage));

  ASSERT_TRUE(RunExtensionTest(crx_path, {}, {}));
}

// Tests that updating a terminated extension sends runtime.onInstalled event
// with correct previousVersion.
// Regression test for https://crbug.com/724563.
IN_PROC_BROWSER_TEST_F(RuntimeAPIUpdateTest,
                       TerminatedExtensionUpdateHasCorrectPreviousVersion) {
  std::vector<ExtensionCRXData> data;
  data.emplace_back("v1");
  data.emplace_back("v2");
  SetUpCRX("runtime/update_terminated_extension", "pem.pem", &data);

  ExtensionId extension_id;
  {
    // Install version 1 of the extension.
    ResultCatcher catcher;
    const int expected_change = 1;
    const Extension* extension_v1 =
        InstallExtension(data[0].crx_path, expected_change);
    extension_id = extension_v1->id();
    ASSERT_TRUE(extension_v1);
    EXPECT_TRUE(catcher.GetNextResult());
  }

  ASSERT_TRUE(CrashEnabledExtension(extension_id));

  // The process-terminated notification may be received immediately before
  // the task that will actually update the active-extensions count, so spin
  // the message loop to ensure we are up-to-date.
  base::RunLoop().RunUntilIdle();

  {
    // Update to version 2, expect runtime.onInstalled with
    // previousVersion = '1'.
    ResultCatcher catcher;
    const int expected_change = 1;
    const Extension* extension_v2 =
        UpdateExtension(extension_id, data[1].crx_path, expected_change);
    ASSERT_TRUE(extension_v2);
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

// Tests that when the last active tab in the window belongs to the extension
// with an uninstall URL, uninstalling the extension does not close the current
// browser. Regression test for crbug.com/362452856
IN_PROC_BROWSER_TEST_P(RuntimeApiTest,
                       OpenUninstallUrlWhenExtensionPageIsTheOnlyActiveTab) {
  ExtensionTestMessageListener ready_listener("ready");
  // Load an extension that has set an uninstall url.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("runtime")
                        .AppendASCII("uninstall_url")
                        .AppendASCII("sets_uninstall_url"));
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(extension.get());
  extension_service()->AddExtension(extension.get());
  ASSERT_TRUE(extension_service()->IsExtensionEnabled(extension->id()));
  TabStripModel* tabs = browser()->tab_strip_model();

  ASSERT_EQ(1, tabs->count());
  ASSERT_EQ("about:blank", GetActiveUrl(browser()));

  // Navigate to an extension page.
  const GURL extension_page_url = extension->GetResourceURL("page.html");
  content::RenderFrameHost* new_host =
      ui_test_utils::NavigateToURL(browser(), extension_page_url);
  ASSERT_TRUE(new_host);

  EXPECT_EQ(1, tabs->count());
  EXPECT_EQ(extension_page_url.spec(), GetActiveUrl(browser()));

  // Uninstall the extension and expect its uninstall url to open in a new tab.
  extension_service()->UninstallExtension(
      extension->id(), UNINSTALL_REASON_USER_INITIATED, nullptr);
  content::WaitForLoadStop(tabs->GetActiveWebContents());
  EXPECT_EQ(2, tabs->count());

  // The current tab should be pointing to the uninstall url of the extension.
  EXPECT_EQ(kUninstallUrl, GetActiveUrl(browser()));

  // The tab at index 0 should now be overwritten with the default NTP.
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            tabs->GetWebContentsAt(0)->GetLastCommittedURL().spec());
}

// Tests that when a blocklisted extension with a set uninstall url is
// uninstalled, its uninstall url does not open.
IN_PROC_BROWSER_TEST_P(RuntimeApiTest,
                       DoNotOpenUninstallUrlForBlocklistedExtensions) {
  ExtensionTestMessageListener ready_listener("ready");
  // Load an extension that has set an uninstall url.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("runtime")
                        .AppendASCII("uninstall_url")
                        .AppendASCII("sets_uninstall_url"));
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(extension.get());
  extension_service()->AddExtension(extension.get());
  ASSERT_TRUE(extension_service()->IsExtensionEnabled(extension->id()));

  // Uninstall the extension and expect its uninstall url to open.
  extension_service()->UninstallExtension(
      extension->id(), UNINSTALL_REASON_USER_INITIATED, nullptr);
  TabStripModel* tabs = browser()->tab_strip_model();

  EXPECT_EQ(2, tabs->count());
  content::WaitForLoadStop(tabs->GetActiveWebContents());
  // Verify the uninstall url
  EXPECT_EQ(kUninstallUrl, GetActiveUrl(browser()));

  // Close the tab pointing to the uninstall url.
  tabs->CloseWebContentsAt(tabs->active_index(), 0);
  EXPECT_EQ(1, tabs->count());
  EXPECT_EQ("about:blank", GetActiveUrl(browser()));

  // Load the same extension again, except blocklist it after installation.
  ExtensionTestMessageListener ready_listener_reload("ready");
  extension = LoadExtension(test_data_dir_.AppendASCII("runtime")
                                .AppendASCII("uninstall_url")
                                .AppendASCII("sets_uninstall_url"));
  EXPECT_TRUE(ready_listener_reload.WaitUntilSatisfied());
  extension_service()->AddExtension(extension.get());
  ASSERT_TRUE(extension_service()->IsExtensionEnabled(extension->id()));

  // Blocklist extension.
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      extension->id(), BitMapBlocklistState::BLOCKLISTED_MALWARE,
      ExtensionPrefs::Get(profile()));

  // Uninstalling a blocklisted extension should not open its uninstall url.
  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()),
                                         extension->id());
  extension_service()->UninstallExtension(
      extension->id(), UNINSTALL_REASON_USER_INITIATED, nullptr);
  observer.WaitForExtensionUninstalled();

  EXPECT_EQ(1, tabs->count());
  EXPECT_TRUE(content::WaitForLoadStop(tabs->GetActiveWebContents()));
  EXPECT_EQ(url::kAboutBlankURL, GetActiveUrl(browser()));
}

// Used for tests that only make sense with a background page.
using BackgroundPageOnlyRuntimeApiTest = RuntimeApiTest;
INSTANTIATE_TEST_SUITE_P(All,
                         BackgroundPageOnlyRuntimeApiTest,
                         testing::Values(ContextType::kPersistentBackground));

// Regression test for https://crbug.com/1298195 - whether a tab opened
// from the background page (via `window.open(...)`) will be correctly
// marked as `mojom::ViewType::kTabContents`.
//
// This test is a BackgroundPageOnlyRuntimeApiTest, because service workers
// can call neither 1) window.open nor 2) chrome.extension.getViews.
IN_PROC_BROWSER_TEST_P(BackgroundPageOnlyRuntimeApiTest,
                       GetViewsOfWindowOpenedFromBackgroundPage) {
  ASSERT_EQ(GetParam(), ContextType::kPersistentBackground);
  static constexpr char kManifest[] = R"(
      {
        "name": "test",
        "version": "1.0",
        "background": {"scripts": ["background.js"]},
        "manifest_version": 2
      })";
  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  dir.WriteFile(FILE_PATH_LITERAL("index.htm"), "");

  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL new_tab_url = extension->GetResourceURL("/index.htm");
  {
    content::TestNavigationObserver nav_observer(new_tab_url);
    nav_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension->id(), R"(window.open('/index.htm', '');)"));
    nav_observer.Wait();
  }

  {
    ExtensionHost* host = ProcessManager::Get(browser()->profile())
                              ->GetBackgroundHostForExtension(extension->id());
    ASSERT_TRUE(host);
    content::DOMMessageQueue message_queue(host->host_contents());

    static constexpr char kScript[] = R"(
        const foundWindows = chrome.extension.getViews({type: 'tab'});
        domAutomationController.send(foundWindows.length);
        domAutomationController.send(foundWindows[0].location.href);
    )";
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(extension->id(), kScript));

    std::string json;
    ASSERT_TRUE(message_queue.WaitForMessage(&json));
    ASSERT_EQ("1", json);

    ASSERT_TRUE(message_queue.WaitForMessage(&json));
    std::optional<base::Value> url =
        base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
    ASSERT_TRUE(url->is_string());
    ASSERT_EQ(new_tab_url.spec(), url->GetString());
  }
}

class RuntimeGetContextsApiTest : public ExtensionApiTest {
 public:
  RuntimeGetContextsApiTest() = default;
  RuntimeGetContextsApiTest(const RuntimeGetContextsApiTest&) = delete;
  RuntimeGetContextsApiTest& operator=(const RuntimeGetContextsApiTest&) =
      delete;
  ~RuntimeGetContextsApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    static constexpr char kManifest[] =
        R"({
             "name": "Get Contexts",
             "version": "0.1",
             "manifest_version": 3,
             "permissions": ["offscreen", "sidePanel"],
             "side_panel": {
               "default_path": "side_panel.html"
             },
             "devtools_page": "devtools.html",
             "action": {},
             "background": {
               "service_worker": "background.js"
             }
           })";
    test_dir_.WriteManifest(kManifest);
    test_dir_.WriteFile(FILE_PATH_LITERAL("background.js"),
                        "// Intentionally blank");
    test_dir_.WriteFile(FILE_PATH_LITERAL("page.html"),
                        "<html>Hello, world!</html>");
    test_dir_.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                        "<html>Hello, offscreen world!</html>");
    test_dir_.WriteFile(FILE_PATH_LITERAL("side_panel.html"),
                        R"(<html>
                             Hello, side panel!
                             <script src="side_panel.js"></script>
                           </html>)");
    test_dir_.WriteFile(FILE_PATH_LITERAL("side_panel.js"),
                        "chrome.test.sendMessage('panel opened');");
    test_dir_.WriteFile(FILE_PATH_LITERAL("devtools.html"),
                        R"(<html>
                             Hello, developer tools!
                             <script src="devtools.js"></script>
                           </html>)");
    test_dir_.WriteFile(FILE_PATH_LITERAL("devtools.js"),
                        "chrome.test.sendMessage('devtools page opened');");
    extension_ = LoadExtension(test_dir_.UnpackedPath());
    ASSERT_TRUE(extension_);
  }

  // Runs `chrome.runtime.getContexts()` and returns the result as a
  // base::Value.
  base::Value GetContexts(std::string_view filter) {
    static constexpr char kScriptTemplate[] =
        R"((async () => {
             chrome.test.sendScriptResult(
                 await chrome.runtime.getContexts(%s));
           })();)";
    std::string script = base::StringPrintf(kScriptTemplate, filter.data());
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), extension_->id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  }

  // Runs `chrome.runtime.getContexts()` and returns the result as a vector
  // of strongly-typed `ExtensionContext`s. Expects the getContexts() call to
  // return a valid value (i.e., not throw an error).
  std::vector<api::runtime::ExtensionContext> GetContextStructs(
      std::string_view filter) {
    base::Value value = GetContexts(filter);
    return ContextValueToContextStructs(value);
  }

  // Converts the given `value` into a vector of strongly-typed
  // `ExtensionContext`s. Expects the value to properly convert.
  std::vector<api::runtime::ExtensionContext> ContextValueToContextStructs(
      const base::Value& value) {
    if (!value.is_list()) {
      ADD_FAILURE() << "Invalid return value: " << value;
      return {};
    }

    std::vector<api::runtime::ExtensionContext> result;
    result.reserve(value.GetList().size());
    for (const auto& entry : value.GetList()) {
      if (!entry.is_dict()) {
        ADD_FAILURE() << "Invalid return value: " << value;
        return {};
      }
      auto context = api::runtime::ExtensionContext::FromValue(entry.GetDict());
      if (!context) {
        ADD_FAILURE() << "Invalid return value: " << value;
        return {};
      }

      result.push_back(std::move(*context));
    }

    return result;
  }

  // Returns a matcher that expects an ExtensionContext to be a valid
  // background context (without testing details of the entry).
  auto GetBackgroundMatcher() {
    return testing::AllOf(
        testing::Field(&api::runtime::ExtensionContext::context_type,
                       testing::Eq(api::runtime::ContextType::kBackground)),
        testing::Field(&api::runtime::ExtensionContext::tab_id,
                       testing::Eq(-1)),
        testing::Field(&api::runtime::ExtensionContext::window_id,
                       testing::Eq(-1)),
        testing::Field(&api::runtime::ExtensionContext::frame_id,
                       testing::Eq(-1)));
  }

  // Returns a matcher that expects an ExtensionContext to correspond to a
  // frame-based context type (without testing the details of the entry).
  auto GetFrameMatcher(api::runtime::ContextType context_type,
                       const GURL& url) {
    return testing::AllOf(
        testing::Field(&api::runtime::ExtensionContext::context_type,
                       testing::Eq(context_type)),
        testing::Field(&api::runtime::ExtensionContext::document_url,
                       testing::Eq(url.spec())));
  }

  const Extension& extension() const { return *extension_; }

 private:
  raw_ptr<const Extension, DanglingUntriaged> extension_ = nullptr;
  TestExtensionDir test_dir_;
};

// Tests retrieving the background service worker context using
// `chrome.runtime.getContexts()`.

// TODO(crbug.com/40901108): failed on "chromium/ci/Mac12 Tests"
#if BUILDFLAG(IS_MAC)
#define MAYBE_GetServiceWorkerContext DISABLED_GetServiceWorkerContext
#else
#define MAYBE_GetServiceWorkerContext GetServiceWorkerContext
#endif
IN_PROC_BROWSER_TEST_F(RuntimeGetContextsApiTest,
                       MAYBE_GetServiceWorkerContext) {
  // An empty dictionary filter should match all contexts (of which there is
  // only one).
  base::Value contexts = GetContexts("{}");

  ProcessManager* process_manager = ProcessManager::Get(profile());
  std::vector<WorkerId> workers =
      process_manager->GetServiceWorkersForExtension(extension().id());
  ASSERT_EQ(1u, workers.size());
  base::Uuid expected_context_id =
      ProcessManager::Get(profile())->GetContextIdForWorker(workers[0]);
  EXPECT_TRUE(expected_context_id.is_valid());

  // Note: fields of `documentId`, `documentUrl`, and `documentOrigin` are
  // undefined (service worker contexts don't have an associated document).
  // `tabId`, `frameId`, and `windowId` are -1 for consistency with other
  // APIs.
  static constexpr char kExpected[] =
      R"([{
            "contextType": "BACKGROUND",
            "contextId": "%s",
            "tabId": -1,
            "windowId": -1,
            "frameId": -1,
            "incognito": false
         }])";
  std::string expected_contents = base::StringPrintf(
      kExpected, expected_context_id.AsLowercaseString().c_str());
  EXPECT_THAT(contexts, base::test::IsJson(expected_contents));

  // Now, wait for the extension worker to terminate and verify that no
  // no contexts are retrieved.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension().id());
  // In order to be able to call the API, we need to open a new tab to an
  // extension resource.
  const GURL extension_page_url = extension().GetResourceURL("page.html");
  content::RenderFrameHost* new_host =
      ui_test_utils::NavigateToURL(browser(), extension_page_url);
  ASSERT_TRUE(new_host);

  static constexpr char kScript[] =
      R"((async () => {
           let contexts =
               await chrome.runtime.getContexts({contextTypes: ['BACKGROUND']});
           return JSON.stringify(contexts);
         })();)";
  // No contexts should have been returned.
  EXPECT_EQ("[]", content::EvalJs(new_host, kScript));
}

// Tests the filter matching behavior of `runtime.getContexts()`.
IN_PROC_BROWSER_TEST_F(RuntimeGetContextsApiTest, FilterMatching) {
  // Currently, there is only one context: the background service worker. Also
  // open a tab-based context.
  const GURL extension_page_url = extension().GetResourceURL("page.html");
  content::RenderFrameHost* new_host =
      ui_test_utils::NavigateToURL(browser(), extension_page_url);
  ASSERT_TRUE(new_host);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(new_host);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  {
    // Pass a filter matching everything. Both the tab context and worker
    // context should be returned.
    std::vector<api::runtime::ExtensionContext> contexts =
        GetContextStructs(R"({})");
    EXPECT_THAT(contexts, testing::UnorderedElementsAre(
                              GetBackgroundMatcher(),
                              GetFrameMatcher(api::runtime::ContextType::kTab,
                                              extension_page_url)));
  }
  {
    // Passing a filter to match background contexts should match the worker.
    std::vector<api::runtime::ExtensionContext> contexts =
        GetContextStructs(R"({"contextTypes": ["BACKGROUND"]})");
    EXPECT_THAT(contexts, testing::ElementsAre(GetBackgroundMatcher()));
  }
  {
    // Passing a filter to match a tab ID should match the corresponding
    // page context.
    std::string filter = base::StringPrintf(R"({"tabIds": [%d]})", tab_id);
    std::vector<api::runtime::ExtensionContext> contexts =
        GetContextStructs(filter);
    EXPECT_THAT(contexts,
                testing::ElementsAre(GetFrameMatcher(
                    api::runtime::ContextType::kTab, extension_page_url)));
  }
  {
    // Try passing a filter for a context type with no corresponding matches. No
    // contexts should be returned.
    std::vector<api::runtime::ExtensionContext> contexts =
        GetContextStructs(R"({"contextTypes": ["POPUP"]})");
    EXPECT_THAT(contexts, testing::IsEmpty());
  }
  {
    // Filter properties support an array of options; if the context matches an
    // entry in the array, it matches the filter for that property. Thus,
    // passing both "BACKGROUND" and "POPUP" should match the service worker
    // context.
    std::vector<api::runtime::ExtensionContext> contexts =
        GetContextStructs(R"({"contextTypes": ["BACKGROUND", "POPUP"]})");
    EXPECT_THAT(contexts, testing::ElementsAre(GetBackgroundMatcher()));
  }
  {
    // All specified filter properties must match. Thus, if we look for a
    // background context and also specify a tab ID, nothing should match
    // (since the background context doesn't have an associated tab).
    static constexpr char kFilter[] =
        R"({
             "contextTypes": ["BACKGROUND"],
             "tabIds": [2]
           })";
    std::vector<api::runtime::ExtensionContext> contexts =
        GetContextStructs(kFilter);
    EXPECT_THAT(contexts, testing::IsEmpty());
  }
}

// Tests retrieving tab contexts using `chrome.runtime.getContexts()`.
IN_PROC_BROWSER_TEST_F(RuntimeGetContextsApiTest, GetTabContext) {
  // Open a new extension tab.
  const GURL frame_url = extension().GetResourceURL("page.html");
  content::RenderFrameHost* new_host =
      ui_test_utils::NavigateToURL(browser(), frame_url);
  ASSERT_TRUE(new_host);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(new_host);

  int expected_tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int expected_window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int expected_frame_id = ExtensionApiFrameIdMap::GetFrameId(new_host);
  std::string expected_context_id =
      ExtensionApiFrameIdMap::GetContextId(new_host).AsLowercaseString();
  std::string expected_document_id =
      ExtensionApiFrameIdMap::GetDocumentId(new_host).ToString();
  std::string expected_frame_url = frame_url.spec();
  std::string expected_origin = extension().origin().Serialize();

  // Query for tab-based contexts. There should only be one.
  base::Value background_contexts = GetContexts(R"({"contextTypes": ["TAB"]})");

  // Verify the properties of the returned context.
  static constexpr char kExpectedTemplate[] =
      R"([{
            "contextType": "TAB",
            "contextId": "%s",
            "tabId": %d,
            "windowId": %d,
            "frameId": %d,
            "documentId": "%s",
            "documentUrl": "%s",
            "documentOrigin": "%s",
            "incognito": false
         }])";
  std::string expected = base::StringPrintf(
      kExpectedTemplate, expected_context_id.c_str(), expected_tab_id,
      expected_window_id, expected_frame_id, expected_document_id.c_str(),
      expected_frame_url.c_str(), expected_origin.c_str());
  EXPECT_THAT(background_contexts, base::test::IsJson(expected));
}

// Tests retrieving offscreen documents with `runtime.getContexts()`.
IN_PROC_BROWSER_TEST_F(RuntimeGetContextsApiTest, GetOffscreenDocumentContext) {
  // Open a new offscreen document.
  static constexpr char kOpenOffscreenDocumentScript[] =
      R"((async () => {
           await chrome.offscreen.createDocument(
               {
                   url: 'offscreen.html',
                   reasons: ['DOM_PARSER'],
                   justification: 'testing'
               });
           chrome.test.sendScriptResult('done');
         })();)";
  base::Value script_result = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension().id(), kOpenOffscreenDocumentScript,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_EQ("done", script_result);

  OffscreenDocumentManager* offscreen_manager =
      OffscreenDocumentManager::Get(profile());
  const OffscreenDocumentHost* offscreen_document =
      offscreen_manager->GetOffscreenDocumentForExtension(extension());
  ASSERT_TRUE(offscreen_document);

  content::RenderFrameHost* offscreen_frame_host =
      offscreen_document->web_contents()->GetPrimaryMainFrame();
  int expected_frame_id =
      ExtensionApiFrameIdMap::GetFrameId(offscreen_frame_host);
  std::string expected_context_id =
      ExtensionApiFrameIdMap::GetContextId(offscreen_frame_host)
          .AsLowercaseString();
  std::string expected_document_id =
      ExtensionApiFrameIdMap::GetDocumentId(offscreen_frame_host).ToString();
  std::string expected_frame_url =
      extension().GetResourceURL("offscreen.html").spec();
  std::string expected_origin = extension().origin().Serialize();

  // Query for offscreen document contexts. There should only be one.
  base::Value background_contexts =
      GetContexts(R"({"contextTypes": ["OFFSCREEN_DOCUMENT"]})");

  // Verify the properties of the returned context.
  static constexpr char kExpectedTemplate[] =
      R"([{
            "contextType": "OFFSCREEN_DOCUMENT",
            "contextId": "%s",
            "tabId": -1,
            "windowId": -1,
            "frameId": %d,
            "documentId": "%s",
            "documentUrl": "%s",
            "documentOrigin": "%s",
            "incognito": false
         }])";
  std::string expected =
      base::StringPrintf(kExpectedTemplate, expected_context_id.c_str(),
                         expected_frame_id, expected_document_id.c_str(),
                         expected_frame_url.c_str(), expected_origin.c_str());
  EXPECT_THAT(background_contexts, base::test::IsJson(expected));
}

// Tests retrieving a side panel context from the `runtime.getContexts()` API.
IN_PROC_BROWSER_TEST_F(RuntimeGetContextsApiTest, GetSidePanelContext) {
  // Set the side panel to open on toolbar action click. This makes it easier
  // to trigger.
  static constexpr char kSetUpSidePanelScript[] =
      R"((async () => {
           await chrome.sidePanel.setPanelBehavior(
               {openPanelOnActionClick: true});
           chrome.test.sendScriptResult('done');
         })();)";

  base::Value script_result = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension().id(), kSetUpSidePanelScript,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_EQ("done", script_result);

  // Click on the toolbar action and wait for the panel context to open.
  ExtensionTestMessageListener panel_listener("panel opened");
  ExtensionActionTestHelper::Create(browser())->Press(extension().id());
  ASSERT_TRUE(panel_listener.WaitUntilSatisfied());

  // Fetch the side panel host.
  ExtensionHostRegistry* host_registry = ExtensionHostRegistry::Get(profile());
  std::vector<ExtensionHost*> hosts =
      host_registry->GetHostsForExtension(extension().id());
  ASSERT_EQ(1u, hosts.size());
  ExtensionHost* panel_host = hosts[0];
  EXPECT_EQ(mojom::ViewType::kExtensionSidePanel,
            panel_host->extension_host_type());
  content::RenderFrameHost* panel_frame_host =
      panel_host->web_contents()->GetPrimaryMainFrame();

  // Verify the `runtime.getContexts()` API can retrieve the context and that
  // the proper values are returned.
  int expected_frame_id = ExtensionApiFrameIdMap::GetFrameId(panel_frame_host);
  std::string expected_context_id =
      ExtensionApiFrameIdMap::GetContextId(panel_frame_host)
          .AsLowercaseString();
  std::string expected_document_id =
      ExtensionApiFrameIdMap::GetDocumentId(panel_frame_host).ToString();
  std::string expected_frame_url =
      extension().GetResourceURL("side_panel.html").spec();
  std::string expected_origin = extension().origin().Serialize();

  base::Value side_panel_contexts =
      GetContexts(R"({"contextTypes": ["SIDE_PANEL"]})");

  // Verify the properties of the returned context.
  static constexpr char kExpectedTemplate[] =
      R"([{
            "contextType": "SIDE_PANEL",
            "contextId": "%s",
            "tabId": -1,
            "windowId": -1,
            "frameId": %d,
            "documentId": "%s",
            "documentUrl": "%s",
            "documentOrigin": "%s",
            "incognito": false
         }])";
  std::string expected =
      base::StringPrintf(kExpectedTemplate, expected_context_id.c_str(),
                         expected_frame_id, expected_document_id.c_str(),
                         expected_frame_url.c_str(), expected_origin.c_str());
  EXPECT_THAT(side_panel_contexts, base::test::IsJson(expected));
}

// Tests the behavior of `runtime.getContexts()` with a split-mode incognito
// extension. In split mode, the extension should only be able to access data
// about its own process's contexts.
IN_PROC_BROWSER_TEST_F(RuntimeGetContextsApiTest,
                       RetrievingIncognitoContexts_SplitMode) {
  // Load up a split-mode extension.
  static constexpr char kManifest[] =
      R"({
           "name": "Split mode extension",
           "version": "0.1",
           "manifest_version": 3,
           "incognito": "split",
           "background": {"service_worker": "background.js"}
         })";

  // Since we need to wait for the incognito profile's separate service worker
  // to start, our bootstrapping code in LoadExtension() doesn't automatically
  // handle it for us. Include a separate "ready" message.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     "chrome.test.sendMessage('ready');");
  test_dir.WriteFile(FILE_PATH_LITERAL("regular.html"), "<html>Regular</html>");
  test_dir.WriteFile(FILE_PATH_LITERAL("incognito.html"),
                     "<html>Incognito</html>");

  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Open a tab on-the-record to one of the extension's pages.
  GURL regular_url = extension->GetResourceURL("regular.html");
  content::RenderFrameHost* regular_host =
      ui_test_utils::NavigateToURL(browser(), regular_url);
  ASSERT_TRUE(regular_host);

  // Open up an incognito tab to another extension page, and wait for the
  // incognito version of the extension to start up.
  ready_listener.Reset();
  GURL incognito_url = extension->GetResourceURL("incognito.html");
  Browser* incognito_browser = OpenURLOffTheRecord(profile(), incognito_url);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // A helper method to retrieve the contexts for the given `profile`.
  auto run_get_contexts_in_profile = [extension](Profile* profile) {
    static constexpr char kScript[] =
        R"((async () => {
             chrome.test.sendScriptResult(
                 await chrome.runtime.getContexts({}));
           })();)";
    return BackgroundScriptExecutor::ExecuteScript(
        profile, extension->id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  {
    // Verify the on-the-record contexts. There should be a single background
    // context and the on-the-record tab.
    base::Value regular_results = run_get_contexts_in_profile(profile());
    std::vector<api::runtime::ExtensionContext> contexts =
        ContextValueToContextStructs(regular_results);
    EXPECT_THAT(contexts, testing::UnorderedElementsAre(
                              GetBackgroundMatcher(),
                              GetFrameMatcher(api::runtime::ContextType::kTab,
                                              regular_url)));
  }
  {
    // Now verify the incognito contexts. Here, too, there should be a single
    // background context and tab, but it should be the incognito tab.
    base::Value incognito_results =
        run_get_contexts_in_profile(incognito_browser->profile());
    std::vector<api::runtime::ExtensionContext> contexts =
        ContextValueToContextStructs(incognito_results);
    EXPECT_THAT(contexts, testing::UnorderedElementsAre(
                              GetBackgroundMatcher(),
                              GetFrameMatcher(api::runtime::ContextType::kTab,
                                              incognito_url)));
  }
}

// Tests the behavior of `runtime.getContexts()` with a spanning-mode incognito
// extension.
IN_PROC_BROWSER_TEST_F(RuntimeGetContextsApiTest,
                       RetrievingIncognitoContexts_SpanningMode) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load up a spanning mode extension. See comment below for why we have
  // a web accessible resource.
  static constexpr char kManifest[] =
      R"({
           "name": "Split mode extension",
           "version": "0.1",
           "manifest_version": 3,
           "incognito": "spanning",
           "web_accessible_resources": [{
             "resources": ["incognito.html"],
             "matches": ["*://example.com/*"]
           }],
           "background": {"service_worker": "background.js"}
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     "// Intentionally blank");
  test_dir.WriteFile(FILE_PATH_LITERAL("regular.html"), "<html>Regular</html>");
  test_dir.WriteFile(FILE_PATH_LITERAL("incognito.html"),
                     "<html>Incognito</html>");

  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(extension);

  // Open an on-the-record tab to an extension page.
  GURL regular_url = extension->GetResourceURL("regular.html");
  content::RenderFrameHost* regular_host =
      ui_test_utils::NavigateToURL(browser(), regular_url);
  ASSERT_TRUE(regular_host);

  // Now, the tricky part. Spanning mode extensions aren't, typically, allowed
  // to open contexts in an incognito profile (which means all contexts just
  // open in the same profile). There's one exception to this: an embedded web-
  // accessible iframe in an incognito tab. Make it so.
  GURL incognito_url = extension->GetResourceURL("incognito.html");
  Browser* incognito_browser = OpenURLOffTheRecord(
      profile(), embedded_test_server()->GetURL("example.com", "/simple.html"));
  // Inject a script to add an iframe and navigate it to the extension's
  // web-accessible resource.
  content::RenderFrameHost* incognito_main_frame =
      incognito_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame();
  static constexpr char kNavigateTemplate[] =
      R"(let frame = document.createElement('iframe');
         frame.src = '%s';
         new Promise(resolve => {
           frame.onload = () => { resolve('success'); };
           frame.onerror = (e) => {
             resolve('failure: ' + e.toString());
           };
           document.body.appendChild(frame);
         });
         )";

  EXPECT_EQ("success",
            content::EvalJs(incognito_main_frame,
                            base::StringPrintf(kNavigateTemplate,
                                               incognito_url.spec().c_str())));

  // Verify the frame loaded properly by checking both the URL and the content.
  content::RenderFrameHost* incognito_extension_frame =
      content::ChildFrameAt(incognito_main_frame, 0);
  ASSERT_TRUE(incognito_extension_frame);
  EXPECT_EQ(incognito_url, incognito_extension_frame->GetLastCommittedURL());
  EXPECT_EQ("Incognito", content::EvalJs(incognito_extension_frame,
                                         "document.body.textContent;"));

  // A helper method to retrieve the contexts for the given `profile`.
  auto run_get_contexts_in_profile = [extension](Profile* profile) {
    static constexpr char kScript[] =
        R"((async () => {
             chrome.test.sendScriptResult(
                 await chrome.runtime.getContexts({}));
           })();)";
    return BackgroundScriptExecutor::ExecuteScript(
        profile, extension->id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  {
    // Verify the results for the on-the-record profile. Since the extension
    // is in spanning mode, this is effectively the only instance of the
    // extension. It should see the background context and the on-the-record
    // tab, but not the embedded frame.
    base::Value regular_results = run_get_contexts_in_profile(profile());
    std::vector<api::runtime::ExtensionContext> contexts =
        ContextValueToContextStructs(regular_results);
    EXPECT_THAT(contexts, testing::UnorderedElementsAre(
                              GetBackgroundMatcher(),
                              GetFrameMatcher(api::runtime::ContextType::kTab,
                                              regular_url)));
  }
}

// This is a manifest V2 test meant to ensure test coverage for
// chrome.extension.getURL, which is deprecated and unavailable
// in MV3.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, GetExtensionURL) {
  static constexpr char kManifest[] = R"(
      {
        "name": "chrome.extension.getURL",
        "version": "1.0",
        "background": {
          "scripts": ["background.js"]
        },
        "manifest_version": 2
      })";

  static constexpr char kScript[] = R"(
    chrome.test.assertEq(
        chrome.extension.getURL('foo.html'),
        chrome.runtime.getURL('foo.html'));
    chrome.test.notifyPass();
  )";

  ResultCatcher catcher;
  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), kScript);

  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult());
}

// Tests retrieving contexts when developer tools are opened.
class GetContextsWithDeveloperToolsOpened
    : public RuntimeGetContextsApiTest,
      public testing::WithParamInterface<bool> {
 public:
  GetContextsWithDeveloperToolsOpened() = default;
  ~GetContextsWithDeveloperToolsOpened() override = default;

  GetContextsWithDeveloperToolsOpened(
      const GetContextsWithDeveloperToolsOpened&) = delete;
  GetContextsWithDeveloperToolsOpened& operator=(
      const GetContextsWithDeveloperToolsOpened&) = delete;
};

// TODO(crbug.com/357845909): flaky on ChromeOS and Linux MSAN.
#if defined(MEMORY_SANITIZER) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
#define MAYBE_ReturnsDevToolsContext DISABLED_ReturnsDevToolsContext
#else
#define MAYBE_ReturnsDevToolsContext ReturnsDevToolsContext
#endif
IN_PROC_BROWSER_TEST_P(GetContextsWithDeveloperToolsOpened,
                       MAYBE_ReturnsDevToolsContext) {
  const bool open_docked = GetParam();

  // Open the developer tools and wait for the extension page to be loaded.
  ExtensionTestMessageListener listener("devtools page opened");
  content::WebContents* inspected_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(inspected_web_contents,
                                                    open_docked);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Assert the docked state of developer tools.
  content::WebContents* devtools_web_contents =
      DevToolsWindowTesting::Get(devtools_window)->main_web_contents();
  bool is_docked = devtools_web_contents->GetTopLevelNativeWindow() ==
                   browser()->window()->GetNativeWindow();
  ASSERT_EQ(open_docked, is_docked);

  // Extract the extension host from the devtools web contents.
  GURL expected_frame_url = extension().GetResourceURL("devtools.html");
  auto is_extension_frame =
      [expected_frame_url](content::RenderFrameHost* rfh) {
        return rfh->GetLastCommittedURL() == expected_frame_url;
      };
  content::RenderFrameHost* extension_host = content::FrameMatchingPredicate(
      devtools_web_contents->GetPrimaryPage(),
      base::BindLambdaForTesting(is_extension_frame));

  // Setup the expected values for the context. Only one tab-based context
  // should be returned by chrome.runtime.getContexts().
  int expected_tab_id = -1;
  int expected_window_id = ExtensionTabUtil::GetWindowIdOfTab(
      is_docked ? inspected_web_contents : devtools_web_contents);
  int expected_frame_id = -1;
  std::string expected_context_id =
      ExtensionApiFrameIdMap::GetContextId(extension_host).AsLowercaseString();
  std::string expected_document_id =
      ExtensionApiFrameIdMap::GetDocumentId(extension_host).ToString();
  std::string expected_origin = extension().origin().Serialize();
  static constexpr char kExpectedTemplate[] =
      R"([{
            "contextType": "DEVELOPER_TOOLS",
            "contextId": "%s",
            "tabId": %d,
            "windowId": %d,
            "frameId": %d,
            "documentId": "%s",
            "documentUrl": "%s",
            "documentOrigin": "%s",
            "incognito": false
         }])";
  std::string expected_contexts = base::StringPrintf(
      kExpectedTemplate, expected_context_id.c_str(), expected_tab_id,
      expected_window_id, expected_frame_id, expected_document_id.c_str(),
      expected_frame_url.spec().c_str(), expected_origin.c_str());

  // Verify the result of chrome.runtime.getContexts().
  base::Value contexts =
      GetContexts(R"({"contextTypes": ["DEVELOPER_TOOLS"]})");
  EXPECT_THAT(contexts, base::test::IsJson(expected_contexts));
}

// Test for undocked developer tools.
INSTANTIATE_TEST_SUITE_P(UndockedDevTools,
                         GetContextsWithDeveloperToolsOpened,
                         ::testing::Values(false) /* open_docked */);

// Test for docked developer tools. This is also a regression test for
// crbug.com/355625882.
INSTANTIATE_TEST_SUITE_P(DockedDevTools,
                         GetContextsWithDeveloperToolsOpened,
                         ::testing::Values(true) /* open_docked */);

}  // namespace extensions
