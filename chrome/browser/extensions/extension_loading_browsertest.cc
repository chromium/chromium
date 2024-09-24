// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains tests for extension loading, reloading, and
// unloading behavior.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace extensions {
namespace {

constexpr char kChangeBackgroundScriptTypeExtensionId[] =
    "ldnnhddmnhbkjipkidpdiheffobcpfmf";
using ContextType = ExtensionBrowserTest::ContextType;

class ExtensionLoadingTest : public ExtensionBrowserTest {
};

// Check the fix for http://crbug.com/178542.
IN_PROC_BROWSER_TEST_F(ExtensionLoadingTest,
                       UpgradeAfterNavigatingFromOverriddenNewTabPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestExtensionDir extension_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Overrides New Tab",
           "version": "%d",
           "description": "Overrides New Tab",
           "manifest_version": 2,
           "background": {
             "persistent": false,
             "scripts": ["event.js"]
           },
           "chrome_url_overrides": {
             "newtab": "newtab.html"
           }
         })";
  extension_dir.WriteManifest(base::StringPrintf(kManifestTemplate, 1));
  extension_dir.WriteFile(FILE_PATH_LITERAL("event.js"), "");
  extension_dir.WriteFile(FILE_PATH_LITERAL("newtab.html"),
                          "<h1>Overridden New Tab Page</h1>");

  const Extension* new_tab_extension =
      InstallExtension(extension_dir.Pack(), 1 /*new install*/);
  ASSERT_TRUE(new_tab_extension);

  // Visit the New Tab Page to get a renderer using the extension into history.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));

  // Navigate that tab to a non-extension URL to swap out the extension's
  // renderer.
  const GURL test_link_from_NTP =
      embedded_test_server()->GetURL("/README.chromium");
  EXPECT_THAT(test_link_from_NTP.spec(), testing::EndsWith("/README.chromium"))
      << "Check that the test server started.";
  EXPECT_TRUE(
      NavigateInRenderer(browser()->tab_strip_model()->GetActiveWebContents(),
                         test_link_from_NTP));

  // Increase the extension's version.
  extension_dir.WriteManifest(base::StringPrintf(kManifestTemplate, 2));

  // Upgrade the extension.
  new_tab_extension = UpdateExtension(
      new_tab_extension->id(), extension_dir.Pack(), 0 /*expected upgrade*/);
  EXPECT_THAT(new_tab_extension->version().components(),
              testing::ElementsAre(2));

  // The extension takes a couple round-trips to the renderer in order
  // to crash, so open a new tab to wait long enough.
  ASSERT_FALSE(AddTabAtIndex(browser()->tab_strip_model()->count(),
                             GURL("http://www.google.com/"),
                             ui::PAGE_TRANSITION_TYPED));

  // Check that the extension hasn't crashed.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_EQ(0U, registry->terminated_extensions().size());
  EXPECT_TRUE(registry->enabled_extensions().Contains(new_tab_extension->id()));
}

IN_PROC_BROWSER_TEST_F(ExtensionLoadingTest,
                       UpgradeAddingNewTabPagePermissionNoPrompt) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestExtensionDir extension_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Overrides New Tab",
           "version": "%d",
           "description": "Will override New Tab soon",
           %s  // Placeholder for future NTP url override block.
           "manifest_version": 2
         })";
  extension_dir.WriteManifest(base::StringPrintf(kManifestTemplate, 1, ""));
  extension_dir.WriteFile(FILE_PATH_LITERAL("event.js"), "");
  extension_dir.WriteFile(FILE_PATH_LITERAL("newtab.html"),
                          "<h1>Overridden New Tab Page</h1>");

  const Extension* new_tab_extension =
      InstallExtension(extension_dir.Pack(), 1 /*new install*/);
  ASSERT_TRUE(new_tab_extension);

  EXPECT_FALSE(new_tab_extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kNewTabPageOverride));

  // Navigate that tab to a non-extension URL to swap out the extension's
  // renderer.
  const GURL test_link_from_ntp =
      embedded_test_server()->GetURL("/README.chromium");
  EXPECT_THAT(test_link_from_ntp.spec(), testing::EndsWith("/README.chromium"))
      << "Check that the test server started.";
  EXPECT_TRUE(
      NavigateInRenderer(browser()->tab_strip_model()->GetActiveWebContents(),
                         test_link_from_ntp));

  // Increase the extension's version and add the NTP url override which will
  // add the kNewTabPageOverride permission.
  constexpr char kNtpOverrideString[] =
      R"("chrome_url_overrides": {
            "newtab": "newtab.html"
         },)";
  extension_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, 2, kNtpOverrideString));

  // Upgrade the extension, ensure that the upgrade 'worked' in the sense that
  // the extension is still present and not disabled and that it now has the
  // new API permission.
  // TODO(robertshield): Update this once most of the population is on M62+
  // and adding NTP permissions implies a permission upgrade.
  new_tab_extension = UpdateExtension(
      new_tab_extension->id(), extension_dir.Pack(), 0 /*expected upgrade*/);
  ASSERT_NE(nullptr, new_tab_extension);

  EXPECT_TRUE(new_tab_extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kNewTabPageOverride));
  EXPECT_THAT(new_tab_extension->version().components(),
              testing::ElementsAre(2));
}

