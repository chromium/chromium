// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_permission_request.h"
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/permissions/attestation_permission_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_impl.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char* kPermissionsKillSwitchFieldStudy =
    permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* kPermissionsKillSwitchBlockedValue =
    permissions::PermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";

// Test implementation of NotificationPermissionUiSelector that always
// returns a canned decision.
class TestQuietNotificationPermissionUiSelector
    : public permissions::NotificationPermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      const Decision& canned_decision)
      : canned_decision_(canned_decision) {}
  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::NotificationPermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(canned_decision_);
  }

 private:
  Decision canned_decision_;

  DISALLOW_COPY_AND_ASSIGN(TestQuietNotificationPermissionUiSelector);
};

class PermissionRequestManagerBrowserTest : public InProcessBrowserTest {
 public:
  PermissionRequestManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        permissions::features::kBlockRepeatedNotificationPermissionPrompts);
  }

  ~PermissionRequestManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    permissions::PermissionRequestManager* manager =
        GetPermissionRequestManager();
    mock_permission_prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    ShutDownFirstTabMockPermissionPromptFactory();
  }

  void ShutDownFirstTabMockPermissionPromptFactory() {
    mock_permission_prompt_factory_.reset();
  }

  permissions::PermissionRequestManager* GetPermissionRequestManager() {
    return permissions::PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  permissions::MockPermissionPromptFactory* bubble_factory() {
    return mock_permission_prompt_factory_.get();
  }

  void EnableKillSwitch(ContentSettingsType content_settings_type) {
    std::map<std::string, std::string> params;
    params[permissions::PermissionUtil::GetPermissionString(
        content_settings_type)] = kPermissionsKillSwitchBlockedValue;
    variations::AssociateVariationParams(kPermissionsKillSwitchFieldStudy,
                                         kPermissionsKillSwitchTestGroup,
                                         params);
    base::FieldTrialList::CreateFieldTrial(kPermissionsKillSwitchFieldStudy,
                                           kPermissionsKillSwitchTestGroup);
  }

  void TriggerAndExpectPromptCooldownToBeStillActiveAfterNavigationAction(
      void navigation_action(content::WebContents*, const GURL&),
      bool expect_cooldown) {
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL kInitialURL = embedded_test_server()->GetURL(
        "a.localhost", "/permissions/killswitch_tester.html");
    const GURL kSecondURL = embedded_test_server()->GetURL(
        "b.localhost", "/permissions/killswitch_tester.html");
    const GURL kThirdURL = embedded_test_server()->GetURL(
        "c.localhost", "/permissions/killswitch_tester.html");

    ui_test_utils::NavigateToURL(browser(), kInitialURL);
    bubble_factory()->ResetCounts();
    bubble_factory()->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

    // Simulate a notification permission request that is denied by the user.
    std::string result;
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractString(
        web_contents, "requestNotification();", &result));
    ASSERT_EQ(1, bubble_factory()->show_count());
    ASSERT_EQ(1, bubble_factory()->TotalRequestCount());
    ASSERT_EQ("denied", result);

    // In response, simulate the website automatically triggering a
    // renderer-initiated cross-origin navigation without user gesture.
    content::TestNavigationObserver navigation_observer(web_contents);
    ASSERT_TRUE(content::ExecuteScriptWithoutUserGesture(
        web_contents, "window.location = \"" + kSecondURL.spec() + "\";"));
    navigation_observer.Wait();

    bubble_factory()->ResetCounts();
    bubble_factory()->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    // Request the notification permission again from a different origin.
    // Cross-origin permission prompt cool-down should be in effect.
    ASSERT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractString(
        web_contents, "requestNotification();", &result));
    ASSERT_EQ(0, bubble_factory()->show_count());
    ASSERT_EQ(0, bubble_factory()->TotalRequestCount());
    ASSERT_EQ("default", result);

    // Now try one of a number other kinds of navigations, and request the
    // notification permission again.
    navigation_action(web_contents, kThirdURL);
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents, "requestNotification();", &result));

    // Cross-origin prompt cool-down may or may not be in effect anymore
    // depending on the type of navigation.
    if (expect_cooldown) {
      EXPECT_EQ(0, bubble_factory()->show_count());
      EXPECT_EQ(0, bubble_factory()->TotalRequestCount());
      EXPECT_EQ("default", result);
    } else {
      EXPECT_EQ(1, bubble_factory()->show_count());
      EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
      EXPECT_EQ("granted", result);
    }
  }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestManagerBrowserTest);
};

