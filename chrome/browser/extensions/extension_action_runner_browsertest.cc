// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "extensions/browser/extension_action.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using SiteAccess = SitePermissionsHelper::SiteAccess;

const char kAllHostsScheme[] = "*://*/*";
const char kExplicitHostsScheme[] = "http://127.0.0.1/*";
const char kBackgroundScript[] =
    "\"background\": {\"scripts\": [\"script.js\"]}";
const char kBackgroundScriptSource[] =
    "var listener = function(tabId) {\n"
    "  chrome.tabs.onUpdated.removeListener(listener);\n"
    "  chrome.tabs.executeScript(tabId, {\n"
    "    code: \"chrome.test.sendMessage('inject succeeded');\"\n"
    "  });"
    "};\n"
    "chrome.tabs.onUpdated.addListener(listener);\n"
    "chrome.test.sendMessage('ready');";
const char kContentScriptSource[] =
    "chrome.test.sendMessage('inject succeeded');";

const char kInjectSucceeded[] = "inject succeeded";

enum InjectionType { CONTENT_SCRIPT, EXECUTE_SCRIPT };

enum HostType { ALL_HOSTS, EXPLICIT_HOSTS };

enum RequiresConsent { REQUIRES_CONSENT, DOES_NOT_REQUIRE_CONSENT };

enum WithholdPermissions { WITHHOLD_PERMISSIONS, DONT_WITHHOLD_PERMISSIONS };

// Runs all pending tasks in the renderer associated with |web_contents|.
// Returns true on success.
bool RunAllPendingInRenderer(content::WebContents* web_contents) {
  // This is slight hack to achieve a RunPendingInRenderer() method. Since IPCs
  // are sent synchronously, anything started prior to this method will finish
  // before this method returns (as content::ExecuteScript() is synchronous).
  return content::ExecuteScript(web_contents, "1 == 1;");
}

// For use with blocked actions browsertests that put the result in
// window.localStorage. Returns whether the extension injected a script.
bool DidInjectScript(content::WebContents* web_contents) {
  std::string out;
  bool exec = content::ExecuteScriptAndExtractString(
      web_contents,
      "var res = window.localStorage.getItem('extResult') || 'undefined';"
      "window.localStorage.removeItem('extResult');"
      "window.domAutomationController.send(res);",
      &out);
  EXPECT_TRUE(exec);
  EXPECT_TRUE(out == "success" || out == "undefined")
      << "Unexpected script value: " << out;
  return out == "success";
}

}  // namespace

class ExtensionActionRunnerBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionActionRunnerBrowserTest() {}

  void TearDownOnMainThread() override;

  // Returns an extension with the given |host_type| and |injection_type|. If
  // one already exists, the existing extension will be returned. Othewrwise,
  // one will be created.
  // This could potentially return NULL if LoadExtension() fails.
  const Extension* CreateExtension(HostType host_type,
                                   InjectionType injection_type,
                                   WithholdPermissions withhold_permissions);

  void RunActiveScriptsTest(const std::string& name,
                            HostType host_type,
                            InjectionType injection_type,
                            WithholdPermissions withhold_permissions,
                            RequiresConsent requires_consent);

 private:
  std::vector<std::unique_ptr<TestExtensionDir>> test_extension_dirs_;
  std::vector<const Extension*> extensions_;
};

void ExtensionActionRunnerBrowserTest::TearDownOnMainThread() {
  test_extension_dirs_.clear();
}

