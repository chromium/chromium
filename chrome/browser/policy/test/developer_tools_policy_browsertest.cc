// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

using extensions::mojom::ManifestLocation;

namespace policy {

namespace {

// Utility for waiting until the dev-mode controls are visible/hidden
// Uses a MutationObserver on the attributes of the DOM element.
void WaitForExtensionsDevModeControlsVisibility(
    content::WebContents* contents,
    const char* dev_controls_accessor_js,
    const char* dev_controls_visibility_check_js,
    bool expected_visible) {
  ASSERT_TRUE(content::ExecJs(
      contents,
      base::StringPrintf(
          "var screenElement = %s;"
          "new Promise(resolve => {"
          "  function SendReplyIfAsExpected() {"
          "    var is_visible = %s;"
          "    if (is_visible != %s)"
          "      return false;"
          "    observer.disconnect();"
          "    resolve(true);"
          "    return true;"
          "  }"
          "  var observer = new MutationObserver(SendReplyIfAsExpected);"
          "  if (!SendReplyIfAsExpected()) {"
          "    var options = { 'attributes': true };"
          "    observer.observe(screenElement, options);"
          "  }"
          "});",
          dev_controls_accessor_js, dev_controls_visibility_check_js,
          (expected_visible ? "true" : "false"))));
}

// Utility to get a PolicyMap for setting the DeveloperToolsAvailability policy
// to a given value.
PolicyMap MakeDeveloperToolsAvailabilityMap(int value) {
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(value),
               nullptr);
  return policies;
}

// Navigates the current tab of the browser to the given URL without any
// waiting after the navigation is triggered. Note: the
// ui_test_utils::BROWSER_TEST_NO_WAIT flag passed in results this returning
// right after the Browser::OpenURL() call without waiting for any load
// events.
void NavigateToURLNoWait(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
}

// Utility to navigate the current tab of the browser to the specified page and
// then kill it using chrome://kill, verifying that the page ends up crashed.
void VerifyPageAllowsKill(Browser* browser, const GURL& url) {
  SCOPED_TRACE(base::StringPrintf("Verifying url allows kill: '%s'",
                                  url.spec().c_str()));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    content::RenderProcessHostWatcher exit_observer(
        browser->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame()
            ->GetProcess(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser, GURL(blink::kChromeUIKillURL)));
    exit_observer.Wait();
    // The kill url will have left a hanging pending entry on the
    // NavigationController and the contents will be marked as having crashed.
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(web_contents->GetController().GetPendingEntry());
    EXPECT_TRUE(web_contents->IsCrashed());
    EXPECT_FALSE(exit_observer.did_exit_normally());
  }
}

// Utility to navigate the current tab of the browser to the specified page and
// then attempt to kill it using chrome://kill, verifying that the kill is
// blocked before any navigation is started.
void VerifyPageBlocksKill(Browser* browser, const GURL& url) {
  SCOPED_TRACE(base::StringPrintf("Verifying url blocks kill: '%s'",
                                  url.spec().c_str()));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  // We expect the debug URL to be blocked synchronously, so we don't have to
  // wait for a load stop here. Afterwards we verify nothing has happened by
  // checking there is no pending entry on the NavigationController (indicating
  // no navigation was started) and that the contents has not crashed.
  NavigateToURLNoWait(browser, GURL(blink::kChromeUIKillURL));
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(web_contents->GetController().GetPendingEntry());
  EXPECT_FALSE(web_contents->IsCrashed());
}

// Utility to navigate the current tab of the browser to the specified page and
// return true if a javascript URL can be run on it, false otherwise.
bool PageAllowsJavascriptURL(Browser* browser, const GURL& url) {
  SCOPED_TRACE(base::StringPrintf("Checking url allows javascript URLs: '%s'",
                                  url.spec().c_str()));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  const std::u16string original_title = web_contents->GetTitle();

  const GURL javascript_url("javascript:void(document.title='Modified Title')");
  NavigateToURLNoWait(browser, javascript_url);
  // Run another script we can wait on to ensure the javascript URL will have
  // processed if it was going to.
  EXPECT_EQ(true, content::EvalJs(web_contents, "true"));

  // Check if the title was changed and return true if so.
  if (web_contents->GetTitle() != original_title) {
    EXPECT_EQ(u"Modified Title", web_contents->GetTitle());
    return true;
  }
  EXPECT_NE(u"Modified Title", web_contents->GetTitle());
  return false;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyTest, DeveloperToolsDisabledByLegacyPolicy) {
  // Verifies that access to the developer tools can be disabled by setting the
  // legacy DeveloperToolsDisabled policy.

  // Open devtools.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  EXPECT_TRUE(devtools_window);

  // Disable devtools via policy.
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  content::WebContentsDestroyedWatcher close_observer(
      DevToolsWindowTesting::Get(devtools_window)->main_web_contents());
  UpdateProviderPolicy(policies);
  // wait for devtools close
  close_observer.Wait();
  // The existing devtools window should have closed.
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
  // And it's not possible to open it again.
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
}

IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DeveloperToolsDisabledByDeveloperToolsAvailability) {
  // Verifies that access to the developer tools can be disabled by setting the
  // DeveloperToolsAvailability policy.

  // Open devtools.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  EXPECT_TRUE(devtools_window);

  content::WebContentsDestroyedWatcher close_observer(
      DevToolsWindowTesting::Get(devtools_window)->main_web_contents());
  // Disable devtools via policy.
  UpdateProviderPolicy(
      MakeDeveloperToolsAvailabilityMap(2 /* DeveloperToolsDisallowed */));
  // wait for devtools close
  close_observer.Wait();
  // The existing devtools window should have closed.
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
  // And it's not possible to open it again.
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
}

// Test for https://b/263040629
IN_PROC_BROWSER_TEST_F(PolicyTest, AvailabilityWins) {
  // DeveloperToolsDisabled is true, but DeveloperToolsAvailability wins.
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(1 /* DeveloperToolsAllowed */), nullptr);
  policies.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  EXPECT_TRUE(devtools_window);

  // Clearing DeveloperToolsAvailability leaves behind
  // DeveloperToolsDisabled, so the DevTools window gets closed.
  content::WebContentsDestroyedWatcher close_observer(
      DevToolsWindowTesting::Get(devtools_window)->main_web_contents());
  policies.Erase(key::kDeveloperToolsAvailability);
  UpdateProviderPolicy(policies);
  // wait for devtools close
  close_observer.Wait();
  // The existing devtools window should have closed.
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
  // And it's not possible to open it again.
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
}

IN_PROC_BROWSER_TEST_F(PolicyTest,
                       ViewSourceDisabledByDeveloperToolsAvailability) {
  // Verifies that entry points to ViewSource can be disabled by setting the
  // DeveloperToolsAvailability policy.

  // Disable devtools via policy.
  UpdateProviderPolicy(
      MakeDeveloperToolsAvailabilityMap(2 /* DeveloperToolsDisallowed */));
  // Verify that it's not possible to ViewSource.
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_VIEW_SOURCE));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DeveloperToolsDisabledExtensionsDevMode) {
  // Verifies that when DeveloperToolsDisabled policy is set, the "dev mode"
  // in chrome://extensions is actively turned off and the checkbox
  // is disabled.
  // Note: We don't test the indicator as it is tested in the policy pref test
  // for kDeveloperToolsDisabled and kDeveloperToolsAvailability.

  // This test depends on the following helper methods to locate the DOM
  // elements to be tested.
  const char define_helpers_js[] =
      R"(function getToolbar() {
           const manager = document.querySelector('extensions-manager');
           return manager.shadowRoot.querySelector('extensions-toolbar');
         }

         function getToggle() {
           return getToolbar().$.devMode;
         }

         function getControls() {
           return getToolbar().$.devDrawer;
         }
        )";

  const char toggle_dev_mode_accessor_js[] = "getToggle()";
  const char dev_controls_accessor_js[] = "getControls()";
  const char dev_controls_visibility_check_js[] =
      "getControls().hasAttribute('expanded')";

  // Navigate to the extensions frame and enabled "Developer mode"
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIExtensionsURL)));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecJs(contents, std::string(define_helpers_js)));

  EXPECT_TRUE(content::ExecJs(
      contents, base::StringPrintf("domAutomationController.send(%s.click());",
                                   toggle_dev_mode_accessor_js)));

  WaitForExtensionsDevModeControlsVisibility(contents, dev_controls_accessor_js,
                                             dev_controls_visibility_check_js,
                                             true);

  // Disable devtools via policy.
  UpdateProviderPolicy(
      MakeDeveloperToolsAvailabilityMap(2 /* DeveloperToolsDisallowed */));

  // Expect devcontrols to be hidden now...
  WaitForExtensionsDevModeControlsVisibility(contents, dev_controls_accessor_js,
                                             dev_controls_visibility_check_js,
                                             false);

  // ... and checkbox is disabled
  EXPECT_EQ(true, content::EvalJs(contents, base::StringPrintf(
                                                "%s.hasAttribute('disabled')",
                                                toggle_dev_mode_accessor_js)));
}

