// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// A fake webstore domain.
const char kWebstoreDomain[] = "cws.com";

// Check whether or not style was injected, with |expected_injection| indicating
// the expected result. Also ensure that no CSS was added to the
// document.styleSheets array.
testing::AssertionResult CheckStyleInjection(Browser* browser,
                                             const GURL& url,
                                             bool expected_injection) {
  ui_test_utils::NavigateToURL(browser, url);

  bool css_injected = false;
  if (!content::ExecuteScriptAndExtractBool(
          browser->tab_strip_model()->GetActiveWebContents(),
          "window.domAutomationController.send("
          "    document.defaultView.getComputedStyle(document.body, null)."
          "        getPropertyValue('display') == 'none');",
          &css_injected)) {
    return testing::AssertionFailure()
        << "Failed to execute script and extract bool for injection status.";
  }

  if (css_injected != expected_injection) {
    std::string message;
    if (css_injected)
      message = "CSS injected when no injection was expected.";
    else
      message = "CSS not injected when injection was expected.";
    return testing::AssertionFailure() << message;
  }

  bool css_doesnt_add_to_list = false;
  if (!content::ExecuteScriptAndExtractBool(
          browser->tab_strip_model()->GetActiveWebContents(),
          "window.domAutomationController.send("
          "    document.styleSheets.length == 0);",
          &css_doesnt_add_to_list)) {
    return testing::AssertionFailure()
        << "Failed to execute script and extract bool for stylesheets length.";
  }
  if (!css_doesnt_add_to_list) {
    return testing::AssertionFailure()
        << "CSS injection added to number of stylesheets.";
  }

  return testing::AssertionSuccess();
}

// Runs all pending tasks in the renderer associated with |web_contents|, and
// then all pending tasks in the browser process.
// Returns true on success.
bool RunAllPending(content::WebContents* web_contents) {
  // This is slight hack to achieve a RunPendingInRenderer() method. Since IPCs
  // are sent synchronously, anything started prior to this method will finish
  // before this method returns (as content::ExecuteScript() is synchronous).
  if (!content::ExecuteScript(web_contents, "1 == 1;"))
    return false;
  base::RunLoop().RunUntilIdle();
  return true;
}

// A simple extension manifest with content scripts on all pages.
const char kManifest[] =
    "{"
    "  \"name\": \"%s\","
    "  \"version\": \"1.0\","
    "  \"manifest_version\": 2,"
    "  \"content_scripts\": [{"
    "    \"matches\": [\"*://*/*\"],"
    "    \"js\": [\"script.js\"],"
    "    \"run_at\": \"%s\""
    "  }]"
    "}";

// A (blocking) content script that pops up an alert.
const char kBlockingScript[] = "alert('ALERT');";

// A (non-blocking) content script that sends a message.
const char kNonBlockingScript[] = "chrome.test.sendMessage('done');";

const char kNewTabOverrideManifest[] =
    "{"
    "  \"name\": \"New tab override\","
    "  \"version\": \"0.1\","
    "  \"manifest_version\": 2,"
    "  \"description\": \"Foo!\","
    "  \"chrome_url_overrides\": {\"newtab\": \"newtab.html\"}"
    "}";

const char kNewTabHtml[] = "<html>NewTabOverride!</html>";

}  // namespace

class ContentScriptApiTest : public ExtensionApiTest {
 public:
  ContentScriptApiTest() {}
  ~ContentScriptApiTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ContentScriptApiTest);
};

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptAllFrames) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/all_frames")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptAboutBlankIframes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/about_blank_iframes"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptAboutBlankAndSrcdoc) {
  // The optional "*://*/*" permission is requested after verifying that
  // content script insertion solely depends on content_scripts[*].matches.
  // The permission is needed for chrome.tabs.executeScript tests.
  PermissionsRequestFunction::SetAutoConfirmForTests(true);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/about_blank_srcdoc"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptExtensionIframe) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/extension_iframe")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptExtensionProcess) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("content_scripts/extension_process")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptFragmentNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const char extension_name[] = "content_scripts/fragment";
  ASSERT_TRUE(RunExtensionTest(extension_name)) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptIsolatedWorlds) {
  // This extension runs various bits of script and tests that they all run in
  // the same isolated world.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/isolated_world1")) << message_;

  // Now load a different extension, inject into same page, verify worlds aren't
  // shared.
  ASSERT_TRUE(RunExtensionTest("content_scripts/isolated_world2")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptIgnoreHostPermissions) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest(
      "content_scripts/dont_match_host_permissions")) << message_;
}