class PermissionRequestManagerWithBackForwardCacheBrowserTest
    : public PermissionRequestManagerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PermissionRequestManagerBrowserTest::SetUpCommandLine(command_line);
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {features::kBackForwardCacheMemoryControls});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Requests before the load event should be bundled into one bubble.
// http://crbug.com/512849 flaky
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       DISABLED_RequestsBeforeLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory()->TotalRequestCount());
}

// Requests before the load should not be bundled with a request after the load.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       RequestsBeforeAfterLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-after-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

// Navigating twice to the same URL should be equivalent to refresh. This means
// showing the bubbles twice.
// http://crbug.com/512849 flaky
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest, DISABLED_NavTwice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(2, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory()->TotalRequestCount());
}

// Navigating twice to the same URL with a hash should be navigation within the
// page. This means the bubble is only shown once.
// http://crbug.com/512849 flaky
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       DISABLED_NavTwiceWithHash) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-load.html#0"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

// Bubble requests should be shown after same-document navigation.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       SameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/empty.html"), 1);

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/empty.html#0"), 1);

  // Request 'geolocation' permission.
  ExecuteScriptAndGetValue(
      GetActiveMainFrame(),
      "navigator.geolocation.getCurrentPosition(function(){});");
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

// Prompts are only shown for active tabs and (on Desktop) hidden on tab
// switching
// Flaky on Win bots crbug.com/1003747.
#if defined(OS_WIN)
#define MAYBE_MultipleTabs DISABLED_MultipleTabs
#else
#define MAYBE_MultipleTabs MultipleTabs
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       MAYBE_MultipleTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/empty.html"), 1);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // SetUp() only creates a mock prompt factory for the first tab.
  permissions::MockPermissionPromptFactory* bubble_factory_0 = bubble_factory();
  std::unique_ptr<permissions::MockPermissionPromptFactory> bubble_factory_1(
      std::make_unique<permissions::MockPermissionPromptFactory>(
          GetPermissionRequestManager()));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(1, tab_strip_model->active_index());

  // Request geolocation in foreground tab, prompt should be shown.
  ExecuteScriptAndGetValue(
      tab_strip_model->GetWebContentsAt(1)->GetMainFrame(),
      "navigator.geolocation.getCurrentPosition(function(){});");
  EXPECT_EQ(1, bubble_factory_1->show_count());
  EXPECT_FALSE(bubble_factory_0->is_visible());
  EXPECT_TRUE(bubble_factory_1->is_visible());

  tab_strip_model->ActivateTabAt(0);
  EXPECT_FALSE(bubble_factory_0->is_visible());
  EXPECT_FALSE(bubble_factory_1->is_visible());

  tab_strip_model->ActivateTabAt(1);
  EXPECT_EQ(2, bubble_factory_1->show_count());
  EXPECT_FALSE(bubble_factory_0->is_visible());
  EXPECT_TRUE(bubble_factory_1->is_visible());

  // Request notification in background tab. No prompt is shown until the tab
  // itself is activated.
  ExecuteScriptAndGetValue(tab_strip_model->GetWebContentsAt(0)->GetMainFrame(),
                           "Notification.requestPermission()");
  EXPECT_FALSE(bubble_factory_0->is_visible());
  EXPECT_EQ(2, bubble_factory_1->show_count());

  tab_strip_model->ActivateTabAt(0);
  EXPECT_TRUE(bubble_factory_0->is_visible());
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory_1->show_count());
}