// Tests the behavior described in http://crbug.com/532088.
IN_PROC_BROWSER_TEST_F(ExtensionLoadingTest,
                       KeepAliveWithDevToolsOpenOnReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestExtensionDir extension_dir;
  const char manifest_contents[] =
      R"({
           "name": "Test With Lazy Background Page",
           "version": "0",
           "manifest_version": 2,
           "app": {
             "background": {
                "scripts": ["event.js"]
             }
           }
         })";
  extension_dir.WriteManifest(manifest_contents);
  extension_dir.WriteFile(FILE_PATH_LITERAL("event.js"), "");

  const Extension* extension =
      InstallExtension(extension_dir.Pack(), 1 /*new install*/);
  ASSERT_TRUE(extension);
  std::string extension_id = extension->id();
  const auto dev_tools_activity =
      std::make_pair(Activity::DEV_TOOLS, std::string());

  ProcessManager* process_manager = ProcessManager::Get(profile());
  EXPECT_EQ(0, process_manager->GetLazyKeepaliveCount(extension));
  ProcessManager::ActivitiesMultiset activities =
      process_manager->GetLazyKeepaliveActivities(extension);
  EXPECT_TRUE(activities.empty());

  DevToolsWindowCreationObserver observer;
  devtools_util::InspectBackgroundPage(extension, profile(),
                                       DevToolsOpenedByAction::kUnknown);
  observer.WaitForLoad();

  // This is due to how these keepalive counters are managed by the extension
  // process manager:
  // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:extensions/browser/process_manager.cc;drc=8ce14ef97f8607b1b57f8d02da575ed5150eea9e;l=924
  // It bumps them each time it sees a DevToolsAgentHost associated to an
  // extension, and in case of the tab target mode, there's one agent host for
  // the WebContents and one for the render frame.
  const int expected_keepalive_count = 2;

  EXPECT_EQ(expected_keepalive_count,
            process_manager->GetLazyKeepaliveCount(extension));
  EXPECT_THAT(activities, testing::Each(dev_tools_activity));

  // Opening DevTools will cause the background page to load. Wait for it.
  WaitForExtensionViewsToLoad();

  ReloadExtension(extension_id);

  // Flush the MessageLoop to ensure that DevTools has a chance to be reattached
  // and the background page has a chance to begin reloading.
  base::RunLoop().RunUntilIdle();

  // And wait for the background page to finish loading again.
  WaitForExtensionViewsToLoad();

  // Ensure that our DevtoolsAgentHost is actually connected to the new
  // background WebContents.
  content::WebContents* background_contents =
      process_manager->GetBackgroundHostForExtension(extension_id)
          ->host_contents();
  EXPECT_TRUE(content::DevToolsAgentHost::HasFor(background_contents));

  // The old Extension object is no longer valid.
  extension = ExtensionRegistry::Get(profile())
      ->enabled_extensions().GetByID(extension_id);

  // Keepalive count should stabilize back to original count, because DevTools
  // is still open.
  EXPECT_EQ(expected_keepalive_count,
            process_manager->GetLazyKeepaliveCount(extension));
  activities = process_manager->GetLazyKeepaliveActivities(extension);
  EXPECT_THAT(activities, testing::Each(dev_tools_activity));
}

// Tests whether the extension runtime stays valid when an extension reloads
// while a devtools extension is hammering the frame with eval requests.
// Regression test for https://crbug.com/544182
// TODO(crbug.com/40893499): Flaky with dbg and sanitizers.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_RuntimeValidWhileDevToolsOpen \
  DISABLED_RuntimeValidWhileDevToolsOpen