const Extension* ExtensionActionRunnerBrowserTest::CreateExtension(
    HostType host_type,
    InjectionType injection_type,
    WithholdPermissions withhold_permissions) {
  std::string name = base::StringPrintf(
      "%s %s",
      injection_type == CONTENT_SCRIPT ? "content_script" : "execute_script",
      host_type == ALL_HOSTS ? "all_hosts" : "explicit_hosts");

  const char* const permission_scheme =
      host_type == ALL_HOSTS ? kAllHostsScheme : kExplicitHostsScheme;

  std::string permissions = base::StringPrintf(
      "\"permissions\": [\"tabs\", \"%s\"]", permission_scheme);

  std::string scripts;
  std::string script_source;
  if (injection_type == CONTENT_SCRIPT) {
    scripts = base::StringPrintf(
        "\"content_scripts\": ["
        " {"
        "  \"matches\": [\"%s\"],"
        "  \"js\": [\"script.js\"],"
        "  \"run_at\": \"document_end\""
        " }"
        "]",
        permission_scheme);
  } else {
    scripts = kBackgroundScript;
  }

  std::string manifest = base::StringPrintf(
      "{"
      " \"name\": \"%s\","
      " \"version\": \"1.0\","
      " \"manifest_version\": 2,"
      " %s,"
      " %s"
      "}",
      name.c_str(), permissions.c_str(), scripts.c_str());

  std::unique_ptr<TestExtensionDir> dir(new TestExtensionDir);
  dir->WriteManifest(manifest);
  dir->WriteFile(FILE_PATH_LITERAL("script.js"),
                 injection_type == CONTENT_SCRIPT ? kContentScriptSource
                                                  : kBackgroundScriptSource);

  const Extension* extension = nullptr;
  if (injection_type == CONTENT_SCRIPT) {
    extension = LoadExtension(dir->UnpackedPath());
  } else {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  if (extension) {
    test_extension_dirs_.push_back(std::move(dir));
    extensions_.push_back(extension);

    ScriptingPermissionsModifier modifier(profile(), extension);
    if (withhold_permissions == WITHHOLD_PERMISSIONS &&
        modifier.CanAffectExtension()) {
      modifier.SetWithholdHostPermissions(true);
    }
  }

  // If extension is NULL here, it will be caught later in the test.
  return extension;
}

void ExtensionActionRunnerBrowserTest::RunActiveScriptsTest(
    const std::string& name,
    HostType host_type,
    InjectionType injection_type,
    WithholdPermissions withhold_permissions,
    RequiresConsent requires_consent) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      CreateExtension(host_type, injection_type, withhold_permissions);
  ASSERT_TRUE(extension);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  ExtensionTestMessageListener inject_success_listener(kInjectSucceeded);
  auto navigate = [this]() {
    // Navigate to an URL (which matches the explicit host specified in the
    // extension content_scripts_explicit_hosts). All extensions should
    // inject the script.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html")));
  };

  if (requires_consent == DOES_NOT_REQUIRE_CONSENT) {
    // If the extension doesn't require explicit consent, it should inject
    // automatically right away.
    navigate();
    EXPECT_FALSE(runner->WantsToRun(extension));
    EXPECT_TRUE(inject_success_listener.WaitUntilSatisfied());
    EXPECT_FALSE(runner->WantsToRun(extension));
    return;
  }

  ASSERT_EQ(REQUIRES_CONSENT, requires_consent);

  class BlockedActionWaiter : public ExtensionActionRunner::TestObserver {
   public:
    explicit BlockedActionWaiter(ExtensionActionRunner* runner)
        : runner_(runner) {
      runner_->set_observer_for_testing(this);
    }

    BlockedActionWaiter(const BlockedActionWaiter&) = delete;
    BlockedActionWaiter& operator=(const BlockedActionWaiter&) = delete;

    ~BlockedActionWaiter() { runner_->set_observer_for_testing(nullptr); }

    void Wait() { run_loop_.Run(); }

   private:
    // ExtensionActionRunner::TestObserver:
    void OnBlockedActionAdded() override { run_loop_.Quit(); }

    raw_ptr<ExtensionActionRunner> runner_;
    base::RunLoop run_loop_;
  };

  BlockedActionWaiter waiter(runner);
  navigate();
  waiter.Wait();
  EXPECT_TRUE(runner->WantsToRun(extension));
  EXPECT_FALSE(inject_success_listener.was_satisfied());

  // Grant permission by clicking on the extension action.
  runner->RunAction(extension, true);

  // Now, the extension should be able to inject the script.
  EXPECT_TRUE(inject_success_listener.WaitUntilSatisfied());

  // The extension should no longer want to run.
  EXPECT_FALSE(runner->WantsToRun(extension));
}

