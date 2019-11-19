// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_sync_service_factory.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/user_script.h"
#include "extensions/common/value_builder.h"

namespace extensions {

namespace {

const char kAllHostsPermission[] = "*://*/*";

}  // namespace

// Unittests for the ExtensionActionRunner mostly test the internal logic
// of the runner itself (when to allow/deny extension script injection).
// Testing real injection is allowed/denied as expected (i.e., that the
// ExtensionActionRunner correctly interfaces in the system) is done in the
// ExtensionActionRunnerBrowserTests.
class ExtensionActionRunnerUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  ExtensionActionRunnerUnitTest();
  ~ExtensionActionRunnerUnitTest() override;

  // Creates an extension with all hosts permission and adds it to the registry.
  const Extension* AddExtension();

  // Reloads |extension_| by removing it from the registry and recreating it.
  const Extension* ReloadExtension();

  // Returns true if the |extension| requires user consent before injecting
  // a script.
  bool RequiresUserConsent(const Extension* extension) const;

  // Request an injection for the given |extension|.
  void RequestInjection(const Extension* extension);
  void RequestInjection(const Extension* extension,
                        UserScript::RunLocation run_location);

  // Returns the number of times a given extension has had a script execute.
  size_t GetExecutionCountForExtension(const std::string& extension_id) const;

  ExtensionActionRunner* runner() const { return extension_action_runner_; }

 private:
  // Returns a closure to use as a script execution for a given extension.
  base::Closure GetExecutionCallbackForExtension(
      const std::string& extension_id);

  // Increment the number of executions for the given |extension_id|.
  void IncrementExecutionCount(const std::string& extension_id);

  void SetUp() override;

  // The associated ExtensionActionRunner.
  ExtensionActionRunner* extension_action_runner_ = nullptr;

  // The map of observed executions, keyed by extension id.
  std::map<std::string, int> extension_executions_;

  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionRunnerUnitTest);
};

ExtensionActionRunnerUnitTest::ExtensionActionRunnerUnitTest() = default;
ExtensionActionRunnerUnitTest::~ExtensionActionRunnerUnitTest() = default;

const Extension* ExtensionActionRunnerUnitTest::AddExtension() {
  const std::string kId = crx_file::id_util::GenerateId("all_hosts_extension");
  extension_ =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "all_hosts_extension")
                  .Set("description", "an extension")
                  .Set("manifest_version", 2)
                  .Set("version", "1.0.0")
                  .Set("permissions",
                       ListBuilder().Append(kAllHostsPermission).Build())
                  .Build())
          .SetLocation(Manifest::INTERNAL)
          .SetID(kId)
          .Build();

  ExtensionRegistry::Get(profile())->AddEnabled(extension_);
  PermissionsUpdater(profile()).InitializePermissions(extension_.get());

  ScriptingPermissionsModifier(profile(), extension_.get())
      .SetWithholdHostPermissions(true);
  return extension_.get();
}

const Extension* ExtensionActionRunnerUnitTest::ReloadExtension() {
  ExtensionRegistry::Get(profile())->RemoveEnabled(extension_->id());
  return AddExtension();
}

bool ExtensionActionRunnerUnitTest::RequiresUserConsent(
    const Extension* extension) const {
  PermissionsData::PageAccess access_type =
      runner()->RequiresUserConsentForScriptInjectionForTesting(
          extension, UserScript::PROGRAMMATIC_SCRIPT);
  // We should never downright refuse access in these tests.
  DCHECK_NE(PermissionsData::PageAccess::kDenied, access_type);
  return access_type == PermissionsData::PageAccess::kWithheld;
}

void ExtensionActionRunnerUnitTest::RequestInjection(
    const Extension* extension) {
  RequestInjection(extension, UserScript::DOCUMENT_IDLE);
}

void ExtensionActionRunnerUnitTest::RequestInjection(
    const Extension* extension,
    UserScript::RunLocation run_location) {
  runner()->RequestScriptInjectionForTesting(
      extension, run_location,
      GetExecutionCallbackForExtension(extension->id()));
}

size_t ExtensionActionRunnerUnitTest::GetExecutionCountForExtension(
    const std::string& extension_id) const {
  auto iter = extension_executions_.find(extension_id);
  if (iter != extension_executions_.end())
    return iter->second;
  return 0u;
}

base::Closure ExtensionActionRunnerUnitTest::GetExecutionCallbackForExtension(
    const std::string& extension_id) {
  // We use base unretained here, but if this ever gets executed outside of
  // this test's lifetime, we have a major problem anyway.
  return base::Bind(&ExtensionActionRunnerUnitTest::IncrementExecutionCount,
                    base::Unretained(this), extension_id);
}

void ExtensionActionRunnerUnitTest::IncrementExecutionCount(
    const std::string& extension_id) {
  ++extension_executions_[extension_id];
}

void ExtensionActionRunnerUnitTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Skip syncing for testing purposes.
  ExtensionSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  TabHelper::CreateForWebContents(web_contents());
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents());
  // These should never be null.
  DCHECK(tab_helper);
  extension_action_runner_ = tab_helper->extension_action_runner();
  DCHECK(extension_action_runner_);
}

// Test that extensions with all_hosts require permission to execute, and, once
// that permission is granted, do execute.
TEST_F(ExtensionActionRunnerUnitTest, RequestPermissionAndExecute) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.google.com"));

  // Ensure that there aren't any executions pending.
  ASSERT_EQ(0u, GetExecutionCountForExtension(extension->id()));
  ASSERT_FALSE(runner()->WantsToRun(extension));

  // Since the extension requests all_hosts, we should require user consent.
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Request an injection. The extension should want to run, but should not have
  // executed.
  RequestInjection(extension);
  EXPECT_TRUE(runner()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Click to accept the extension executing.
  runner()->RunForTesting(extension);

  // The extension should execute, and the extension shouldn't want to run.
  EXPECT_EQ(1u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(runner()->WantsToRun(extension));

  // Since we already executed on the given page, we shouldn't need permission
  // for a second time.
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Reloading and same-origin navigations shouldn't clear those permissions,
  // and we shouldn't require user constent again.
  content::NavigationSimulator::Reload(web_contents());
  EXPECT_FALSE(RequiresUserConsent(extension));
  NavigateAndCommit(GURL("https://www.google.com/foo"));
  EXPECT_FALSE(RequiresUserConsent(extension));
  NavigateAndCommit(GURL("https://www.google.com/bar"));
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Cross-origin navigations should clear permissions.
  NavigateAndCommit(GURL("https://otherdomain.google.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Grant access.
  RequestInjection(extension);
  runner()->RunForTesting(extension);
  EXPECT_EQ(2u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(runner()->WantsToRun(extension));

  // Navigating to another site should also clear the permissions.
  NavigateAndCommit(GURL("https://www.foo.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));
}

// Test that injections that are not executed by the time the user navigates are
// ignored and never execute.
TEST_F(ExtensionActionRunnerUnitTest, PendingInjectionsRemovedAtNavigation) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.google.com"));

  ASSERT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Request an injection. The extension should want to run, but not execute.
  RequestInjection(extension);
  EXPECT_TRUE(runner()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Reload. This should remove the pending injection, and we should not
  // execute anything.
  content::NavigationSimulator::Reload(web_contents());
  EXPECT_FALSE(runner()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Request and accept a new injection.
  RequestInjection(extension);
  runner()->RunForTesting(extension);

  // The extension should only have executed once, even though a grand total
  // of two executions were requested.
  EXPECT_EQ(1u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(runner()->WantsToRun(extension));
}

// Test that queueing multiple pending injections, and then accepting, triggers
// them all.
TEST_F(ExtensionActionRunnerUnitTest, MultiplePendingInjection) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);
  NavigateAndCommit(GURL("https://www.google.com"));

  ASSERT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  const size_t kNumInjections = 3u;
  // Queue multiple pending injections.
  for (size_t i = 0u; i < kNumInjections; ++i)
    RequestInjection(extension);

  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  runner()->RunForTesting(extension);

  // All pending injections should have executed.
  EXPECT_EQ(kNumInjections, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(runner()->WantsToRun(extension));
}

TEST_F(ExtensionActionRunnerUnitTest, ActiveScriptsUseActiveTabPermissions) {
  const Extension* extension = AddExtension();
  NavigateAndCommit(GURL("https://www.google.com"));

  ActiveTabPermissionGranter* active_tab_permission_granter =
      TabHelper::FromWebContents(web_contents())
          ->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_permission_granter);
  // Grant the extension active tab permissions. This normally happens, e.g.,
  // if the user clicks on a browser action.
  active_tab_permission_granter->GrantIfRequested(extension);

  // Since we have active tab permissions, we shouldn't need user consent
  // anymore.
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Reloading and other same-origin navigations maintain the permission to
  // execute.
  content::NavigationSimulator::Reload(web_contents());
  EXPECT_FALSE(RequiresUserConsent(extension));
  NavigateAndCommit(GURL("https://www.google.com/foo"));
  EXPECT_FALSE(RequiresUserConsent(extension));
  NavigateAndCommit(GURL("https://www.google.com/bar"));
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Navigating to a different origin will require user consent again.
  NavigateAndCommit(GURL("https://yahoo.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Back to the original origin should also re-require constent.
  NavigateAndCommit(GURL("https://www.google.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  RequestInjection(extension);
  EXPECT_TRUE(runner()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Grant active tab.
  active_tab_permission_granter->GrantIfRequested(extension);

  // The pending injections should have run since active tab permission was
  // granted.
  EXPECT_EQ(1u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(runner()->WantsToRun(extension));
}

TEST_F(ExtensionActionRunnerUnitTest, ActiveScriptsCanHaveAllUrlsPref) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.google.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Enable the extension on all urls.
  ScriptingPermissionsModifier permissions_modifier(profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(false);

  EXPECT_FALSE(RequiresUserConsent(extension));
  // This should carry across navigations, and websites.
  NavigateAndCommit(GURL("http://www.foo.com"));
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Turning off the preference should have instant effect.
  permissions_modifier.SetWithholdHostPermissions(true);
  EXPECT_TRUE(RequiresUserConsent(extension));

  // And should also persist across navigations and websites.
  NavigateAndCommit(GURL("http://www.bar.com"));
  EXPECT_TRUE(RequiresUserConsent(extension));
}

TEST_F(ExtensionActionRunnerUnitTest, TestAlwaysRun) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.google.com/?gws_rd=ssl"));

  // Ensure that there aren't any executions pending.
  ASSERT_EQ(0u, GetExecutionCountForExtension(extension->id()));
  ASSERT_FALSE(runner()->WantsToRun(extension));

  // Since the extension requests all_hosts, we should require user consent.
  EXPECT_TRUE(RequiresUserConsent(extension));

  // Request an injection. The extension should want to run, but not execute.
  RequestInjection(extension);
  EXPECT_TRUE(runner()->WantsToRun(extension));
  EXPECT_EQ(0u, GetExecutionCountForExtension(extension->id()));

  // Allow the extension to always run on this origin.
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.GrantHostPermission(web_contents()->GetLastCommittedURL());
  runner()->RunForTesting(extension);

  // The extension should execute, and the extension shouldn't want to run.
  EXPECT_EQ(1u, GetExecutionCountForExtension(extension->id()));
  EXPECT_FALSE(runner()->WantsToRun(extension));

  // Since we already executed on the given page, we shouldn't need permission
  // for a second time.
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Navigating to another site that hasn't been granted a persisted permission
  // should necessitate user consent.
  NavigateAndCommit(GURL("https://www.foo.com/bar"));
  EXPECT_TRUE(RequiresUserConsent(extension));

  // We shouldn't need user permission upon returning to the original origin.
  NavigateAndCommit(GURL("https://www.google.com/foo/bar"));
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Reloading the extension should not clear any granted host permissions.
  extension = ReloadExtension();
  content::NavigationSimulator::Reload(web_contents());
  EXPECT_FALSE(RequiresUserConsent(extension));

  // Different host...
  NavigateAndCommit(GURL("https://www.foo.com/bar"));
  EXPECT_TRUE(RequiresUserConsent(extension));
  // Different scheme...
  NavigateAndCommit(GURL("http://www.google.com/foo/bar"));
  EXPECT_TRUE(RequiresUserConsent(extension));
  // Different subdomain...
  NavigateAndCommit(GURL("https://en.google.com/foo/bar"));
  EXPECT_TRUE(RequiresUserConsent(extension));
  // Only the "always run" origin should be allowed to run without user consent.
  NavigateAndCommit(GURL("https://www.google.com/foo/bar"));
  EXPECT_FALSE(RequiresUserConsent(extension));
}

TEST_F(ExtensionActionRunnerUnitTest, TestDifferentScriptRunLocations) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.foo.com"));

  EXPECT_EQ(BLOCKED_ACTION_NONE, runner()->GetBlockedActions(extension));

  RequestInjection(extension, UserScript::DOCUMENT_END);
  EXPECT_EQ(BLOCKED_ACTION_SCRIPT_OTHER,
            runner()->GetBlockedActions(extension));
  RequestInjection(extension, UserScript::DOCUMENT_IDLE);
  EXPECT_EQ(BLOCKED_ACTION_SCRIPT_OTHER,
            runner()->GetBlockedActions(extension));
  RequestInjection(extension, UserScript::DOCUMENT_START);
  EXPECT_EQ(BLOCKED_ACTION_SCRIPT_AT_START | BLOCKED_ACTION_SCRIPT_OTHER,
            runner()->GetBlockedActions(extension));

  runner()->RunForTesting(extension);
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner()->GetBlockedActions(extension));
}

TEST_F(ExtensionActionRunnerUnitTest, TestWebRequestBlocked) {
  const Extension* extension = AddExtension();
  ASSERT_TRUE(extension);

  NavigateAndCommit(GURL("https://www.foo.com"));

  EXPECT_EQ(BLOCKED_ACTION_NONE, runner()->GetBlockedActions(extension));
  EXPECT_FALSE(runner()->WantsToRun(extension));

  runner()->OnWebRequestBlocked(extension);
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST, runner()->GetBlockedActions(extension));
  EXPECT_TRUE(runner()->WantsToRun(extension));

  RequestInjection(extension);
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST | BLOCKED_ACTION_SCRIPT_OTHER,
            runner()->GetBlockedActions(extension));
  EXPECT_TRUE(runner()->WantsToRun(extension));

  NavigateAndCommit(GURL("https://www.bar.com"));
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner()->GetBlockedActions(extension));
  EXPECT_FALSE(runner()->WantsToRun(extension));
}

}  // namespace extensions