// Regularly timing out in Windows, Linux and macOS Debug Builds.
// https://crbug.com/931657
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       DISABLED_BackgroundTabNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/empty.html"), 1);

  // Request camera, prompt should be shown.
  ExecuteScriptAndGetValue(
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetMainFrame(),
      "navigator.getUserMedia({video: true}, ()=>{}, ()=>{})");
  bubble_factory()->WaitForPermissionBubble();
  EXPECT_TRUE(bubble_factory()->is_visible());
  EXPECT_EQ(1, bubble_factory()->show_count());

  // SetUp() only creates a mock prompt factory for the first tab but this test
  // doesn't request any permissions in the second tab so it doesn't need one.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Navigate background tab, prompt should be removed.
  ExecuteScriptAndGetValue(
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetMainFrame(),
      "window.location = 'simple.html'");
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  observer.Wait();
  EXPECT_FALSE(bubble_factory()->is_visible());

  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_FALSE(bubble_factory()->is_visible());
  EXPECT_EQ(1, bubble_factory()->show_count());
}

// Bubble requests should not be shown when the killswitch is on.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       KillSwitchGeolocation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(web_contents, "requestGeolocation();"));
  bubble_factory()->WaitForPermissionBubble();
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Now enable the geolocation killswitch.
  EnableKillSwitch(ContentSettingsType::GEOLOCATION);

  // Reload the page to get around blink layer caching for geolocation
  // requests.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html"));

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "requestGeolocation();", &result));
  EXPECT_EQ("denied", result);
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

// Regression test for crbug.com/900997.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       CrossOriginPromptCooldown) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kInitialURL = embedded_test_server()->GetURL(
      "a.localhost", "/permissions/killswitch_tester.html");
  const GURL kSecondURL = embedded_test_server()->GetURL(
      "b.localhost", "/permissions/killswitch_tester.html");

  ui_test_utils::NavigateToURL(browser(), kInitialURL);
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Simulate a notification permission request that is denied by the user.
  std::string result;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractString(
      web_contents, "requestNotification();", &result));
  ASSERT_EQ(1, bubble_factory()->show_count());
  ASSERT_EQ(1, bubble_factory()->TotalRequestCount());
  ASSERT_EQ("denied", result);

  // In response, simulate the website automatically triggering a
  // renderer-initiated cross-origin navigation without user gesture.
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(content::ExecuteScriptWithoutUserGesture(
      web_contents, "window.location = \"" + kSecondURL.spec() + "\";"));
  navigation_observer.Wait();

  // Request the notification permission again from a different origin.
  // Cross-origin permission prompt cool-down should be in effect.
  bubble_factory()->ResetCounts();
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
  ASSERT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractString(
      web_contents, "requestNotification();", &result));
  EXPECT_EQ(0, bubble_factory()->show_count());
  EXPECT_EQ(0, bubble_factory()->TotalRequestCount());
  EXPECT_EQ("default", result);
}

// Regression test for crbug.com/900997.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       CooldownEndsOnUserInitiatedReload) {
  TriggerAndExpectPromptCooldownToBeStillActiveAfterNavigationAction(
      [](content::WebContents* web_contents, const GURL& unused_url) {
        content::NavigationController& controller =
            web_contents->GetController();
        controller.Reload(content::ReloadType::NORMAL, false);
        EXPECT_TRUE(content::WaitForLoadStop(web_contents));
      },
      false /* expect_cooldown */);
}

// Regression test for crbug.com/900997.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       CooldownEndsOnBrowserInitiateNavigation) {
  TriggerAndExpectPromptCooldownToBeStillActiveAfterNavigationAction(
      [](content::WebContents* web_contents, const GURL& url) {
        EXPECT_TRUE(content::NavigateToURL(web_contents, url));
      },
      false /* expect_cooldown */);
}