// Verifies debug URLs, specifically chrome://kill and javascript URLs, are
// blocked or allowed for different pages depending on the
// DeveloperToolsAvailability policy setting. Note: javascript URLs are always
// blocked on extension schemes, regardless of the policy setting.
// TODO(crbug.com/40064953): The loading of a force installed extension in this
// test runs into an issue on branded Windows builders.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_WIN)
#define MAYBE_DebugURLsDisabledByDeveloperToolsAvailability \
  DISABLED_DebugURLsDisabledByDeveloperToolsAvailability
#else
#define MAYBE_DebugURLsDisabledByDeveloperToolsAvailability \
  DebugURLsDisabledByDeveloperToolsAvailability
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest,
                       MAYBE_DebugURLsDisabledByDeveloperToolsAvailability) {
  // Get a url for a standard web page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL tab_url(embedded_test_server()->GetURL("/empty.html"));

  // Get a url for a force installed extension.
  base::FilePath crx_path(ui_test_utils::GetTestFilePath(
      base::FilePath().AppendASCII("devtools").AppendASCII("extensions"),
      base::FilePath().AppendASCII("options.crx")));
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  // TODO(crbug.com/40269105): We shouldn't need to ignore manifest warnings
  // here, but there's an issue related to the _metadata folder added for
  // content verification when force-installing an off-store crx in a branded
  // build, which produces an install warning.
  loader.set_ignore_manifest_warnings(true);
  loader.set_location(ManifestLocation::kExternalPolicyDownload);
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(crx_path);
  ASSERT_TRUE(extension);
  GURL extension_url("chrome-extension://" + extension->id() + "/options.html");

  // The default for DeveloperToolsAvailability is to disallow for force
  // installed extensions. Even though that is already the value, we set it here
  // to be explicit about what the value currently is.
  // With this setting force installed extension should block the debug and
  // javascript URLs and but normal pages should allow them.
  UpdateProviderPolicy(MakeDeveloperToolsAvailabilityMap(
      0 /* DeveloperToolsDisallowedForForceInstalledExtensions */));
  {
    SCOPED_TRACE(
        "Testing DeveloperToolsDisallowedForForceInstalledExtensions policy "
        "setting");
    VerifyPageAllowsKill(browser(), tab_url);
    EXPECT_TRUE(PageAllowsJavascriptURL(browser(), tab_url));
    VerifyPageBlocksKill(browser(), extension_url);
    EXPECT_FALSE(PageAllowsJavascriptURL(browser(), extension_url));
  }

  // When the policy is set to always allow Devtools all the pages should allow
  // debug URLs to be used, but javascript URLs will still be blocked on any
  // extension schemes.
  UpdateProviderPolicy(
      MakeDeveloperToolsAvailabilityMap(1 /* DeveloperToolsAllowed */));
  {
    SCOPED_TRACE("Testing DeveloperToolsAllowed policy setting");
    VerifyPageAllowsKill(browser(), tab_url);
    EXPECT_TRUE(PageAllowsJavascriptURL(browser(), tab_url));
    VerifyPageAllowsKill(browser(), extension_url);
    EXPECT_FALSE(PageAllowsJavascriptURL(browser(), extension_url));
  }

  // When the policy is set to always disallow Devtools all the pages should
  // block debug and javascript URLs.
  UpdateProviderPolicy(
      MakeDeveloperToolsAvailabilityMap(2 /* DeveloperToolsDisallowed */));
  {
    SCOPED_TRACE("Testing DeveloperToolsDisallowed policy setting");
    VerifyPageBlocksKill(browser(), tab_url);
    EXPECT_FALSE(PageAllowsJavascriptURL(browser(), tab_url));
    VerifyPageBlocksKill(browser(), extension_url);
    EXPECT_FALSE(PageAllowsJavascriptURL(browser(), extension_url));
  }
}

}  // namespace policy
