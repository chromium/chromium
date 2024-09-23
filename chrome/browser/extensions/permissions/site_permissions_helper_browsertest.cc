// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions/site_permissions_helper.h"

#include <string_view>

#include "base/run_loop.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using UserSiteAccess = PermissionsManager::UserSiteAccess;

}  // namespace

class SitePermissionsHelperBrowserTest : public ExtensionBrowserTest {
 public:
  SitePermissionsHelperBrowserTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Loads an extension that can run on every page at document start. Then
    // loads a test page and confirm it is running on the page.
    ASSERT_TRUE(embedded_test_server()->Start());
    extension_ = LoadExtension(
        test_data_dir_.AppendASCII("blocked_actions/content_scripts"));
    ASSERT_TRUE(extension_);

    // Navigate to a page where the extension wants to run.
    original_url_ = embedded_test_server()->GetURL("/simple.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), original_url_));
    ASSERT_TRUE(content::WaitForLoadStop(active_web_contents()));
    original_nav_id_ =
        active_nav_controller().GetLastCommittedEntry()->GetUniqueID();

    // The extension should want to run on the page, script should have
    // injected, and should have "on all sites" access.
    ASSERT_TRUE(active_action_runner());
    ASSERT_FALSE(active_action_runner()->WantsToRun(extension_));
    ASSERT_EQ(
        ContentScriptsInfo::GetContentScripts(extension_)[0]->run_location(),
        mojom::RunLocation::kDocumentStart);
    ASSERT_TRUE(active_web_contents());
    ASSERT_TRUE(ContentScriptInjected());
    permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());
    permissions_manager_ = PermissionsManager::Get(profile());
    ASSERT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnAllSites);
  }

  void TearDownOnMainThread() override {
    ExtensionBrowserTest::TearDownOnMainThread();
    // Extension is created as a scoped_refptr so no need to delete.
    extension_ = nullptr;
    // Avoid dangling pointer.
    permissions_manager_ = nullptr;
    // Avoid dangling pointer to profile.
    permissions_helper_.reset(nullptr);
  }

  // The content script for the extension was successfully injected into the
  // page.
  bool ContentScriptInjected();
  // Extension has blocked actions that are pending to run.
  bool ExtensionWantsToRun();

  bool ReloadPageAndWaitForLoad();
  bool WaitForReloadToFinish();

  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::NavigationController& active_nav_controller() {
    return active_web_contents()->GetController();
  }

  ExtensionActionRunner* active_action_runner() {
    return ExtensionActionRunner::GetForWebContents(active_web_contents());
  }

 protected:
  int original_nav_id_{0};
  GURL original_url_{};
  raw_ptr<const Extension> extension_;
  std::unique_ptr<SitePermissionsHelper> permissions_helper_;
  raw_ptr<PermissionsManager> permissions_manager_;
};

bool SitePermissionsHelperBrowserTest::ContentScriptInjected() {
  return browsertest_util::DidChangeTitle(*active_web_contents(),
                                          /*original_title=*/u"OK",
                                          /*changed_title=*/u"success");
}

bool SitePermissionsHelperBrowserTest::ExtensionWantsToRun() {
  return active_action_runner()->WantsToRun(extension_);
}

bool SitePermissionsHelperBrowserTest::ReloadPageAndWaitForLoad() {
  active_web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                                false);
  return WaitForReloadToFinish();
}

bool SitePermissionsHelperBrowserTest::WaitForReloadToFinish() {
  // This is needed in the instance where on site -> on-click revokes
  // permissions. This is because when testing we run
  // `ExtensionActionRunner::accept_bubble_for_testing(true)` which causes
  // `ExtensionActionRunner::ShowReloadPageBubble(...)` to run the reload with
  // a `base::SingleThreadTaskRunner` so we must wait for that to complete.
  base::RunLoop().RunUntilIdle();
  return content::WaitForLoadStop(active_web_contents());
}

// TODO(crbug.com/40883928): Paramertize these test scenarios (and the setup as
// well). This would allow us to concisely describe the multiple state changes
// and expected end states without having an individual test case for each or
// (as below) have two large tests that rely on previous tests steps creating
// state to proceed successfully.