// Regression test for crbug.com/900997.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       CooldownEndsOnRendererInitiateNavigationWithGesture) {
  TriggerAndExpectPromptCooldownToBeStillActiveAfterNavigationAction(
      [](content::WebContents* web_contents, const GURL& url) {
        content::TestNavigationObserver navigation_observer(web_contents);
        EXPECT_TRUE(content::ExecuteScript(
            web_contents, "window.location = \"" + url.spec() + "\";"));
        navigation_observer.Wait();
      },
      false /* expect_cooldown */);
}

// Regression test for crbug.com/900997.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       CooldownOutlastsRendererInitiatedReload) {
  TriggerAndExpectPromptCooldownToBeStillActiveAfterNavigationAction(
      [](content::WebContents* web_contents, const GURL& unused_url) {
        content::TestNavigationObserver navigation_observer(web_contents);
        EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
            web_contents, "window.location.reload();"));
        navigation_observer.Wait();
      },
      true /* expect_cooldown */);
}

// Regression test for crbug.com/900997.
IN_PROC_BROWSER_TEST_F(
    PermissionRequestManagerBrowserTest,
    CooldownOutlastsRendererInitiateNavigationWithoutGesture) {
  TriggerAndExpectPromptCooldownToBeStillActiveAfterNavigationAction(
      [](content::WebContents* web_contents, const GURL& url) {
        content::TestNavigationObserver navigation_observer(web_contents);
        EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
            web_contents, "window.location = \"" + url.spec() + "\";"));
        navigation_observer.Wait();
      },
      true /* expect_cooldown */);
}

// Bubble requests should not be shown when the killswitch is on.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       KillSwitchNotifications) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(web_contents, "requestNotification();"));
  bubble_factory()->WaitForPermissionBubble();
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Now enable the notifications killswitch.
  EnableKillSwitch(ContentSettingsType::NOTIFICATIONS);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "requestNotification();", &result));
  EXPECT_EQ("denied", result);
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       PendingRequestsDisableBackForwardCache) {
  content::BackForwardCacheDisabledTester back_forward_cache_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-after-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  content::RenderFrameHost* main_frame = GetActiveMainFrame();
  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int main_frame_routing_id = main_frame->GetRoutingID();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("b.com", "/title1.html"), 1);
  EXPECT_TRUE(back_forward_cache_tester.IsDisabledForFrameWithReason(
      main_frame_process_id, main_frame_routing_id,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kPermissionRequestManager)));
}