#else
#define MAYBE_RuntimeValidWhileDevToolsOpen RuntimeValidWhileDevToolsOpen
#endif
IN_PROC_BROWSER_TEST_F(ExtensionLoadingTest,
                       MAYBE_RuntimeValidWhileDevToolsOpen) {
  TestExtensionDir devtools_dir;
  TestExtensionDir inspect_dir;

  constexpr char kDevtoolsManifest[] =
      R"({
           "name": "Devtools",
           "version": "1",
           "manifest_version": 2,
           "devtools_page": "devtools.html"
         })";

  constexpr char kDevtoolsJs[] =
      R"(setInterval(function() {
           chrome.devtools.inspectedWindow.eval('1', function() {
           });
         }, 4);
         chrome.test.sendMessage('devtools_page_ready');)";

  constexpr char kTargetManifest[] =
      R"({
           "name": "Inspect target",
           "version": "1",
           "manifest_version": 2,
           "background": {
             "scripts": ["background.js"]
           }
         })";

  // A script to duck-type whether it runs in a background page.
  const char kTargetJs[] =
      "var is_valid = !!(chrome.tabs && chrome.tabs.create);";

  devtools_dir.WriteManifest(kDevtoolsManifest);
  devtools_dir.WriteFile(FILE_PATH_LITERAL("devtools.js"), kDevtoolsJs);
  devtools_dir.WriteFile(FILE_PATH_LITERAL("devtools.html"),
                         "<script src='devtools.js'></script>");

  inspect_dir.WriteManifest(kTargetManifest);
  inspect_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kTargetJs);
  const Extension* devtools_ext = LoadExtension(devtools_dir.UnpackedPath());
  ASSERT_TRUE(devtools_ext);

  const Extension* inspect_ext = LoadExtension(inspect_dir.UnpackedPath());
  ASSERT_TRUE(inspect_ext);
  const std::string inspect_ext_id = inspect_ext->id();

  // Open the devtools and wait until the devtools_page is ready.
  ExtensionTestMessageListener devtools_ready("devtools_page_ready");
  devtools_util::InspectBackgroundPage(inspect_ext, profile(),
                                       DevToolsOpenedByAction::kUnknown);
  ASSERT_TRUE(devtools_ready.WaitUntilSatisfied());

  // Reload the extension. The devtools window will stay open, but temporarily
  // be detached. As soon as the background is attached again, the devtools
  // continues with spamming eval requests.
  ReloadExtension(inspect_ext_id);
  WaitForExtensionViewsToLoad();

  content::WebContents* bg_contents =
      ProcessManager::Get(profile())
          ->GetBackgroundHostForExtension(inspect_ext_id)
          ->host_contents();
  ASSERT_TRUE(bg_contents);

  // Now check whether the extension runtime is valid (see kTargetJs).
  EXPECT_EQ(true, content::EvalJs(bg_contents, "is_valid;"));

  // Tidy up.
  scoped_refptr<content::DevToolsAgentHost> agent_host(
      content::DevToolsAgentHost::GetOrCreateForTab(bg_contents));
  DevToolsWindowTesting::CloseDevToolsWindowSync(
      DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

// Tests that changing a Service Worker based extension to an event page doesn't
// crash. Regression test for https://crbug.com/1239752.
//
// This test loads a SW based extension that has an event listener for
// chrome.tabs.onCreated. The event would be registered in ExtensionPrefs. The
// test then changes the extension to event page and ensures that restarting the
// browser wouldn't route the event incorrectly to ServiceWorkerTaskQueue (which
// used to cause a crash).
IN_PROC_BROWSER_TEST_F(ExtensionLoadingTest, PRE_ChangeBackgroundScriptType) {
  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("manifest_changed_before_restart"),
      {.context_type = ContextType::kServiceWorker});
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionId extension_id = extension->id();
  EXPECT_TRUE(BackgroundInfo::IsServiceWorkerBased(extension));

  // Change |extension| to become an event page extension.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath event_page_manifest_file =
        extension->path().Append(FILE_PATH_LITERAL("manifest.json"));
    ASSERT_TRUE(base::PathExists(event_page_manifest_file));
    EXPECT_TRUE(base::CopyFile(
        test_data_dir_.AppendASCII("manifest_changed_before_restart")
            .Append(FILE_PATH_LITERAL("manifest.json")),
        event_page_manifest_file));
  }

  // Ensure that tabs.onCreated SW event was registered.
  // It is sufficient that a "lazy" event is present because we already know
  // that |extension| is SW based.
  EXPECT_TRUE(EventRouter::Get(profile())->HasLazyEventListenerForTesting(
      api::tabs::OnCreated::kEventName));
}

IN_PROC_BROWSER_TEST_F(ExtensionLoadingTest, ChangeBackgroundScriptType) {
  // The goal of this test step is to not crash.
  const Extension* extension =
      extension_registry()->enabled_extensions().GetByID(
          kChangeBackgroundScriptTypeExtensionId);
  ASSERT_TRUE(extension);

  // |extension| should not run as SW based after browser restart as it became
  // an event page extension.
  EXPECT_FALSE(BackgroundInfo::IsServiceWorkerBased(extension));
}

}  // namespace
}  // namespace extensions