// Tests the various states of permission changes that can occur. When changes
// occur we automatically accept the reload bubble, confirm the content script
// for the extension is running/not running, and that we are still on the same
// page after changing permissions. User site access changes are expected to be
// immediate. There are many ASSERTS here because each test case is relying on
// the previous changes completing in order to properly test its scenario.
// Scenarios tested (in order):
//
//  on all sites -> on site
//  on site -> on-click (refresh needed due to revoking permissions)
//  on click -> on site  (refresh needed due to script wanting to load at start)
//  on site -> on all sites
//  on all sites -> on-click (refresh needed due to revoking permissions)
IN_PROC_BROWSER_TEST_F(SitePermissionsHelperBrowserTest,
                       UpdateSiteAccess_AcceptReloadBubble) {
  // By default, test setup should set site access to be on all sites.
  ASSERT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnAllSites);
  active_action_runner()->accept_bubble_for_testing(true);

  // on all sites -> on site
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnSite);
  // We assume that there is only ever one action that wants to run for the test
  // extension used by these tests. Anything else is an unexpected change, bug,
  // or a flaw in the test.
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  // on site -> on-click (refresh needed due to revoking permissions)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnClick);
  ASSERT_TRUE(WaitForReloadToFinish());
  ASSERT_FALSE(ContentScriptInjected());
  ASSERT_TRUE(ExtensionWantsToRun());

  // on click -> on site (refresh needed due to script wanting to load at
  // start)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnSite);
  ASSERT_TRUE(WaitForReloadToFinish());
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  // on site -> on all sites
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnAllSites);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnAllSites);
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  // on all sites -> on-click (refresh needed due to revoking permissions)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnClick);
  EXPECT_TRUE(WaitForReloadToFinish());
  EXPECT_FALSE(ContentScriptInjected());
  EXPECT_TRUE(ExtensionWantsToRun());
}

// Tests the various states of permission changes that can occur. When changes
// occur we automatically dismiss the reload bubble, confirm the content script
// for the extension is running/not running, and that we are still on the same
// page after changing permissions. User site access changes are expected to be
// immediate, but a reload is expected so we instead simulate reloading via the
// "Reload this page" button. There are many ASSERTS here because each test case
// is relying on the previous changes completing in order to properly test its
// scenario. Scenarios tested (in order):
//
//  on all sites -> on site
//  on site -> on-click (refresh needed, and done manually, due to revoking
//    permissions)
//  on click -> on site (refresh needed, and done manually, due to
//    script wanting to load at start)
//  on site -> on all sites on all sites -> on-click (refresh needed, and done
//    manually due to revoking permissions)
IN_PROC_BROWSER_TEST_F(
    SitePermissionsHelperBrowserTest,
    UpdateSiteAccess_DismissReloadBubble_ReloadPageManually) {
  // By default, test setup should set site access to be on all sites.
  ASSERT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnAllSites);
  // Reload will not happen via the user reload bubble.
  active_action_runner()->accept_bubble_for_testing(false);

  // on all sites -> on site
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnSite);
  // We assume that there is only ever one action that wants to run for the test
  // extension used by these tests. Anything else is an unexpected change, bug,
  // or a flaw in the test.
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  // on site -> on-click (refresh needed due to revoking permissions)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnClick);
  EXPECT_TRUE(ContentScriptInjected() && !ExtensionWantsToRun());
  ASSERT_TRUE(ReloadPageAndWaitForLoad());
  ASSERT_FALSE(ContentScriptInjected());
  ASSERT_TRUE(ExtensionWantsToRun());

  // on click -> on site (refresh needed due to script wanting to load at
  // start)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnSite);
  EXPECT_TRUE(!ContentScriptInjected() && ExtensionWantsToRun());
  ASSERT_TRUE(ReloadPageAndWaitForLoad());
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  // on site -> on all sites
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnAllSites);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnAllSites);
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  // on all sites -> on-click (refresh needed due to revoking permissions)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnClick);
  EXPECT_TRUE(ContentScriptInjected() && !ExtensionWantsToRun());
  ASSERT_TRUE(ReloadPageAndWaitForLoad());
  EXPECT_TRUE(!ContentScriptInjected() && ExtensionWantsToRun());
}