// crbug.com/39249 -- content scripts js should not run on view source.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptViewSource) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/view_source")) << message_;
}

// crbug.com/126257 -- content scripts should not get injected into other
// extensions.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptOtherExtensions) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // First, load extension that sets up content script.
  ASSERT_TRUE(RunExtensionTest("content_scripts/other_extensions/injector"))
      << message_;
  // Then load targeted extension to make sure its content isn't changed.
  ASSERT_TRUE(RunExtensionTest("content_scripts/other_extensions/victim"))
      << message_;
}

// https://crbug.com/825111 -- content scripts may fetch() a blob URL from their
// chrome-extension:// origin.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptBlobFetch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/blob_fetch")) << message_;
}

class ContentScriptCssInjectionTest : public ExtensionApiTest {
 protected:
  // TODO(rdevlin.cronin): Make a testing switch that looks like FeatureSwitch,
  // but takes in an optional value so that we don't have to do this.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // We change the Webstore URL to be http://cws.com. We need to do this so
    // we can check that css injection is not allowed on the webstore (which
    // could lead to spoofing). Unfortunately, host_resolver seems to have
    // problems with redirecting "chrome.google.com" to the test server, so we
    // can't use the real Webstore's URL. If this changes, we could clean this
    // up.
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryURL,
        base::StringPrintf("http://%s", kWebstoreDomain));
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptDuplicateScriptInjection) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL url(
      base::StringPrintf("http://maps.google.com:%i/extensions/test_file.html",
                         embedded_test_server()->port()));

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "content_scripts/duplicate_script_injection")));

  ui_test_utils::NavigateToURL(browser(), url);

  // Test that a script that matches two separate, yet overlapping match
  // patterns is only injected once.
  bool scripts_injected_once = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "document.getElementsByClassName('injected-once')"
      ".length == 1)",
      &scripts_injected_once));
  ASSERT_TRUE(scripts_injected_once);

  // Test that a script injected at two different load process times, document
  // idle and document end, is injected exactly twice.
  bool scripts_injected_twice = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "document.getElementsByClassName('injected-twice')"
      ".length == 2)",
      &scripts_injected_twice));
  ASSERT_TRUE(scripts_injected_twice);
}

IN_PROC_BROWSER_TEST_F(ContentScriptCssInjectionTest,
                       ContentScriptInjectsStyles) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("content_scripts")
                                          .AppendASCII("css_injection")));

  // CSS injection should be allowed on an aribitrary web page.
  GURL url =
      embedded_test_server()->GetURL("/extensions/test_file_with_body.html");
  EXPECT_TRUE(CheckStyleInjection(browser(), url, true));

  // The loaded extension has an exclude match for "extensions/test_file.html",
  // so no CSS should be injected.
  url = embedded_test_server()->GetURL("/extensions/test_file.html");
  EXPECT_TRUE(CheckStyleInjection(browser(), url, false));

  // We disallow all injection on the webstore.
  GURL::Replacements replacements;
  replacements.SetHostStr(kWebstoreDomain);
  url = embedded_test_server()->GetURL("/extensions/test_file_with_body.html")
            .ReplaceComponents(replacements);
  EXPECT_TRUE(CheckStyleInjection(browser(), url, false));
}

// crbug.com/120762
IN_PROC_BROWSER_TEST_F(
    ExtensionApiTest,
    DISABLED_ContentScriptStylesInjectedIntoExistingRenderers) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  content::WindowedNotificationObserver signal(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(browser()->profile()));

  // Start with a renderer already open at a URL.
  GURL url(embedded_test_server()->GetURL("/extensions/test_file.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/existing_renderers"));

  signal.Wait();

  // And check that its styles were affected by the styles that just got loaded.
  bool styles_injected;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "    document.defaultView.getComputedStyle(document.body, null)."
      "        getPropertyValue('background-color') == 'rgb(255, 0, 0)')",
      &styles_injected));
  ASSERT_TRUE(styles_injected);
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptCSSLocalization) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/css_l10n")) << message_;
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptExtensionAPIs) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // TODO(https://crbug.com/898682): Waiting for content scripts to load should
  // be done as part of the extension loading process.
  content::WindowedNotificationObserver scripts_updated_observer(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::NotificationService::AllSources());
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/extension_api"));
  scripts_updated_observer.Wait();

  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/content_scripts/extension_api/functions.html"));
  EXPECT_TRUE(catcher.GetNextResult());

  // Navigate to a page that will cause a content script to run that starts
  // listening for an extension event.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/content_scripts/extension_api/events.html"));

  // Navigate to an extension page that will fire the event events.js is
  // listening for.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("fire_event.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptPermissionsApi) {
  extensions::PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  extensions::PermissionsRequestFunction::SetAutoConfirmForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       ContentScriptPolicy) {
  // Set enterprise policy to block injection to policy specified host.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://example.com");
  }
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/policy")) << message_;
}

