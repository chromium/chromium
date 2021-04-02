// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/navigation_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {

// A class for testing various scenarios of disabled extensions.
class DisableExtensionBrowserTest : public ExtensionBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    extension_ = LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));

    extension_id_ = extension_->id();
    extension_resource_url_ = extension_->GetResourceURL("file.html");

    NavigationObserver::SetAllowedRepeatedPromptingForTesting(true);
    ASSERT_TRUE(extension_);

    registry_ = ExtensionRegistry::Get(profile());
    EXPECT_TRUE(registry_->enabled_extensions().Contains(extension_id_));

    prefs_ = ExtensionPrefs::Get(profile());
  }

  // We always navigate in a new tab because when we disable the extension, it
  // closes all tabs for that extension. If we only opened in the current tab,
  // this would result in the only open tab being closed, and the test
  // quitting.
  void NavigateToUrlInNewTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // TODO(lukasza): https://crbug.com/970917: We should not terminate a renderer
  // that hosts a disabled extension.  Once that is fixed, we should remove
  // ScopedAllowRendererCrashes below.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  scoped_refptr<const Extension> extension_;
  std::string extension_id_;
  GURL extension_resource_url_;
  ExtensionRegistry* registry_;
  ExtensionPrefs* prefs_;
};

// Test that visiting an url associated with a disabled extension offers to
// re-enable it.
IN_PROC_BROWSER_TEST_F(
    DisableExtensionBrowserTest,
    PromptToReEnableExtensionsOnNavigation_PermissionsIncrease) {
  base::ScopedClosureRunner reset_repeated_prompting(base::BindOnce([]() {
    NavigationObserver::SetAllowedRepeatedPromptingForTesting(false);
  }));
  // Disable the extension due to a permissions increase.
  extension_service()->DisableExtension(
      extension_id_, disable_reason::DISABLE_PERMISSIONS_INCREASE);
  EXPECT_TRUE(registry_->disabled_extensions().Contains(extension_id_));

  EXPECT_EQ(disable_reason::DISABLE_PERMISSIONS_INCREASE,
            prefs_->GetDisableReasons(extension_id_));

  {
    // Visit an associated url and deny the prompt. The extension should remain
    // disabled.
    ScopedTestDialogAutoConfirm auto_deny(ScopedTestDialogAutoConfirm::CANCEL);
    NavigateToUrlInNewTab(extension_resource_url_);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(registry_->disabled_extensions().Contains(extension_id_));
    EXPECT_EQ(disable_reason::DISABLE_PERMISSIONS_INCREASE,
              prefs_->GetDisableReasons(extension_id_));
  }

  {
    // Visit an associated url and accept the prompt. The extension should get
    // re-enabled.
    ScopedTestDialogAutoConfirm auto_accept(
        ScopedTestDialogAutoConfirm::ACCEPT);
    NavigateToUrlInNewTab(extension_resource_url_);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(registry_->enabled_extensions().Contains(extension_id_));
    EXPECT_EQ(disable_reason::DISABLE_NONE,
              prefs_->GetDisableReasons(extension_id_));
  }
}

// Test that visiting an url associated with a disabled extension offers to
// re-enable it.
IN_PROC_BROWSER_TEST_F(DisableExtensionBrowserTest,
                       PromptToReEnableExtensionsOnNavigation_UserAction) {
  // Disable the extension for something other than a permissions increase.
  extension_service()->DisableExtension(extension_id_,
                                        disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(registry_->disabled_extensions().Contains(extension_id_));
  EXPECT_EQ(disable_reason::DISABLE_USER_ACTION,
            prefs_->GetDisableReasons(extension_id_));

  {
    // We only prompt for permissions increases, not any other disable reason.
    // As such, the extension should stay disabled.
    ScopedTestDialogAutoConfirm auto_accept(
        ScopedTestDialogAutoConfirm::ACCEPT);
    NavigateToUrlInNewTab(extension_resource_url_);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(registry_->disabled_extensions().Contains(extension_id_));
    EXPECT_EQ(disable_reason::DISABLE_USER_ACTION,
              prefs_->GetDisableReasons(extension_id_));
  }
}

// Test that visiting an url associated with a disabled hosted app offers to
// re-enable it.
IN_PROC_BROWSER_TEST_F(DisableExtensionBrowserTest,
                       PromptToReEnableHostedAppOnNavigation) {
  // Load a hosted app and disable it for a permissions increase.
  scoped_refptr<const Extension> hosted_app =
      LoadExtension(test_data_dir_.AppendASCII("hosted_app"));
  ASSERT_TRUE(hosted_app);
  const std::string kHostedAppId = hosted_app->id();
  const GURL kHostedAppUrl("http://localhost/extensions/hosted_app/main.html");
  EXPECT_EQ(hosted_app, registry_->enabled_extensions().GetExtensionOrAppByURL(
                            kHostedAppUrl));

  extension_service()->DisableExtension(
      kHostedAppId, disable_reason::DISABLE_PERMISSIONS_INCREASE);
  EXPECT_TRUE(registry_->disabled_extensions().Contains(kHostedAppId));
  EXPECT_EQ(disable_reason::DISABLE_PERMISSIONS_INCREASE,
            prefs_->GetDisableReasons(kHostedAppId));

  {
    // When visiting a site that's associated with a hosted app, but not a
    // chrome-extension url, we don't prompt to re-enable. This is to avoid
    // prompting when visiting a regular website like calendar.google.com.
    // See crbug.com/678631.
    ScopedTestDialogAutoConfirm auto_accept(
        ScopedTestDialogAutoConfirm::ACCEPT);
    NavigateToUrlInNewTab(kHostedAppUrl);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(registry_->disabled_extensions().Contains(kHostedAppId));
    EXPECT_EQ(disable_reason::DISABLE_PERMISSIONS_INCREASE,
              prefs_->GetDisableReasons(kHostedAppId));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, NoExtensionsInRefererHeader) {
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<const Extension> extension =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);
  GURL page_url = extension->GetResourceURL("file.html");
  ui_test_utils::NavigateToURL(browser(), page_url);

  // Click a link in the extension.
  GURL target_url = embedded_test_server()->GetURL("/echoheader?referer");
  const char kScriptTemplate[] = R"(
      let a = document.createElement('a');
      a.href = $1;
      document.body.appendChild(a);
      a.click();
  )";
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents, 1);
  ExecuteScriptAsync(web_contents,
                     content::JsReplace(kScriptTemplate, target_url));

  // Wait for navigation to complete and verify it was successful.
  nav_observer.WaitForNavigationFinished();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(target_url, nav_observer.last_navigation_url());
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());

  // Verify that the Referrer header was not present (in particular, it should
  // not reveal the identity of the extension).
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ("None", content::EvalJs(web_contents, "document.body.innerText"));

  // Verify that the initiator_origin was present and set to the extension.
  ASSERT_TRUE(nav_observer.last_initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(page_url),
            nav_observer.last_initiator_origin());
}

}  // namespace extensions
