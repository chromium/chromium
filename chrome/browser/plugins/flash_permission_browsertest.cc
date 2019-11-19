// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permissions_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "url/gurl.h"

namespace {

class PageReloadWaiter {
 public:
  explicit PageReloadWaiter(content::WebContents* web_contents)
      : web_contents_(web_contents),
        navigation_observer_(web_contents,
                             web_contents->GetLastCommittedURL()) {}

  bool Wait() {
    navigation_observer_.WaitForNavigationFinished();
    return content::WaitForLoadStop(web_contents_);
  }

 private:
  content::WebContents* web_contents_;
  content::TestNavigationManager navigation_observer_;
};

}  // namespace

class FlashPermissionBrowserTest : public PermissionsBrowserTest {
 public:
  FlashPermissionBrowserTest()
      : PermissionsBrowserTest("/permissions/flash.html") {}
  ~FlashPermissionBrowserTest() override {}

  // PermissionsBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PermissionsBrowserTest::SetUpCommandLine(command_line);

    ASSERT_TRUE(ppapi::RegisterFlashTestPlugin(command_line));

    // These tests are for the permission prompt to add and remove Flash from
    // navigator.plugins. We disable Plugin Power Saver, because its plugin
    // throttling make it harder to test if Flash was succcessfully enabled.
    command_line->AppendSwitchASCII(
        switches::kOverridePluginPowerSaverForTesting, "never");
  }

  void SetUpOnMainThread() override {
    PermissionsBrowserTest::SetUpOnMainThread();

    // This browser test verifies the Flash permission prompt behavior. The
    // permission prompt only appears when Flash permission is set to DETECT.
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetDefaultContentSetting(ContentSettingsType::PLUGINS,
                                   CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
  }

  void TearDownOnMainThread() override {
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetDefaultContentSetting(ContentSettingsType::PLUGINS,
                                   CONTENT_SETTING_DEFAULT);

    PermissionsBrowserTest::TearDownOnMainThread();
  }

  void TriggerPrompt() override {
    if (prompt_factory()->response_type() ==
        PermissionRequestManager::ACCEPT_ALL) {
      // If the prompt will be allowed, we need to wait for the page to refresh.
      PageReloadWaiter reload_waiter(GetWebContents());
      EXPECT_TRUE(RunScriptReturnBool("triggerPrompt();"));
      EXPECT_TRUE(reload_waiter.Wait());
    } else {
      EXPECT_TRUE(RunScriptReturnBool("triggerPrompt();"));
      // Make a round trip to the renderer to flush any old did stop IPCs,
      // otherwise they can race with the next navigation and cause it to be
      // cancelled if it's the same URL.
      EXPECT_TRUE(ExecuteScript(GetWebContents(), std::string()));
    }
  }

  bool FeatureUsageSucceeds() override {
    // If flash should have been blocked, reload the page to be sure that it is
    // blocked.
    //
    // NB: In cases where flash is allowed the page reloads automatically,
    // and tests should always wait for that reload to finish before calling
    // this method.
    ui_test_utils::NavigateToURL(browser(),
                                 GetWebContents()->GetLastCommittedURL());
    // If either flash with or without fallback content runs successfully it
    // indicates the feature is at least partly working, which could imply a
    // faulty permission.
    return RunScriptReturnBool("flashIsEnabled();") ||
           RunScriptReturnBool("flashIsEnabledForPluginWithoutFallback();");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FlashPermissionBrowserTest);
};

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest,
                       CommonFailsBeforeRequesting) {
  CommonFailsBeforeRequesting();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, CommonFailsIfDismissed) {
  CommonFailsIfDismissed();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, CommonFailsIfBlocked) {
  CommonFailsIfBlocked();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, CommonSucceedsIfAllowed) {
  CommonSucceedsIfAllowed();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, SucceedsInPopupWindow) {
  // Spawn the same page in a popup window and wait for it to finish loading.
  content::WebContents* original_contents = GetWebContents();
  ASSERT_TRUE(RunScriptReturnBool("spawnPopupAndAwaitLoad();"));

  // Assert that the popup's WebContents is now the active one.
  ASSERT_NE(original_contents, GetWebContents());

  PermissionRequestManager* manager = PermissionRequestManager::FromWebContents(
      GetWebContents());
  auto popup_prompt_factory =
      std::make_unique<MockPermissionPromptFactory>(manager);

  EXPECT_EQ(0, popup_prompt_factory->TotalRequestCount());
  popup_prompt_factory->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  // FlashPermissionContext::UpdateTabContext will reload the page, we'll have
  // to wait until it is ready.
  PageReloadWaiter reload_waiter(GetWebContents());
  EXPECT_TRUE(RunScriptReturnBool("triggerPrompt();"));
  EXPECT_TRUE(reload_waiter.Wait());

  EXPECT_TRUE(FeatureUsageSucceeds());
  EXPECT_EQ(1, popup_prompt_factory->TotalRequestCount());

  // Shut down the popup window tab, as the normal test teardown assumes there
  // is only one test tab.
  popup_prompt_factory.reset();
  GetWebContents()->Close();
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, TriggerPromptViaNewWindow) {
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  // FlashPermissionContext::UpdateTabContext will reload the page, we'll have
  // to wait until it is ready.
  PageReloadWaiter reload_waiter(GetWebContents());
  EXPECT_TRUE(RunScriptReturnBool("triggerPromptViaNewWindow();"));
  EXPECT_TRUE(reload_waiter.Wait());

  EXPECT_TRUE(FeatureUsageSucceeds());
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest,
                       TriggerPromptViaPluginPlaceholder) {
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  EXPECT_FALSE(FeatureUsageSucceeds());
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);
  // We need to simulate a mouse click to trigger the placeholder to prompt.
  // When the prompt is auto-accepted, the page will be reloaded.
  PageReloadWaiter reload_waiter(GetWebContents());
  content::SimulateMouseClickAt(GetWebContents(), 0 /* modifiers */,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(50, 50));
  EXPECT_TRUE(reload_waiter.Wait());

  EXPECT_TRUE(FeatureUsageSucceeds());
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest,
                       TriggerPromptViaMainFrameNavigationWithoutUserGesture) {
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  EXPECT_FALSE(FeatureUsageSucceeds());
  prompt_factory()->set_response_type(PermissionRequestManager::ACCEPT_ALL);

  PageReloadWaiter reload_waiter(GetWebContents());

  // Unlike the other tests, this JavaScript is called without a user gesture.
  GetWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("triggerPromptWithMainFrameNavigation();"),
      base::NullCallback());

  EXPECT_TRUE(reload_waiter.Wait());

  EXPECT_TRUE(FeatureUsageSucceeds());
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, AllowFileURL) {
  base::FilePath test_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_path);
  ui_test_utils::NavigateToURL(
      browser(), GURL("file://" + test_path.AsUTF8Unsafe() + test_url()));
  CommonSucceedsIfAllowed();
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());

  // Navigate to a second URL to verify it's allowed on all file: URLs.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("file://" + test_path.AsUTF8Unsafe() + "/permissions/flash2.html"));
  EXPECT_TRUE(FeatureUsageSucceeds());
}

IN_PROC_BROWSER_TEST_F(FlashPermissionBrowserTest, BlockFileURL) {
  base::FilePath test_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_path);
  ui_test_utils::NavigateToURL(
      browser(), GURL("file://" + test_path.AsUTF8Unsafe() + test_url()));
  CommonFailsIfBlocked();
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());

  // Navigate to a second URL to verify it's blocked on all file: URLs.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("file://" + test_path.AsUTF8Unsafe() + "/permissions/flash2.html"));
  EXPECT_FALSE(FeatureUsageSucceeds());
}