// Verifies wildcard can NOT be used for effective TLD.
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       ContentScriptPolicyWildcard) {
  // Set enterprise policy to block injection to policy specified hosts.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://example.*");
  }
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_FALSE(RunExtensionTest("content_scripts/policy")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       ContentScriptPolicyByExtensionId) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("content_scripts/policy");
  // Pack extension because by-extension policies aren't applied to unpacked
  // "transient" extensions.
  base::FilePath crx_path = PackExtension(extension_path);
  EXPECT_FALSE(crx_path.empty());

  // Load first time to get extension id.
  const Extension* extension = LoadExtensionWithFlags(
      crx_path, ExtensionBrowserTest::kFlagEnableFileAccess);
  ASSERT_TRUE(extension);
  auto extension_id = extension->id();
  UnloadExtension(extension_id);

  // Set enterprise policy to block injection of specified extension to policy
  // specified host.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost(extension_id, "*://example.com");
  }
  // Some policy updating operations are performed asynchronuosly. Wait for them
  // to complete before installing extension.
  base::RunLoop().RunUntilIdle();

  extensions::ResultCatcher catcher;
  EXPECT_TRUE(LoadExtensionWithFlags(
      crx_path, ExtensionBrowserTest::kFlagEnableFileAccess));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptBypassPageCSP) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_scripts/bypass_page_csp")) << message_;
}

// Test that when injecting a blocking content script, other scripts don't run
// until the blocking script finishes.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptBlockingScript) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load up two extensions.
  TestExtensionDir ext_dir1;
  ext_dir1.WriteManifest(
      base::StringPrintf(kManifest, "ext1", "document_start"));
  ext_dir1.WriteFile(FILE_PATH_LITERAL("script.js"), kBlockingScript);
  const Extension* ext1 = LoadExtension(ext_dir1.UnpackedPath());
  ASSERT_TRUE(ext1);

  TestExtensionDir ext_dir2;
  ext_dir2.WriteManifest(base::StringPrintf(kManifest, "ext2", "document_end"));
  ext_dir2.WriteFile(FILE_PATH_LITERAL("script.js"), kNonBlockingScript);
  const Extension* ext2 = LoadExtension(ext_dir2.UnpackedPath());
  ASSERT_TRUE(ext2);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(web_contents);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());

  ExtensionTestMessageListener listener("done", false);
  listener.set_extension_id(ext2->id());

  // Navigate! Both extensions will try to inject.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  dialog_wait.Run();
  // Right now, the alert dialog is showing and blocking injection of anything
  // after it, so the listener shouldn't be satisfied.
  EXPECT_FALSE(listener.was_satisfied());
  js_helper->HandleJavaScriptDialog(web_contents, true, nullptr);

  // After closing the dialog, the rest of the scripts should be able to
  // inject.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Test that closing a tab with a blocking script results in no further scripts
// running (and we don't crash).
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptBlockingScriptTabClosed) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // We're going to close a tab in this test, so make a new one (to ensure
  // we don't close the browser).
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Set up the same as the previous test case.
  TestExtensionDir ext_dir1;
  ext_dir1.WriteManifest(
      base::StringPrintf(kManifest, "ext1", "document_start"));
  ext_dir1.WriteFile(FILE_PATH_LITERAL("script.js"), kBlockingScript);
  const Extension* ext1 = LoadExtension(ext_dir1.UnpackedPath());
  ASSERT_TRUE(ext1);

  TestExtensionDir ext_dir2;
  ext_dir2.WriteManifest(base::StringPrintf(kManifest, "ext2", "document_end"));
  ext_dir2.WriteFile(FILE_PATH_LITERAL("script.js"), kNonBlockingScript);
  const Extension* ext2 = LoadExtension(ext_dir2.UnpackedPath());
  ASSERT_TRUE(ext2);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(web_contents);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());

  ExtensionTestMessageListener listener("done", false);
  listener.set_extension_id(ext2->id());

  // Navigate!
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  // Now, instead of closing the dialog, just close the tab. Later scripts
  // should never get a chance to run (and we shouldn't crash).
  dialog_wait.Run();
  EXPECT_FALSE(listener.was_satisfied());
  EXPECT_TRUE(browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), 0));
  EXPECT_FALSE(listener.was_satisfied());
}