// Load up different combinations of extensions, and verify that script
// injection is properly withheld and indicated to the user.
// NOTE: Though these could be parameterized test cases, there's enough
// bits here that just having a helper method is quite a bit more readable.
IN_PROC_BROWSER_TEST_F(
    ExtensionActionRunnerBrowserTest,
    ActiveScriptsAreDisplayedAndDelayExecution_ExecuteScripts_AllHosts) {
  RunActiveScriptsTest("execute_scripts_all_hosts", ALL_HOSTS, EXECUTE_SCRIPT,
                       WITHHOLD_PERMISSIONS, REQUIRES_CONSENT);
}
IN_PROC_BROWSER_TEST_F(
    ExtensionActionRunnerBrowserTest,
    ActiveScriptsAreDisplayedAndDelayExecution_ExecuteScripts_ExplicitHosts) {
  RunActiveScriptsTest("execute_scripts_explicit_hosts", EXPLICIT_HOSTS,
                       EXECUTE_SCRIPT, WITHHOLD_PERMISSIONS, REQUIRES_CONSENT);
}
IN_PROC_BROWSER_TEST_F(
    ExtensionActionRunnerBrowserTest,
    ActiveScriptsAreDisplayedAndDelayExecution_ContentScripts_AllHosts) {
  RunActiveScriptsTest("content_scripts_all_hosts", ALL_HOSTS, CONTENT_SCRIPT,
                       WITHHOLD_PERMISSIONS, REQUIRES_CONSENT);
}
IN_PROC_BROWSER_TEST_F(
    ExtensionActionRunnerBrowserTest,
    ActiveScriptsAreDisplayedAndDelayExecution_ContentScripts_ExplicitHosts) {
  RunActiveScriptsTest("content_scripts_explicit_hosts", EXPLICIT_HOSTS,
                       CONTENT_SCRIPT, WITHHOLD_PERMISSIONS, REQUIRES_CONSENT);
}

// Test that removing an extension with pending injections a) removes the
// pending injections for that extension, and b) does not affect pending
// injections for other extensions.
IN_PROC_BROWSER_TEST_F(ExtensionActionRunnerBrowserTest,
                       RemoveExtensionWithPendingInjections) {
  // Load up two extensions, each with content scripts.
  scoped_refptr<const Extension> extension1 =
      CreateExtension(ALL_HOSTS, CONTENT_SCRIPT, WITHHOLD_PERMISSIONS);
  ASSERT_TRUE(extension1);
  scoped_refptr<const Extension> extension2 =
      CreateExtension(ALL_HOSTS, CONTENT_SCRIPT, WITHHOLD_PERMISSIONS);
  ASSERT_TRUE(extension2);

  ASSERT_NE(extension1->id(), extension2->id());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(action_runner);

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));

  // Both extensions should have pending requests.
  EXPECT_TRUE(action_runner->WantsToRun(extension1.get()));
  EXPECT_TRUE(action_runner->WantsToRun(extension2.get()));

  // Unload one of the extensions.
  UnloadExtension(extension2->id());

  EXPECT_TRUE(RunAllPendingInRenderer(web_contents));

  // We should have pending requests for extension1, but not the removed
  // extension2.
  EXPECT_TRUE(action_runner->WantsToRun(extension1.get()));
  EXPECT_FALSE(action_runner->WantsToRun(extension2.get()));

  // We should still be able to run the request for extension1.
  ExtensionTestMessageListener inject_success_listener(kInjectSucceeded);
  inject_success_listener.set_extension_id(extension1->id());
  action_runner->RunAction(extension1.get(), true);
  EXPECT_TRUE(inject_success_listener.WaitUntilSatisfied());
}