class PermissionRequestManagerQuietUiBrowserTest
    : public PermissionRequestManagerBrowserTest {
 public:
  PermissionRequestManagerQuietUiBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kQuietNotificationPrompts);
  }

 protected:
  using UiDecision = permissions::NotificationPermissionUiSelector::Decision;
  using QuietUiReason =
      permissions::NotificationPermissionUiSelector::QuietUiReason;
  using WarningReason =
      permissions::NotificationPermissionUiSelector::WarningReason;

  void SetCannedUiDecision(base::Optional<QuietUiReason> quiet_ui_reason,
                           base::Optional<WarningReason> warning_reason) {
    GetPermissionRequestManager()
        ->set_notification_permission_ui_selector_for_testing(
            std::make_unique<TestQuietNotificationPermissionUiSelector>(
                UiDecision(quiet_ui_reason, warning_reason)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Re-enable when 1016233 is fixed.
// Quiet permission requests are cancelled when a new request is made.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerQuietUiBrowserTest,
                       DISABLED_QuietPendingRequestsKilledOnNewRequest) {
  content::RenderFrameHost* source_frame = GetActiveMainFrame();
  // First add a quiet permission request. Ensure that this request is decided
  // by the end of this test.
  permissions::MockPermissionRequest request_quiet(
      "quiet", permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(source_frame, &request_quiet);
  base::RunLoop().RunUntilIdle();

  // Add a second permission request. This ones should cause the initial
  // request to be cancelled.
  permissions::MockPermissionRequest request_loud(
      "loud", permissions::RequestType::kGeolocation,
      permissions::PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(source_frame, &request_loud);
  base::RunLoop().RunUntilIdle();

  // The first dialog should now have been decided.
  EXPECT_TRUE(request_quiet.finished());
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Cleanup remaining request. And check that this was the last request.
  GetPermissionRequestManager()->Closing();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetPermissionRequestManager()->Requests().size());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerQuietUiBrowserTest,
                       PermissionPromptDisposition) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      WarningReason::kAbusiveContent);

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  permissions::MockPermissionRequest request_quiet(
      "quiet", permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(web_contents->GetMainFrame(),
                                            &request_quiet);

  bubble_factory()->WaitForPermissionBubble();
  auto* manager = GetPermissionRequestManager();

  base::Optional<permissions::PermissionPromptDisposition> disposition =
      manager->current_request_prompt_disposition_for_testing();
  auto disposition_from_prompt_bubble =
      manager->view_for_testing()->GetPromptDisposition();

  manager->Closing();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(disposition.has_value());
  EXPECT_EQ(disposition.value(), disposition_from_prompt_bubble);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerQuietUiBrowserTest,
                       PermissionPromptDispositionHidden) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      WarningReason::kAbusiveContent);

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  permissions::MockPermissionRequest request_quiet(
      "quiet", permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(web_contents->GetMainFrame(),
                                            &request_quiet);

  bubble_factory()->WaitForPermissionBubble();
  auto* manager = GetPermissionRequestManager();
  auto disposition_from_prompt_bubble =
      manager->view_for_testing()->GetPromptDisposition();

  // There will be no instance of PermissionPromptImpl after a tab marked as
  // HIDDEN.
  manager->OnVisibilityChanged(content::Visibility::HIDDEN);

  base::Optional<permissions::PermissionPromptDisposition> disposition =
      manager->current_request_prompt_disposition_for_testing();

  EXPECT_TRUE(disposition.has_value());
  EXPECT_EQ(disposition.value(), disposition_from_prompt_bubble);

  //  DCHECK failure if Closing executed on HIDDEN PermissionRequestManager.
  manager->OnVisibilityChanged(content::Visibility::VISIBLE);
  manager->Closing();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerQuietUiBrowserTest,
                       ConsoleMessages) {
  const struct {
    base::Optional<QuietUiReason> simulated_quiet_ui_reason;
    base::Optional<WarningReason> simulated_warning_reason;
    const char* expected_message;
  } kTestCases[] = {
      {UiDecision::UseNormalUi(), UiDecision::ShowNoWarning(), nullptr},
      {QuietUiReason::kEnabledInPrefs, UiDecision::ShowNoWarning(), nullptr},
      {QuietUiReason::kTriggeredByCrowdDeny, UiDecision::ShowNoWarning(),
       nullptr},
      {QuietUiReason::kTriggeredDueToAbusiveRequests,
       UiDecision::ShowNoWarning(),
       permissions::kAbusiveNotificationRequestsEnforcementMessage},
      {UiDecision::UseNormalUi(), WarningReason::kAbusiveRequests,
       permissions::kAbusiveNotificationRequestsWarningMessage},
      {QuietUiReason::kTriggeredDueToAbusiveContent,
       UiDecision::ShowNoWarning(),
       permissions::kAbusiveNotificationContentEnforcementMessage},
      {UiDecision::UseNormalUi(), WarningReason::kAbusiveContent,
       permissions::kAbusiveNotificationContentWarningMessage},
  };

  constexpr char kCounterVerificationPattern[] = "NOTHING";

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test index: " << (&test - kTestCases));

    SetCannedUiDecision(test.simulated_quiet_ui_reason,
                        test.simulated_warning_reason);

    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    content::WebContentsConsoleObserver console_observer(web_contents);

    permissions::MockPermissionRequest request_quiet(
        "quiet", permissions::RequestType::kNotifications,
        permissions::PermissionRequestGestureType::UNKNOWN);
    GetPermissionRequestManager()->AddRequest(web_contents->GetMainFrame(),
                                              &request_quiet);

    bubble_factory()->WaitForPermissionBubble();
    GetPermissionRequestManager()->Closing();
    base::RunLoop().RunUntilIdle();

    if (!test.expected_message) {
      web_contents->GetMainFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          kCounterVerificationPattern);
    }

    console_observer.Wait();

    ASSERT_EQ(1u, console_observer.messages().size());
    if (test.expected_message) {
      EXPECT_EQ(test.expected_message, console_observer.GetMessageAt(0));
      EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kWarning,
                console_observer.messages()[0].log_level);
    } else {
      EXPECT_EQ(kCounterVerificationPattern, console_observer.GetMessageAt(0));
    }
  }
}