// There was a bug by which content scripts that blocked and ran on
// document_idle could be injected twice (crbug.com/431263). Test for
// regression.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       ContentScriptBlockingScriptsDontRunTwice) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load up an extension.
  TestExtensionDir ext_dir1;
  ext_dir1.WriteManifest(
      base::StringPrintf(kManifest, "ext1", "document_idle"));
  ext_dir1.WriteFile(FILE_PATH_LITERAL("script.js"), kBlockingScript);
  // TODO(https://crbug.com/898682): Waiting for content scripts to load should
  // be done as part of the extension loading process.
  content::WindowedNotificationObserver scripts_updated_observer(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::NotificationService::AllSources());
  const Extension* ext1 = LoadExtension(ext_dir1.UnpackedPath());
  scripts_updated_observer.Wait();
  ASSERT_TRUE(ext1);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(web_contents);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());

  // Navigate!
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  dialog_wait.Run();

  // The extension will have injected at idle, but it should only inject once.
  js_helper->HandleJavaScriptDialog(web_contents, true, nullptr);
  EXPECT_TRUE(RunAllPending(web_contents));
  EXPECT_FALSE(js_helper->IsShowingDialogForTesting());
}

// Bug fix for crbug.com/507461.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       DocumentStartInjectionFromExtensionTabNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir new_tab_override_dir;
  new_tab_override_dir.WriteManifest(kNewTabOverrideManifest);
  new_tab_override_dir.WriteFile(FILE_PATH_LITERAL("newtab.html"), kNewTabHtml);
  const Extension* new_tab_override =
      LoadExtension(new_tab_override_dir.UnpackedPath());
  ASSERT_TRUE(new_tab_override);

  TestExtensionDir injector_dir;
  injector_dir.WriteManifest(
      base::StringPrintf(kManifest, "injector", "document_start"));
  injector_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kNonBlockingScript);
  const Extension* injector = LoadExtension(injector_dir.UnpackedPath());
  ASSERT_TRUE(injector);

  ExtensionTestMessageListener listener("done", false);
  AddTabAtIndex(0, GURL("chrome://newtab"), ui::PAGE_TRANSITION_LINK);
  browser()->tab_strip_model()->ActivateTabAt(0, false);
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(new_tab_override->GetResourceURL("newtab.html"),
            tab_contents->GetMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(listener.was_satisfied());
  listener.Reset();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(listener.was_satisfied());
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest,
                       DontInjectContentScriptsInBackgroundPages) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load two extensions, one with an iframe to a.com in its background page,
  // the other, a content script for a.com. The latter should never be able to
  // inject the script, because scripts aren't allowed to run on foreign
  // extensions' pages.
  base::FilePath data_dir = test_data_dir_.AppendASCII("content_scripts");
  ExtensionTestMessageListener iframe_loaded_listener("iframe loaded", false);
  ExtensionTestMessageListener content_script_listener("script injected",
                                                       false);
  LoadExtension(data_dir.AppendASCII("script_a_com"));
  LoadExtension(data_dir.AppendASCII("background_page_iframe"));
  EXPECT_TRUE(iframe_loaded_listener.WaitUntilSatisfied());
  EXPECT_FALSE(content_script_listener.was_satisfied());
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, CannotScriptTheNewTabPage) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener test_listener("ready", true);
  LoadExtension(test_data_dir_.AppendASCII("content_scripts/ntp"));
  ASSERT_TRUE(test_listener.WaitUntilSatisfied());

  auto did_script_inject = [](content::WebContents* web_contents) {
    bool did_inject = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "domAutomationController.send(document.title === 'injected');",
        &did_inject));
    return did_inject;
  };

  // First, test the executeScript() method.
  ResultCatcher catcher;
  test_listener.Reply(std::string());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(search::IsInstantNTP(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_FALSE(
      did_script_inject(browser()->tab_strip_model()->GetActiveWebContents()));

  // Next, check content script injection.
  ui_test_utils::NavigateToURL(browser(), search::GetNewTabPageURL(profile()));
  EXPECT_FALSE(
      did_script_inject(browser()->tab_strip_model()->GetActiveWebContents()));

  // The extension should inject on "normal" urls.
  GURL unprotected_url = embedded_test_server()->GetURL(
      "example.com", "/extensions/test_file.html");
  ui_test_utils::NavigateToURL(browser(), unprotected_url);
  EXPECT_TRUE(
      did_script_inject(browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ContentScriptSameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("a.com", "/extensions/body1.html");
  ResultCatcher catcher;
  constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, () => {
           let message = 'success';
           if (chrome.runtime.lastError)
             message = chrome.runtime.lastError.message;
           domAutomationController.send(message);
         });)";
  std::string result = browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension->id(),
      base::StringPrintf(kScript, url.spec().c_str()));

  EXPECT_EQ("success", result);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ExecuteScriptFileSameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("b.com", "/extensions/body1.html");
  ResultCatcher catcher;
  constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, (tab) => {
           if (chrome.runtime.lastError) {
             domAutomationController.send(chrome.runtime.lastError.message);
             return;
           }
           chrome.tabs.executeScript(tab.id, {file: 'cookies.js'}, () => {
             let message = 'success';
             if (chrome.runtime.lastError)
               message = chrome.runtime.lastError.message;
             domAutomationController.send(message);
           });
         });)";
  std::string result = browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension->id(),
      base::StringPrintf(kScript, url.spec().c_str()));

  EXPECT_EQ("success", result);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ExecuteScriptCodeSameSiteCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/request_cookies"));
  ASSERT_TRUE(extension);
  GURL url = embedded_test_server()->GetURL("b.com", "/extensions/body1.html");
  ResultCatcher catcher;
  constexpr char kScript[] =
      R"(chrome.tabs.create({url: '%s'}, (tab) => {
           if (chrome.runtime.lastError) {
             domAutomationController.send(chrome.runtime.lastError.message);
             return;
           }
           fetch(chrome.runtime.getURL('cookies.js')).then((response) => {
             return response.text();
           }).then((text) => {
             chrome.tabs.executeScript(tab.id, {code: text}, () => {
               let message = 'success';
               if (chrome.runtime.lastError)
                 message = chrome.runtime.lastError.message;
               domAutomationController.send(message);
             });
           }).catch((e) => {
             domAutomationController.send(e);
           });
         });)";
  std::string result = browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension->id(),
      base::StringPrintf(kScript, url.spec().c_str()));

  EXPECT_EQ("success", result);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that extension content scripts can execute (including asynchronously
// through timeouts) in pages with Content-Security-Policy: sandbox.
// See https://crbug.com/811528.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, ExecuteScriptBypassingSandbox) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Bypass Sandbox CSP",
           "description": "Extensions should bypass a page's CSP sandbox.",
           "version": "0.1",
           "manifest_version": 2,
           "content_scripts": [{
             "matches": ["*://example.com:*/*"],
             "js": ["script.js"]
           }]
         })");
  test_dir.WriteFile(
      FILE_PATH_LITERAL("script.js"),
      R"(window.setTimeout(() => { chrome.test.notifyPass(); }, 10);)");

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = embedded_test_server()->GetURL(
      "example.com", "/extensions/page_with_sandbox_csp.html");
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests the cross-origin access of content scripts.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, CrossOriginXhr) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Cross Origin XHR",
           "description": "Content script cross origin XHR",
           "version": "0.1",
           "manifest_version": 2,
           "content_scripts": [{
             "matches": ["*://example.com:*/*"],
             "js": ["script.js"]
           }],
           "permissions": ["http://chromium.org:*/*"]
         })");
  constexpr char kScript[] =
      R"(document.getElementById('go-button').addEventListener(
             'click',
             function() {
               fetch('%s').then((response) => {
                 domAutomationController.send('Fetched');
               }).catch((e) => {
                 domAutomationController.send('Not Fetched');
               });
             });)";

  const GURL cross_origin_url =
      embedded_test_server()->GetURL("chromium.org", "/extensions/body1.html");
  test_dir.WriteFile(
      FILE_PATH_LITERAL("script.js"),
      base::StringPrintf(kScript, cross_origin_url.spec().c_str()));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL example_com_url = embedded_test_server()->GetURL(
      "example.com", "/extensions/page_with_button.html");
  ui_test_utils::NavigateToURL(browser(), example_com_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  constexpr char kClickButtonScript[] =
      "document.getElementById('go-button').click();";

  {
    // Since the extension has permission to access chromium.org, it should be
    // able to make a cross-origin fetch.
    content::DOMMessageQueue message_queue;
    content::ExecuteScriptAsync(web_contents, kClickButtonScript);
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ(R"("Fetched")", message);
  }

  extension_service()->DisableExtension(extension->id(),
                                        disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
      extension->id()));

  {
    // When the extension is unloaded, the content script remains injected
    // (since we don't currently have any means of "uninjecting" JS). However,
    // it should no longer have the extra cross-origin permissions, so a
    // cross-origin fetch should fail.
    // https://crbug.com/843381.
    content::DOMMessageQueue message_queue;
    content::ExecuteScriptAsync(web_contents, kClickButtonScript);
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ(R"("Not Fetched")", message);
  }
}

