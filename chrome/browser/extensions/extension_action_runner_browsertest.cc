// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

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
// window.localStorage. Returns the result or "undefined" if the result is not
// set.
std::string GetValue(content::WebContents* web_contents) {
  std::string out;
  if (!content::ExecuteScriptAndExtractString(
          web_contents,
          "var res = window.localStorage.getItem('extResult') || 'undefined';"
          "window.localStorage.removeItem('extResult');"
          "window.domAutomationController.send(res);",
          &out)) {
    out = "Failed to inject script";
  }
  return out;
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
    ExtensionTestMessageListener listener("ready", false);
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

  const bool will_reply = false;
  ExtensionTestMessageListener inject_success_listener(kInjectSucceeded,
                                                       will_reply);
  auto navigate = [this]() {
    // Navigate to an URL (which matches the explicit host specified in the
    // extension content_scripts_explicit_hosts). All extensions should
    // inject the script.
    ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                                "/extensions/test_file.html"));
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
    ~BlockedActionWaiter() { runner_->set_observer_for_testing(nullptr); }

    void Wait() { run_loop_.Run(); }

   private:
    // ExtensionActionRunner::TestObserver:
    void OnBlockedActionAdded() override { run_loop_.Quit(); }

    ExtensionActionRunner* runner_;
    base::RunLoop run_loop_;

    DISALLOW_COPY_AND_ASSIGN(BlockedActionWaiter);
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
  const Extension* extension1 =
      CreateExtension(ALL_HOSTS, CONTENT_SCRIPT, WITHHOLD_PERMISSIONS);
  ASSERT_TRUE(extension1);
  const Extension* extension2 =
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
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  // Both extensions should have pending requests.
  EXPECT_TRUE(action_runner->WantsToRun(extension1));
  EXPECT_TRUE(action_runner->WantsToRun(extension2));

  // Unload one of the extensions.
  UnloadExtension(extension2->id());

  EXPECT_TRUE(RunAllPendingInRenderer(web_contents));

  // We should have pending requests for extension1, but not the removed
  // extension2.
  EXPECT_TRUE(action_runner->WantsToRun(extension1));
  EXPECT_FALSE(action_runner->WantsToRun(extension2));

  // We should still be able to run the request for extension1.
  ExtensionTestMessageListener inject_success_listener(
      new ExtensionTestMessageListener(kInjectSucceeded,
                                       false /* won't reply */));
  inject_success_listener.set_extension_id(extension1->id());
  action_runner->RunAction(extension1, true);
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

  ExtensionTestMessageListener inject_success_listener(
      new ExtensionTestMessageListener(kInjectSucceeded,
                                       false /* won't reply */));
  inject_success_listener.set_extension_id(extension->id());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  ui_test_utils::NavigateToURL(browser(), url);

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
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(action_runner->WantsToRun(extension));
  EXPECT_EQ(0, action_runner->num_page_requests());
  EXPECT_TRUE(inject_success_listener.WaitUntilSatisfied());

  // Revoke all urls permissions.
  inject_success_listener.Reset();
  modifier.SetWithholdHostPermissions(true);
  EXPECT_TRUE(RunAllPendingInRenderer(web_contents));

  // Re-navigate; the extension should again need permission to run.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(action_runner->WantsToRun(extension));
  EXPECT_EQ(1, action_runner->num_page_requests());
  EXPECT_FALSE(inject_success_listener.was_satisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionActionRunnerBrowserTest,
                       BlockedActionBrowserTest) {
  // Load an extension that wants to run on every page at document start, and
  // load a test page.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("blocked_actions/content_scripts"));
  ASSERT_TRUE(extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // The extension should want to run on the page, and should not have
  // injected.
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);
  EXPECT_TRUE(runner->WantsToRun(extension));
  EXPECT_EQ("undefined", GetValue(web_contents));

  // Wire up the runner to automatically accept the bubble to prompt for page
  // refresh.
  runner->set_default_bubble_close_action_for_testing(
      std::make_unique<ToolbarActionsBarBubbleDelegate::CloseAction>(
          ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE));

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  const int first_nav_id = entry->GetUniqueID();

  // Run the extension action, which should cause a page refresh (since we
  // automatically accepted the bubble prompting us), and the extension should
  // have injected at document start.
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  entry = web_contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  // Confirm that we refreshed the page.
  EXPECT_GE(entry->GetUniqueID(), first_nav_id);
  EXPECT_EQ("success", GetValue(web_contents));
  EXPECT_FALSE(runner->WantsToRun(extension));

  // Revoke permission and reload to try different bubble options.
  ActiveTabPermissionGranter* active_tab_granter =
      TabHelper::FromWebContents(web_contents)->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_granter);
  active_tab_granter->RevokeForTesting();
  web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // The extension should again want to run. Automatically dismiss the bubble
  // that pops up prompting for page refresh.
  EXPECT_TRUE(runner->WantsToRun(extension));
  EXPECT_EQ("undefined", GetValue(web_contents));
  const int next_nav_id =
      web_contents->GetController().GetLastCommittedEntry()->GetUniqueID();
  runner->set_default_bubble_close_action_for_testing(
      std::make_unique<ToolbarActionsBarBubbleDelegate::CloseAction>(
          ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION));

  // Try running the extension. Nothing should happen, because the user
  // didn't agree to refresh the page. The extension should still want to run.
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ("undefined", GetValue(web_contents));
  EXPECT_EQ(
      next_nav_id,
      web_contents->GetController().GetLastCommittedEntry()->GetUniqueID());

  // Repeat with a dismissal from bubble deactivation - same story.
  runner->set_default_bubble_close_action_for_testing(
      std::make_unique<ToolbarActionsBarBubbleDelegate::CloseAction>(
          ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION));
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ("undefined", GetValue(web_contents));
  EXPECT_EQ(
      next_nav_id,
      web_contents->GetController().GetLastCommittedEntry()->GetUniqueID());
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

}  // namespace extensions