// Two loud requests are simply queued one after another.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       LoudPendingRequestsQueued) {
  content::RenderFrameHost* source_frame = GetActiveMainFrame();
  permissions::MockPermissionRequest request1(
      "request1", permissions::RequestType::kClipboard,
      permissions::PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(source_frame, &request1);
  base::RunLoop().RunUntilIdle();

  permissions::MockPermissionRequest request2(
      "request2", permissions::RequestType::kGeolocation,
      permissions::PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(source_frame, &request2);
  base::RunLoop().RunUntilIdle();

  // Both requests are still pending (though only one is active).
  EXPECT_FALSE(request1.finished());
  EXPECT_FALSE(request2.finished());
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Close first request.
  GetPermissionRequestManager()->Closing();
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(permissions::features::kPermissionChip)) {
    EXPECT_FALSE(request1.finished());
    EXPECT_TRUE(request2.finished());
  } else {
    EXPECT_TRUE(request1.finished());
    EXPECT_FALSE(request2.finished());
  }
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Close second request. No more requests pending
  GetPermissionRequestManager()->Closing();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(request1.finished());
  EXPECT_TRUE(request2.finished());
  EXPECT_EQ(0u, GetPermissionRequestManager()->Requests().size());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerWithBackForwardCacheBrowserTest,
                       NoPermissionBubbleShownForPagesInCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");

  ui_test_utils::NavigateToURL(browser(), url_a);
  content::RenderFrameHost* rfh_a = GetActiveMainFrame();
  content::RenderFrameDeletedObserver a_observer(rfh_a);

  ui_test_utils::NavigateToURL(browser(), url_b);
  ASSERT_FALSE(a_observer.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  permissions::MockPermissionRequest req;
  GetPermissionRequestManager()->AddRequest(rfh_a, &req);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, bubble_factory()->show_count());
  EXPECT_EQ(0, bubble_factory()->TotalRequestCount());
  // Page gets evicted if bubble would have been showed
  EXPECT_TRUE(a_observer.deleted());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerWithBackForwardCacheBrowserTest,
                       RequestsForPagesInCacheNotGrouped) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");

  ui_test_utils::NavigateToURL(browser(), url_a);
  content::RenderFrameHost* rfh_a = GetActiveMainFrame();
  content::RenderFrameDeletedObserver a_observer(rfh_a);

  ui_test_utils::NavigateToURL(browser(), url_b);
  ASSERT_FALSE(a_observer.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  content::RenderFrameHost* rfh_b = GetActiveMainFrame();

  // PERMISSION_MEDIASTREAM_MIC, PERMISSION_MEDIASTREAM_CAMERA, and
  // PERMISSION_CAMERA_PAN_TILT_ZOOM requests are grouped if they come from the
  // same origin. Make sure this will not include requests from a cached frame.
  // Note pages will not be cached when navigating within the same origin, so we
  // have different urls in the navigations above but use the same url (default)
  // for the MockPermissionRequest here.
  permissions::MockPermissionRequest req_a_1(
      "req_a_1", permissions::RequestType::kCameraPanTiltZoom,
      permissions::PermissionRequestGestureType::GESTURE);
  permissions::MockPermissionRequest req_a_2(
      "req_a_2", permissions::RequestType::kCameraPanTiltZoom,
      permissions::PermissionRequestGestureType::GESTURE);
  permissions::MockPermissionRequest req_b_1(
      "req_b_1", permissions::RequestType::kCameraStream,
      permissions::PermissionRequestGestureType::GESTURE);
  permissions::MockPermissionRequest req_b_2(
      "req_b_2", permissions::RequestType::kMicStream,
      permissions::PermissionRequestGestureType::GESTURE);
  GetPermissionRequestManager()->AddRequest(rfh_a,
                                            &req_a_1);  // Should be skipped
  GetPermissionRequestManager()->AddRequest(rfh_b, &req_b_1);
  GetPermissionRequestManager()->AddRequest(rfh_a,
                                            &req_a_2);  // Should be skipped
  GetPermissionRequestManager()->AddRequest(rfh_b, &req_b_2);

  bubble_factory()->WaitForPermissionBubble();

  // One bubble with the two grouped requests and none of the skipped ones.
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory()->TotalRequestCount());
  EXPECT_TRUE(req_a_1.cancelled());
  EXPECT_TRUE(req_a_2.cancelled());

  // Page gets evicted if bubble would have been showed.
  EXPECT_TRUE(a_observer.deleted());

  // Cleanup before we delete the requests.
  GetPermissionRequestManager()->Closing();
}