// Regression test for https://crbug.com/883526.
IN_PROC_BROWSER_TEST_F(ContentScriptApiTest, InifiniteLoopInGetEffectiveURL) {
  // Create an extension that injects content scripts into about:blank frames
  // (and therefore has a chance to trigger an infinite loop in
  // ScriptContext::GetEffectiveDocumentURL).
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Content scripts everywhere",
           "description": "Content scripts everywhere",
           "version": "0.1",
           "manifest_version": 2,
           "content_scripts": [{
             "matches": ["<all_urls>"],
             "all_frames": true,
             "match_about_blank": true,
             "js": ["script.js"]
           }],
           "permissions": ["*://*/*"],
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), "console.log('blah')");

  // Create an "infinite" loop for hopping over parent/opener:
  // subframe1 ---parent---> mainFrame ---opener--> subframe1 ...
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(web_contents,
                              R"(
                                  var iframe = document.createElement('iframe');
                                  document.body.appendChild(iframe);
                                  window.name = 'main-frame'; )"));
  content::RenderFrameHost* subframe1 = web_contents->GetAllFrames()[1];
  ASSERT_TRUE(
      content::ExecJs(subframe1, "var w = window.open('', 'main-frame');"));
  EXPECT_EQ(subframe1, web_contents->GetOpener());

  // Trigger GetEffectiveURL from another subframe:
  ASSERT_TRUE(content::ExecJs(web_contents,
                              R"(
                                  var iframe = document.createElement('iframe');
                                  document.body.appendChild(iframe); )"));

  // Verify that the renderer is still responsive / that the renderer didn't
  // enter an infinite loop.
  EXPECT_EQ(123, content::EvalJs(web_contents, "123"));
}