// Test that granting the extension all urls permission allows it to run on
// pages, and that the permission update is sent to existing renderers.
IN_PROC_BROWSER_TEST_F(ExtensionActionRunnerBrowserTest,
                       GrantExtensionAllUrlsPermission) {
  // Loadup an extension and navigate.
  const Extension* extension =
      CreateExtension(ALL_HOSTS, CONTENT_SCRIPT, WITHHOLD_PERMISSIONS);
  ASSERT_TRUE(extension);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(action_runner);

  ExtensionTestMessageListener inject_success_listener(kInjectSucceeded);
  inject_success_listener.set_extension_id(extension->id());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // The extension shouldn't be allowed to run.
  EXPECT_TRUE(action_runner->WantsToRun(extension));
  EXPECT_EQ(1, action_runner->num_page_requests());
  EXPECT_FALSE(inject_success_listener.was_satisfied());

  // Enable the extension to run on all urls.
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(false);
  EXPECT_TRUE(RunAllPendingInRenderer(web_contents));

  // Navigate again - this time, the extension should execute immediately (and
  // should not need to ask the script controller for permission).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(action_runner->WantsToRun(extension));
  EXPECT_EQ(0, action_runner->num_page_requests());
  EXPECT_TRUE(inject_success_listener.WaitUntilSatisfied());

  // Revoke all urls permissions.
  inject_success_listener.Reset();
  modifier.SetWithholdHostPermissions(true);
  EXPECT_TRUE(RunAllPendingInRenderer(web_contents));

  // Re-navigate; the extension should again need permission to run.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(action_runner->WantsToRun(extension));
  EXPECT_EQ(1, action_runner->num_page_requests());
  EXPECT_FALSE(inject_success_listener.was_satisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionActionRunnerBrowserTest,
                       BlockedActionBrowserTest) {
  // Load an extension that wants to run on every page at document start, and
  // load a test page.
  ASSERT_TRUE(embedded_test_server()->Start());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("blocked_actions/content_scripts"));
  ASSERT_TRUE(extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  // Navigate to a page where the extension wants to run.
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  content::NavigationController& web_controller = web_contents->GetController();
  const int nav_id = web_controller.GetLastCommittedEntry()->GetUniqueID();

  // The extension should want to run on the page, should not have
  // injected, and should have "on click" access.
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);
  EXPECT_TRUE(runner->WantsToRun(extension));
  EXPECT_FALSE(DidInjectScript(web_contents));
  SitePermissionsHelper permissions(profile());
  EXPECT_EQ(permissions.GetSiteAccess(*extension, url), SiteAccess::kOnClick);

  // Wire up the runner to reject the blocked action bubble prompting for
  // page refresh, and run the action.
  runner->accept_bubble_for_testing(false);
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Nothing should happen, because the user didn't agree to refresh the page.
  // The extension should still want to run.
  EXPECT_EQ(web_controller.GetLastCommittedEntry()->GetUniqueID(), nav_id);
  EXPECT_FALSE(DidInjectScript(web_contents));
  EXPECT_TRUE(runner->WantsToRun(extension));

  // Wire up the runner to automatically accept the blocked action bubble with
  // no checkbox marked, and run the action.
  runner->accept_bubble_for_testing(true);
  runner->bubble_is_checked_for_testing(false);
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Since we automatically accepted the bubble prompting us, the page should
  // have refreshed, the extension should have injected at document start and
  // the site access should still be "on click".
  EXPECT_GE(web_controller.GetLastCommittedEntry()->GetUniqueID(), nav_id);
  EXPECT_TRUE(DidInjectScript(web_contents));
  EXPECT_FALSE(runner->WantsToRun(extension));
  EXPECT_EQ(permissions.GetSiteAccess(*extension, url), SiteAccess::kOnClick);

  // Revoke permission and reload to try different bubble options.
  ActiveTabPermissionGranter* active_tab_granter =
      TabHelper::FromWebContents(web_contents)->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_granter);
  active_tab_granter->RevokeForTesting();
  web_controller.Reload(content::ReloadType::NORMAL, true);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_TRUE(runner->WantsToRun(extension));

  // Wire up the runner to automatically accept the blocked action bubble with
  // the checkbox marked, and run the action.
  runner->accept_bubble_for_testing(true);
  runner->bubble_is_checked_for_testing(true);
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Since we automatically accepted the bubble (with checkbox selected)
  // prompting us, the page should have refreshed, the extension should have
  // injected at document start and the site access should now be "on site".
  EXPECT_GE(web_controller.GetLastCommittedEntry()->GetUniqueID(), nav_id);
  EXPECT_TRUE(DidInjectScript(web_contents));
  EXPECT_FALSE(runner->WantsToRun(extension));
  EXPECT_EQ(permissions.GetSiteAccess(*extension, url), SiteAccess::kOnSite);
}

// If we don't withhold permissions, extensions should execute normally.
IN_PROC_BROWSER_TEST_F(ExtensionActionRunnerBrowserTest,
                       ScriptsExecuteWhenNoPermissionsWithheld_ContentScripts) {
  RunActiveScriptsTest("content_scripts_all_hosts", ALL_HOSTS, CONTENT_SCRIPT,
                       DONT_WITHHOLD_PERMISSIONS, DOES_NOT_REQUIRE_CONSENT);
}
IN_PROC_BROWSER_TEST_F(ExtensionActionRunnerBrowserTest,
                       ScriptsExecuteWhenNoPermissionsWithheld_ExecuteScripts) {
  RunActiveScriptsTest("execute_scripts_all_hosts", ALL_HOSTS, EXECUTE_SCRIPT,
                       DONT_WITHHOLD_PERMISSIONS, DOES_NOT_REQUIRE_CONSENT);
}