// Provides test cases with an extension that executes a script programmatically
// on every site it visits.
class SitePermissionsHelperExecuteSciptBrowserTest
    : public SitePermissionsHelperBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("a.com", "127.0.0.1");
    host_resolver()->AddRule("b.com", "127.0.0.1");

    ExtensionBrowserTest::SetUpOnMainThread();
    // Loads an extension that executes a script on every page that is navigated
    // to. Then loads a test page and confirms it is running on the page.
    ASSERT_TRUE(embedded_test_server()->Start());
    extension_ = LoadExtension(test_data_dir_.AppendASCII(
        "blocked_actions/revoke_execute_script_on_click"));
    ASSERT_TRUE(extension_);

    // Navigate to a page where the extension can run.
    original_url_ = embedded_test_server()->GetURL("/simple.html");
    ExtensionTestMessageListener listener("injection succeeded");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), original_url_));
    ASSERT_TRUE(active_web_contents());
    ASSERT_TRUE(content::WaitForLoadStop(active_web_contents()));

    permissions_manager_ = PermissionsManager::Get(profile());
    ASSERT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnAllSites);

    ASSERT_TRUE(listener.WaitUntilSatisfied());
    ASSERT_TRUE(active_action_runner());
    ASSERT_TRUE(ContentScriptInjected());
    ASSERT_FALSE(ExtensionWantsToRun());

    permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());
    original_nav_id_ =
        active_nav_controller().GetLastCommittedEntry()->GetUniqueID();
  }

  // Navigates to `host_name` with `relative_url`. `host_name` must be added as
  // a rule in SetUpOnMainThread().
  void NavigateTo(std::string_view host_name, std::string_view relative_url) {
    GURL url = embedded_test_server()->GetURL(host_name, relative_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }
};

// Tests that active tab is cleared when we revoke site permissions of an
// extension that injects a script programmatically into the page. To fix
// crbug.com/1433399.
IN_PROC_BROWSER_TEST_F(
    SitePermissionsHelperExecuteSciptBrowserTest,
    UpdateSiteAccess_RevokingSitePermission_AlsoClearsActiveTab) {
  // We want to control refreshes manually due to timing issues with permissions
  // being updated across browser/renderer.
  active_action_runner()->accept_bubble_for_testing(true);

  {
    // on all sites -> on click (revokes access)
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnClick);
    ASSERT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnClick);
    ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                      active_web_contents()),
              SitePermissionsHelper::SiteInteraction::kWithheld);
    ASSERT_TRUE(WaitForReloadToFinish());
    blocked_action_waiter.Wait();
    ASSERT_FALSE(ContentScriptInjected());
    ASSERT_TRUE(ExtensionWantsToRun());
  }

  ExtensionTestMessageListener listener("injection succeeded");
  // on click -> on site (grants site access and active tab permission)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnSite);
  ASSERT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnSite);
  ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                    active_web_contents()),
            SitePermissionsHelper::SiteInteraction::kGranted);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  {
    // on site -> on-click (should remove site access and active tab
    // permissions)
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnClick);
    ASSERT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnClick);
    ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                      active_web_contents()),
              SitePermissionsHelper::SiteInteraction::kWithheld);
    ASSERT_TRUE(WaitForReloadToFinish());
    blocked_action_waiter.Wait();
    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }

  {
    // Confirm that unintended access isn't just waiting for a reload to allow
    // it to run.
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    ASSERT_TRUE(ReloadPageAndWaitForLoad());
    ASSERT_TRUE(WaitForReloadToFinish());
    blocked_action_waiter.Wait();
    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }
}

// Tests that active tab is cleared when we revoke site permissions after
// granting active tab permissions of an extension that injects a script
// programmatically into the page.  To fix crbug.com/1433399.
IN_PROC_BROWSER_TEST_F(
    SitePermissionsHelperExecuteSciptBrowserTest,
    UpdateSiteAccess_RevokingSitePermissionAfterGrantTab_AlsoClearsActiveTab) {
  active_action_runner()->accept_bubble_for_testing(true);

  {
    // on all sites -> on click (revokes access)
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnClick);
    ASSERT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnClick);
    ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                      active_web_contents()),
              SitePermissionsHelper::SiteInteraction::kWithheld);
    ASSERT_TRUE(WaitForReloadToFinish());
    blocked_action_waiter.Wait();
    ASSERT_FALSE(ContentScriptInjected());
    ASSERT_TRUE(ExtensionWantsToRun());
  }

  ExtensionTestMessageListener listener("injection succeeded");
  // Grant active tab independently.
  active_action_runner()->RunAction(extension_, /*grant_tab_permissions=*/true);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  // on click -> on site (grants site access and redundantly active tab
  // permission)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnSite);
  ASSERT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnSite);
  ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                    active_web_contents()),
            SitePermissionsHelper::SiteInteraction::kGranted);
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  {
    // on site -> on-click (should remove site access and active tab
    // permissions)
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnClick);
    ASSERT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnClick);
    ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                      active_web_contents()),
              SitePermissionsHelper::SiteInteraction::kWithheld);
    ASSERT_TRUE(WaitForReloadToFinish());
    blocked_action_waiter.Wait();
    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }

  {
    // Confirm that unintended access isn't just waiting for a reload to allow
    // it to run.
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    ASSERT_TRUE(ReloadPageAndWaitForLoad());
    ASSERT_TRUE(WaitForReloadToFinish());
    blocked_action_waiter.Wait();
    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }
}