// Test fixture which sets a custom NTP Page.
// TODO(karandeepb): Similar logic to set up a custom NTP is used elsewhere as
// well. Abstract this away into a reusable test fixture class.
class NTPInterceptionTest : public ExtensionApiTest {
 public:
  NTPInterceptionTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  // ExtensionApiTest override:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    test_data_dir_ = test_data_dir_.AppendASCII("ntp_content_script");
    https_test_server_.ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(https_test_server_.Start());

    GURL ntp_url = https_test_server_.GetURL("/fake_ntp.html");
    local_ntp_test_utils::SetUserSelectedDefaultSearchProvider(
        profile(), https_test_server_.base_url().spec(), ntp_url.spec());
  }

  const net::EmbeddedTestServer* https_test_server() const {
    return &https_test_server_;
  }

 private:
  net::EmbeddedTestServer https_test_server_;
  DISALLOW_COPY_AND_ASSIGN(NTPInterceptionTest);
};

// Ensure extensions can't inject a content script into the New Tab page.
// Regression test for crbug.com/844428.
IN_PROC_BROWSER_TEST_F(NTPInterceptionTest, ContentScript) {
  // Load an extension which tries to inject a script into every frame.
  ExtensionTestMessageListener listener("ready", false /*will_reply*/);
  const Extension* extension = LoadExtension(test_data_dir_);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Create a corresponding off the record profile for the current profile. This
  // is necessary to reproduce crbug.com/844428, which occurs in part due to
  // incorrect handling of multiple profiles by the NTP code.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  ASSERT_TRUE(incognito_browser);

  // Ensure that the extension isn't able to inject the script into the New Tab
  // Page.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(search::IsInstantNTP(web_contents));

  bool script_injected_in_ntp = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send(document.title !== 'Fake NTP');",
      &script_injected_in_ntp));
  EXPECT_FALSE(script_injected_in_ntp);
}

}  // namespace extensions