class PermissionRequestManagerOneTimeGeolocationPermissionBrowserTest
    : public PermissionRequestManagerBrowserTest {
 public:
  PermissionRequestManagerOneTimeGeolocationPermissionBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        permissions::features::kOneTimeGeolocationPermission);
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(0, 0);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_F(
    PermissionRequestManagerOneTimeGeolocationPermissionBrowserTest,
    RequestForPermission) {
  const char kQueryCurrentPosition[] = R"(
        navigator.geolocation.getCurrentPosition(
          _ => domAutomationController.send('success'),
          _ => domAutomationController.send('failure'));
      )";
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/title1.html"), 1);
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ONCE);

  // Request 'geolocation' permission.
  std::string result = content::EvalJsWithManualReply(GetActiveMainFrame(),
                                                      kQueryCurrentPosition)
                           .ExtractString();
  EXPECT_EQ("success", result);
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Request 'geolocation' permission. There should not be a 2nd prompt.
  result = content::EvalJsWithManualReply(GetActiveMainFrame(),
                                          kQueryCurrentPosition)
               .ExtractString();
  EXPECT_EQ("success", result);
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Open a new tab with same domain.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Create a new mock permission prompt factory for the second tab.
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      second_tab_bubble_factory(
          std::make_unique<permissions::MockPermissionPromptFactory>(
              GetPermissionRequestManager()));

  // Request 'geolocation' permission.
  result = content::EvalJsWithManualReply(GetActiveMainFrame(),
                                          kQueryCurrentPosition)
               .ExtractString();
  EXPECT_EQ("success", result);
  // There should be no permission prompt.
  EXPECT_EQ(0, second_tab_bubble_factory.get()->TotalRequestCount());

  // Open a new empty tab before closing the first two tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Need to close the mock permission managers before closing the tabs.
  // Otherwise the tab instances can't be destroyed due to a DCHECK
  ShutDownFirstTabMockPermissionPromptFactory();
  second_tab_bubble_factory.reset();

  // Close the first two tabs.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->CloseWebContentsAt(0, TabStripModel::CLOSE_USER_GESTURE);
  tab_strip_model->CloseWebContentsAt(0, TabStripModel::CLOSE_USER_GESTURE);

  ASSERT_EQ(1, tab_strip_model->count());

  // Create a new mock permission prompt factory for the third tab.
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      third_tab_bubble_factory(
          std::make_unique<permissions::MockPermissionPromptFactory>(
              GetPermissionRequestManager()));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/title1.html"), 1);
  third_tab_bubble_factory.get()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ONCE);

  // Request 'geolocation' permission. We should get a prompt.
  result = content::EvalJsWithManualReply(GetActiveMainFrame(),
                                          kQueryCurrentPosition)
               .ExtractString();
  EXPECT_EQ("success", result);

  EXPECT_EQ(1, third_tab_bubble_factory.get()->TotalRequestCount());
}

}  // anonymous namespace