// Tests that active tab is cleared when we renavigate to a site previously
// granted one-time tab permission after a cross-origin navigation. Regression
// rest for b/324455951.
IN_PROC_BROWSER_TEST_F(SitePermissionsHelperExecuteSciptBrowserTest,
                       CrossOriginRenavigationClearsGrantedTabPermission) {
  active_action_runner()->accept_bubble_for_testing(true);

  // Withheld extension's site access.
  ScriptingPermissionsModifier(profile(), extension_.get())
      .SetWithholdHostPermissions(true);

  {
    // Navigate to a.com. Script is not injected since extension has withheld
    // site access.
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    NavigateTo("a.com", "/simple.html");
    blocked_action_waiter.Wait();
    ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                      active_web_contents()),
              SitePermissionsHelper::SiteInteraction::kWithheld);
    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }

  {
    // Grant tab permissions to a.com. Script is injected (one time).
    ExtensionTestMessageListener script_injection_listener(
        "injection succeeded");
    active_action_runner()->GrantTabPermissions({extension_.get()});
    ASSERT_TRUE(WaitForReloadToFinish());
    ASSERT_TRUE(script_injection_listener.WaitUntilSatisfied());
    EXPECT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                      active_web_contents()),
              SitePermissionsHelper::SiteInteraction::kGranted);
    EXPECT_TRUE(ContentScriptInjected());
    EXPECT_FALSE(ExtensionWantsToRun());
  }

  {
    // Navigate to b.com. Script is not injected since extension has withheld
    // site access.
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    NavigateTo("b.com", "/simple.html");
    blocked_action_waiter.Wait();
    EXPECT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                      active_web_contents()),
              SitePermissionsHelper::SiteInteraction::kWithheld);
    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }

  {
    // Navigate back to a.com. Since we navigated to another origin, and then
    // back to a.com it should not have tab permissions anymore. Thus, the
    // script is not injected.
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    NavigateTo("a.com", "/simple.html");
    blocked_action_waiter.Wait();
    EXPECT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                      active_web_contents()),
              SitePermissionsHelper::SiteInteraction::kWithheld);
    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }
}

class SitePermissionsHelperContentScriptBrowserTest
    : public SitePermissionsHelperBrowserTest {
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    // Loads an extension that injects a content script on every page that is
    // navigated to on document_end. Then loads a test page and confirms it is
    // running on the page.
    ASSERT_TRUE(embedded_test_server()->Start());
    extension_ = LoadExtension(
        test_data_dir_.AppendASCII("blocked_actions/content_script_at_end"));
    ASSERT_TRUE(extension_);

    // Navigate to a page where the extension can run.
    original_url_ = embedded_test_server()->GetURL("/simple.html");
    ExtensionTestMessageListener listener("injection succeeded");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), original_url_));
    ASSERT_TRUE(active_web_contents());
    ASSERT_TRUE(content::WaitForLoadStop(active_web_contents()));

    permissions_manager_ = PermissionsManager::Get(profile());
    ASSERT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnAllSites);

    ASSERT_TRUE(listener.WaitUntilSatisfied());
    ASSERT_TRUE(active_action_runner());
    ASSERT_TRUE(ContentScriptInjected());
    ASSERT_FALSE(ExtensionWantsToRun());

    permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());
    original_nav_id_ =
        active_nav_controller().GetLastCommittedEntry()->GetUniqueID();
  }
};

// Tests that active tab is cleared when we revoke site permissions of an
// extension that injects a content script. To fix crbug.com/1433399.
IN_PROC_BROWSER_TEST_F(
    SitePermissionsHelperContentScriptBrowserTest,
    UpdateSiteAccess_RevokingSitePermission_AlsoClearsActiveTab) {
  // We want to control refreshes manually due to timing issues with permissions
  // being updated across browser/renderer.
  active_action_runner()->accept_bubble_for_testing(true);

  // on all sites -> on click (revokes access)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnClick);
  ASSERT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnClick);
  ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                    active_web_contents()),
            SitePermissionsHelper::SiteInteraction::kWithheld);
  ASSERT_TRUE(WaitForReloadToFinish());
  ASSERT_FALSE(ContentScriptInjected());
  ASSERT_TRUE(ExtensionWantsToRun());

  ExtensionTestMessageListener listener("injection succeeded");
  // on click -> on site (grants site access and active tab permission)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnSite);
  ASSERT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnSite);
  ASSERT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                    active_web_contents()),
            SitePermissionsHelper::SiteInteraction::kGranted);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_TRUE(ContentScriptInjected());
  ASSERT_FALSE(ExtensionWantsToRun());

  // on site -> on-click (should remove site access and active tab permissions)
  permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                        UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_helper_->GetSiteInteraction(*extension_,
                                                    active_web_contents()),
            SitePermissionsHelper::SiteInteraction::kWithheld);
  ASSERT_TRUE(WaitForReloadToFinish());
  EXPECT_FALSE(ContentScriptInjected());
  EXPECT_TRUE(ExtensionWantsToRun());

  // Confirm that unintended access isn't just waiting for a reload to allow it
  // to run.
  ASSERT_TRUE(ReloadPageAndWaitForLoad());
  EXPECT_FALSE(ContentScriptInjected());
  EXPECT_TRUE(ExtensionWantsToRun());
}