// A version of the test with the flag off, in order to test that everything
// still works as expected.
class FlagOffExtensionActionRunnerBrowserTest
    : public ExtensionActionRunnerBrowserTest {
 private:
  // Simply don't append the flag.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(FlagOffExtensionActionRunnerBrowserTest,
                       ScriptsExecuteWhenFlagAbsent_ContentScripts) {
  RunActiveScriptsTest("content_scripts_all_hosts", ALL_HOSTS, CONTENT_SCRIPT,
                       DONT_WITHHOLD_PERMISSIONS, DOES_NOT_REQUIRE_CONSENT);
}
IN_PROC_BROWSER_TEST_F(FlagOffExtensionActionRunnerBrowserTest,
                       ScriptsExecuteWhenFlagAbsent_ExecuteScripts) {
  RunActiveScriptsTest("execute_scripts_all_hosts", ALL_HOSTS, EXECUTE_SCRIPT,
                       DONT_WITHHOLD_PERMISSIONS, DOES_NOT_REQUIRE_CONSENT);
}

class ExtensionActionRunnerFencedFrameBrowserTest
    : public ExtensionActionRunnerBrowserTest {
 public:
  ExtensionActionRunnerFencedFrameBrowserTest() = default;
  ~ExtensionActionRunnerFencedFrameBrowserTest() override = default;

  ExtensionActionRunnerFencedFrameBrowserTest(
      const ExtensionActionRunnerFencedFrameBrowserTest&) = delete;
  ExtensionActionRunnerFencedFrameBrowserTest& operator=(
      const ExtensionActionRunnerFencedFrameBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ExtensionActionRunnerBrowserTest::SetUpOnMainThread();
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Tests that a fenced frame doesn't clear active extensions.
IN_PROC_BROWSER_TEST_F(ExtensionActionRunnerFencedFrameBrowserTest,
                       FencedFrameDoesNotClearActiveExtensions) {
  // Set a situation that |granted_extensions_| of ActiveTabPermissionGranter is
  // not empty to test a fenced frame doesn't clear active extensions.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("blocked_actions/content_scripts"));
  ASSERT_TRUE(extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  GURL initial_url = embedded_test_server()->GetURL("a.com", "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  runner->accept_bubble_for_testing(true);

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  const int first_nav_id = entry->GetUniqueID();

  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  entry = web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  EXPECT_GE(entry->GetUniqueID(), first_nav_id);
  EXPECT_TRUE(DidInjectScript(web_contents));
  EXPECT_FALSE(runner->WantsToRun(extension));

  ActiveTabPermissionGranter* active_tab_granter =
      TabHelper::FromWebContents(web_contents)->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_granter);
  EXPECT_EQ(active_tab_granter->granted_extensions_.size(), 1U);

  // The origin of |url| and |fenced_frame_url| should be different because
  // ActiveTabPermissionGranter::DidFinishNavigation is only able to clear
  // active extensions when the origins are different.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title1.html");
  // Create a fenced frame and load the test url. Active extensions should not
  // be cleared by the fenced frame navigation.
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_helper_.CreateFencedFrame(
          web_contents->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);
  EXPECT_EQ(active_tab_granter->granted_extensions_.size(), 1U);

  // Active extensions should be cleared after navigating a test url on the
  // primary main frame.
  GURL test_url = embedded_test_server()->GetURL("c.com", "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  EXPECT_EQ(active_tab_granter->granted_extensions_.size(), 0U);
}

IN_PROC_BROWSER_TEST_F(ExtensionActionRunnerFencedFrameBrowserTest,
                       DoNotResetExtensionActionRunner) {
  // Loadup an extension and navigate to test that a fenced frame doesn't reset
  // ExtensionActionRunner's member variables.
  const Extension* extension =
      CreateExtension(ALL_HOSTS, CONTENT_SCRIPT, WITHHOLD_PERMISSIONS);
  ASSERT_TRUE(extension);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(action_runner);

  ExtensionTestMessageListener inject_success_listener(kInjectSucceeded);
  inject_success_listener.set_extension_id(extension->id());

  GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(false);
  EXPECT_TRUE(RunAllPendingInRenderer(web_contents));

  // Create a fenced frame and navigate the fenced frame url.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_helper_.CreateFencedFrame(
          web_contents->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);
  // Fenced frame doesn't clear pending script injection requests and the
  // scripts.
  EXPECT_EQ(1, action_runner->num_page_requests());
  EXPECT_EQ(1U, action_runner->pending_scripts_.size());

  // Navigate again on the primary main frame. Pending script injection requests
  // and scripts should be cleared.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(0, action_runner->num_page_requests());
  EXPECT_EQ(0U, action_runner->pending_scripts_.size());
}

}  // namespace extensions
