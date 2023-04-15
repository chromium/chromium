// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/site_permissions_helper.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/test/extension_test_message_listener.h"
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

// TODO(crbug.com/1400812): Paramertize these test scenarios (and the setup as
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

}  // namespace extensions