class SitePermissionsHelperOptionalHostPermissions
    : public SitePermissionsHelperBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    // Loads an extension that, when granted the optional host permission,
    // executes a script on every page that is navigated to. Then loads a test
    // page and confirms it is running on the page.
    ASSERT_TRUE(embedded_test_server()->Start());
    extension_ = LoadExtension(test_data_dir_.AppendASCII(
        "blocked_actions/optional_host_permissions"));
    ASSERT_TRUE(extension_);

    // Grant the optional host permissions.
    permissions_manager_ = PermissionsManager::Get(profile());
    permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
        profile(), *extension_,
        PermissionsParser::GetOptionalPermissions(extension_));

    // Navigate to a page where the extension can run.
    original_url_ = embedded_test_server()->GetURL("/simple.html");
    ExtensionTestMessageListener listener("success");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), original_url_));
    ASSERT_TRUE(active_web_contents());
    ASSERT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnAllSites);

    ASSERT_TRUE(listener.WaitUntilSatisfied());
    ASSERT_TRUE(active_action_runner());
    ASSERT_TRUE(ContentScriptInjected());
    ASSERT_FALSE(ExtensionWantsToRun());

    permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());
  }
};

// Tests the behavior when granted optional host permissions are altered by
// updating site access. The scenarios tested here are
// on all sites -> on site
// on site -> on click
// on click -> on site
// on site -> on all sites
// on all sites -> on click
IN_PROC_BROWSER_TEST_F(SitePermissionsHelperOptionalHostPermissions,
                       UpdateSiteAccess) {
  // By default, test setup should set site access to be on all sites.
  ASSERT_EQ(permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
            UserSiteAccess::kOnAllSites);
  active_action_runner()->accept_bubble_for_testing(true);

  {
    // on all sites -> on site.
    ExtensionTestMessageListener listener("success");
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnSite);
    EXPECT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnSite);
    // Changing the site access to on site should still allow script injection.
    // Reloading the page to verify if the scripts were re-injected.
    ASSERT_TRUE(ReloadPageAndWaitForLoad());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(ContentScriptInjected());
    EXPECT_FALSE(ExtensionWantsToRun());
  }
  {
    // on site -> on-click (refresh needed due to revoking permissions).
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnClick);
    EXPECT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnClick);
    ASSERT_TRUE(WaitForReloadToFinish());
    blocked_action_waiter.Wait();

    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }
  {
    // on click -> on site
    ExtensionTestMessageListener listener("success");
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnSite);
    EXPECT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnSite);
    ASSERT_TRUE(WaitForReloadToFinish());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(ContentScriptInjected());
    EXPECT_FALSE(ExtensionWantsToRun());
  }
  {
    // on site -> on all sites
    ExtensionTestMessageListener listener("success");
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnAllSites);
    EXPECT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnAllSites);
    // Reloading the page to verify if the scripts were re-injected.
    ASSERT_TRUE(ReloadPageAndWaitForLoad());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(ContentScriptInjected());
    EXPECT_FALSE(ExtensionWantsToRun());
  }
  {
    // on all sites -> on-click
    browsertest_util::BlockedActionWaiter blocked_action_waiter(
        active_action_runner());
    permissions_helper_->UpdateSiteAccess(*extension_, active_web_contents(),
                                          UserSiteAccess::kOnClick);
    EXPECT_EQ(
        permissions_manager_->GetUserSiteAccess(*extension_, original_url_),
        UserSiteAccess::kOnClick);
    ASSERT_TRUE(WaitForReloadToFinish());
    blocked_action_waiter.Wait();
    EXPECT_FALSE(ContentScriptInjected());
    EXPECT_TRUE(ExtensionWantsToRun());
  }
}

}  // namespace extensions
