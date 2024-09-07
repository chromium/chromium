// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/custom_handlers/register_protocol_handler_permission_request.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
const char* kPermissionsKillSwitchFieldStudy =
    permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* kPermissionsKillSwitchBlockedValue =
    permissions::PermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";

// Test implementation of PermissionUiSelector that always returns a canned
// decision.
class TestQuietNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      const Decision& canned_decision)
      : canned_decision_(canned_decision) {}

  TestQuietNotificationPermissionUiSelector(
      const TestQuietNotificationPermissionUiSelector&) = delete;
  TestQuietNotificationPermissionUiSelector& operator=(
      const TestQuietNotificationPermissionUiSelector&) = delete;

  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(canned_decision_);
  }

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override {
    return request_type == permissions::RequestType::kNotifications;
  }

 private:
  Decision canned_decision_;
};

class PermissionRequestManagerBrowserTest : public InProcessBrowserTest {
 public:
  PermissionRequestManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {permissions::features::kBlockRepeatedNotificationPermissionPrompts},
        {permissions::features::kBackForwardCacheUnblockPermissionRequest});
  }

  PermissionRequestManagerBrowserTest(
      const PermissionRequestManagerBrowserTest&) = delete;
  PermissionRequestManagerBrowserTest& operator=(
      const PermissionRequestManagerBrowserTest&) = delete;

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
    base::AssociateFieldTrialParams(kPermissionsKillSwitchFieldStudy,
                                    kPermissionsKillSwitchTestGroup, params);
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

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialURL));
    bubble_factory()->ResetCounts();
    bubble_factory()->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

    // Simulate a notification permission request that is denied by the user.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_EQ("denied",
              content::EvalJs(web_contents, "requestNotification();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    ASSERT_EQ(1, bubble_factory()->show_count());
    ASSERT_EQ(1, bubble_factory()->TotalRequestCount());

    // In response, simulate the website automatically triggering a
    // renderer-initiated cross-origin navigation without user gesture.
    content::TestNavigationObserver navigation_observer(web_contents);
    ASSERT_TRUE(content::ExecJs(
        web_contents, "window.location = \"" + kSecondURL.spec() + "\";",
        content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    navigation_observer.Wait();

    bubble_factory()->ResetCounts();
    bubble_factory()->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    // Request the notification permission again from a different origin.
    // Cross-origin permission prompt cool-down should be in effect.
    ASSERT_EQ("default",
              content::EvalJs(web_contents, "requestNotification();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    ASSERT_EQ(0, bubble_factory()->show_count());
    ASSERT_EQ(0, bubble_factory()->TotalRequestCount());

    // Now try one of a number other kinds of navigations, and request the
    // notification permission again.
    navigation_action(web_contents, kThirdURL);
    std::string result =
        content::EvalJs(web_contents, "requestNotification();",
                        content::EXECUTE_SCRIPT_NO_USER_GESTURE)
            .ExtractString();

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
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
};

class PermissionRequestManagerWithBackForwardCacheBrowserTest
    : public PermissionRequestManagerBrowserTest {
 public:
  PermissionRequestManagerWithBackForwardCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PermissionRequestManagerWithPrerenderingTest
    : public PermissionRequestManagerBrowserTest {
 public:
  PermissionRequestManagerWithPrerenderingTest()
      : prerender_test_helper_(base::BindRepeating(
            &PermissionRequestManagerWithPrerenderingTest::GetWebContents,
            base::Unretained(this))) {}

  void SetUp() override {
    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
    PermissionRequestManagerBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    PermissionRequestManagerBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_test_helper_;
  }

 private:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper prerender_test_helper_;
};

class PermissionRequestManagerWithBackForwardCacheUnblockBrowserTest
    : public PermissionRequestManagerWithBackForwardCacheBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PermissionRequestManagerBrowserTest::SetUpCommandLine(command_line);
    feature_list_.InitAndEnableFeature(
        permissions::features::kBackForwardCacheUnblockPermissionRequest);
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

// Requests before the load should not be bundled with a request after the
// load.
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

// Navigating twice to the same URL should be equivalent to refresh. This
// means showing the bubbles twice. http://crbug.com/512849 flaky
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

// Navigating twice to the same URL with a hash should be navigation within
// the page. This means the bubble is only shown once. http://crbug.com/512849
// flaky
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
  ASSERT_TRUE(content::ExecJs(
      GetActiveMainFrame(),
      "navigator.geolocation.getCurrentPosition(function(){});"));
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

// Ignored permission request should not trigger a blocked activity indicator on
// a new document.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       SameOriginCrossDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/empty.html"), 1);

  auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
      GetActiveMainFrame());

  ASSERT_TRUE(pscs);
  EXPECT_FALSE(pscs->IsContentBlocked(ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(pscs->IsContentAllowed(ContentSettingsType::GEOLOCATION));

  // Request 'geolocation' permission.
  ASSERT_TRUE(content::ExecJs(
      GetActiveMainFrame(),
      "navigator.geolocation.getCurrentPosition(function(){});"));

  bubble_factory()->WaitForPermissionBubble();
  EXPECT_TRUE(bubble_factory()->is_visible());

  // Start a same-origin cross-document navigation. This will resolve currently
  // visible permission prompt as `Ignored`.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/empty.html"), 1);

  EXPECT_FALSE(bubble_factory()->is_visible());

  // After a new started navigation PSCS will be deleted. Get a new instance.
  pscs = content_settings::PageSpecificContentSettings::GetForFrame(
      GetActiveMainFrame());
  // Geolocation content setting was not blocked nor allowed. In other words,
  // there is no visible activity indicator after Geolocation permission prompt
  // was resolved as `Ignored`.
  EXPECT_FALSE(pscs->IsContentBlocked(ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(pscs->IsContentAllowed(ContentSettingsType::GEOLOCATION));
}

// Prompts are only shown for active tabs and (on Desktop) hidden on tab
// switching
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest, MultipleTabs) {
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

  constexpr char kRequestNotifications[] = R"(
      new Promise(resolve => {
        Notification.requestPermission().then(function (permission) {
          resolve(permission)
        });
      })
      )";

  {
    permissions::PermissionRequestObserver observer(
        tab_strip_model->GetWebContentsAt(1));

    // Request permission in foreground tab, prompt should be shown.
    EXPECT_TRUE(content::ExecJs(
        tab_strip_model->GetWebContentsAt(1)->GetPrimaryMainFrame(),
        kRequestNotifications,
        content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    observer.Wait();
  }

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

  {
    permissions::PermissionRequestObserver observer(
        tab_strip_model->GetWebContentsAt(0));

    // Request notification in background tab. No prompt is shown until the
    // tab itself is activated.
    EXPECT_TRUE(content::ExecJs(
        tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame(),
        kRequestNotifications,
        content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    observer.Wait();
    EXPECT_TRUE(observer.is_prompt_show_failed_hidden_tab());
  }

  EXPECT_FALSE(bubble_factory_0->is_visible());
  EXPECT_EQ(2, bubble_factory_1->show_count());

  tab_strip_model->ActivateTabAt(0);
  EXPECT_TRUE(bubble_factory_0->is_visible());
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory_1->show_count());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       BackgroundTabNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/empty.html"), 1);

  // Request Notifications, prompt should be shown.
  ASSERT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame(),
      "Notification.requestPermission()",
      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  bubble_factory()->WaitForPermissionBubble();
  EXPECT_TRUE(bubble_factory()->is_visible());
  EXPECT_EQ(1, bubble_factory()->show_count());

  // SetUp() only creates a mock prompt factory for the first tab but this
  // test doesn't request any permissions in the second tab so it doesn't need
  // one.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Navigate background tab, prompt should be removed.
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  ASSERT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame(),
      "window.location = 'simple.html'"));

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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecJs(web_contents, "requestGeolocation();",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  bubble_factory()->WaitForPermissionBubble();
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Now enable the geolocation killswitch.
  EnableKillSwitch(ContentSettingsType::GEOLOCATION);

  // Reload the page to get around blink layer caching for geolocation
  // requests.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html")));

  EXPECT_EQ("denied", content::EvalJs(web_contents, "requestGeolocation();"));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialURL));
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  // Simulate a notification permission request that is denied by the user.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ("denied", content::EvalJs(web_contents, "requestNotification();",
                                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_EQ(1, bubble_factory()->show_count());
  ASSERT_EQ(1, bubble_factory()->TotalRequestCount());

  // In response, simulate the website automatically triggering a
  // renderer-initiated cross-origin navigation without user gesture.
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents, "window.location = \"" + kSecondURL.spec() + "\";",
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  navigation_observer.Wait();

  // Request the notification permission again from a different origin.
  // Cross-origin permission prompt cool-down should be in effect.
  bubble_factory()->ResetCounts();
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
  ASSERT_EQ("default",
            content::EvalJs(web_contents, "requestNotification();",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(0, bubble_factory()->show_count());
  EXPECT_EQ(0, bubble_factory()->TotalRequestCount());
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
        EXPECT_TRUE(content::ExecJs(
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
        EXPECT_TRUE(content::ExecJs(web_contents, "window.location.reload();",
                                    content::EXECUTE_SCRIPT_NO_USER_GESTURE));
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
        EXPECT_TRUE(content::ExecJs(web_contents,
                                    "window.location = \"" + url.spec() + "\";",
                                    content::EXECUTE_SCRIPT_NO_USER_GESTURE));
        navigation_observer.Wait();
      },
      true /* expect_cooldown */);
}

// Bubble requests should not be shown when the killswitch is on.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       KillSwitchNotifications) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecJs(web_contents, "requestNotification();",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  bubble_factory()->WaitForPermissionBubble();
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Now enable the notifications killswitch.
  EnableKillSwitch(ContentSettingsType::NOTIFICATIONS);

  EXPECT_EQ("denied", content::EvalJs(web_contents, "requestNotification();"));
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
  using UiDecision = permissions::PermissionUiSelector::Decision;
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(std::optional<QuietUiReason> quiet_ui_reason,
                           std::optional<WarningReason> warning_reason) {
    GetPermissionRequestManager()->set_permission_ui_selector_for_testing(
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
      permissions::RequestType::kNotifications);
  GetPermissionRequestManager()->AddRequest(source_frame, &request_quiet);
  base::RunLoop().RunUntilIdle();

  // Add a second permission request. This ones should cause the initial
  // request to be cancelled.
  permissions::MockPermissionRequest request_loud(
      permissions::RequestType::kGeolocation);
  GetPermissionRequestManager()->AddRequest(source_frame, &request_loud);
  base::RunLoop().RunUntilIdle();

  // The first dialog should now have been decided.
  EXPECT_TRUE(request_quiet.finished());
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Cleanup remaining request. And check that this was the last request.
  GetPermissionRequestManager()->Dismiss();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetPermissionRequestManager()->Requests().size());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerQuietUiBrowserTest,
                       PermissionPromptDisposition) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      WarningReason::kAbusiveContent);

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  permissions::MockPermissionRequest request_quiet(
      permissions::RequestType::kNotifications);
  GetPermissionRequestManager()->AddRequest(web_contents->GetPrimaryMainFrame(),
                                            &request_quiet);

  bubble_factory()->WaitForPermissionBubble();
  auto* manager = GetPermissionRequestManager();

  std::optional<permissions::PermissionPromptDisposition> disposition =
      manager->current_request_prompt_disposition_for_testing();
  auto disposition_from_prompt_bubble =
      manager->view_for_testing()->GetPromptDisposition();

  manager->Dismiss();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(disposition.has_value());
  EXPECT_EQ(disposition.value(), disposition_from_prompt_bubble);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerQuietUiBrowserTest,
                       PermissionPromptDispositionHidden) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      WarningReason::kAbusiveContent);

  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/empty.html"), 1);

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  permissions::MockPermissionRequest request_quiet(
      permissions::RequestType::kNotifications);
  GetPermissionRequestManager()->AddRequest(web_contents->GetPrimaryMainFrame(),
                                            &request_quiet);

  bubble_factory()->WaitForPermissionBubble();
  auto* manager = GetPermissionRequestManager();
  auto disposition_from_prompt_bubble =
      manager->view_for_testing()->GetPromptDisposition();

  // There will be no instance of PermissionPromptImpl after a new tab is opened
  // and existing tab marked as HIDDEN.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/empty.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  std::optional<permissions::PermissionPromptDisposition> disposition =
      manager->current_request_prompt_disposition_for_testing();

  EXPECT_TRUE(disposition.has_value());
  EXPECT_EQ(disposition.value(), disposition_from_prompt_bubble);

  //  DCHECK failure if Closing executed on HIDDEN PermissionRequestManager.
  browser()->tab_strip_model()->ActivateTabAt(0);
  manager->Dismiss();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerQuietUiBrowserTest,
                       ConsoleMessages) {
  const struct {
    std::optional<QuietUiReason> simulated_quiet_ui_reason;
    std::optional<WarningReason> simulated_warning_reason;
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
        permissions::RequestType::kNotifications);
    GetPermissionRequestManager()->AddRequest(
        web_contents->GetPrimaryMainFrame(), &request_quiet);

    bubble_factory()->WaitForPermissionBubble();
    GetPermissionRequestManager()->Dismiss();
    base::RunLoop().RunUntilIdle();

    if (!test.expected_message) {
      web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          kCounterVerificationPattern);
    }

    ASSERT_TRUE(console_observer.Wait());

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
      permissions::RequestType::kClipboard);
  GetPermissionRequestManager()->AddRequest(source_frame, &request1);
  base::RunLoop().RunUntilIdle();

  permissions::MockPermissionRequest request2(
      permissions::RequestType::kMicStream);
  GetPermissionRequestManager()->AddRequest(source_frame, &request2);
  base::RunLoop().RunUntilIdle();

  // Both requests are still pending (though only one is active).
  EXPECT_FALSE(request1.finished());
  EXPECT_FALSE(request2.finished());
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Close first request.
  GetPermissionRequestManager()->Dismiss();
  base::RunLoop().RunUntilIdle();

  if (permissions::PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_FALSE(request1.finished());
    EXPECT_TRUE(request2.finished());
  } else {
    EXPECT_TRUE(request1.finished());
    EXPECT_FALSE(request2.finished());
  }
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Close second request. No more requests pending
  GetPermissionRequestManager()->Dismiss();
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(GetActiveMainFrame());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  permissions::MockPermissionRequest req(
      permissions::RequestType::kNotifications);
  GetPermissionRequestManager()->AddRequest(rfh_a.get(), &req);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, bubble_factory()->show_count());
  EXPECT_EQ(0, bubble_factory()->TotalRequestCount());
  // Page gets evicted if bubble would have been shown.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerWithBackForwardCacheBrowserTest,
                       RequestsForPagesInCacheNotGrouped) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(GetActiveMainFrame());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  content::RenderFrameHostWrapper rfh_b(GetActiveMainFrame());

  // Mic, camera, and pan/tilt/zoom requests are grouped if they come from the
  // same origin. Make sure this will not include requests from a cached
  // frame. Note pages will not be cached when navigating within the same
  // origin, so we have different urls in the navigations above but use the
  // same (default) url for the MockPermissionRequest here.
  permissions::MockPermissionRequest req_a_1(
      permissions::RequestType::kCameraPanTiltZoom,
      permissions::PermissionRequestGestureType::GESTURE);
  permissions::MockPermissionRequest req_b_1(
      permissions::RequestType::kCameraStream,
      permissions::PermissionRequestGestureType::GESTURE);
  permissions::MockPermissionRequest req_b_2(
      permissions::RequestType::kMicStream,
      permissions::PermissionRequestGestureType::GESTURE);
  GetPermissionRequestManager()->AddRequest(rfh_a.get(),
                                            &req_a_1);  // Should be skipped
  GetPermissionRequestManager()->AddRequest(rfh_b.get(), &req_b_1);
  GetPermissionRequestManager()->AddRequest(rfh_b.get(), &req_b_2);

  bubble_factory()->WaitForPermissionBubble();

  // One bubble with the two grouped requests and not the skipped one.
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory()->TotalRequestCount());
  EXPECT_TRUE(req_a_1.cancelled());

  // Page gets evicted if bubble would have been shown.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Cleanup before we delete the requests.
  GetPermissionRequestManager()->Dismiss();
}

class PermissionRequestManagerOneTimePermissionBrowserTest
    : public PermissionRequestManagerBrowserTest {
 public:
  PermissionRequestManagerOneTimePermissionBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        permissions::features::kOneTimePermission);
    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(0, 0);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerOneTimePermissionBrowserTest,
                       RequestForPermission) {
  const char kQueryCurrentPosition[] = R"(
        new Promise(resolve => {
          navigator.geolocation.getCurrentPosition(
            _ => resolve('success'),
            _ => resolve('failure'));
        });
      )";
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/title1.html"), 1);
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ONCE);

  // Request 'geolocation' permission.
  std::string result =
      content::EvalJs(GetActiveMainFrame(), kQueryCurrentPosition)
          .ExtractString();
  EXPECT_EQ("success", result);
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Request 'geolocation' permission. There should not be a 2nd prompt.
  result = content::EvalJs(GetActiveMainFrame(), kQueryCurrentPosition)
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
  result = content::EvalJs(GetActiveMainFrame(), kQueryCurrentPosition)
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
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);

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
  result = content::EvalJs(GetActiveMainFrame(), kQueryCurrentPosition)
               .ExtractString();
  EXPECT_EQ("success", result);

  EXPECT_EQ(1, third_tab_bubble_factory.get()->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerWithPrerenderingTest,
                       RequestForPermission) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/empty.html");
  GURL prerender_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);
  ASSERT_EQ(GetActiveMainFrame()->GetLastCommittedURL(), initial_url);

  prerender_test_helper().AddPrerender(prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_test_helper().GetHostForUrl(prerender_url);
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);

  content::RenderFrameDeletedObserver deleted_observer(prerender_frame);
  permissions::MockPermissionRequest request(
      permissions::RequestType::kNotifications);
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  GetPermissionRequestManager()->AddRequest(prerender_frame, &request);

  deleted_observer.WaitUntilDeleted();

  // Permission request should be denied and prerender that sent the request
  // should be discarded.
  EXPECT_TRUE(request.cancelled());
  EXPECT_TRUE(deleted_observer.deleted());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerWithPrerenderingTest,
                       DuplicateRequestForPermission) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/empty.html");
  GURL prerender_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);
  ASSERT_EQ(GetActiveMainFrame()->GetLastCommittedURL(), initial_url);

  prerender_test_helper().AddPrerender(prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_test_helper().GetHostForUrl(prerender_url);
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);

  content::RenderFrameDeletedObserver deleted_observer(prerender_frame);
  permissions::MockPermissionRequest request_1(
      permissions::RequestType::kNotifications);
  auto request_2 = request_1.CreateDuplicateRequest();
  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  GetPermissionRequestManager()->AddRequest(GetActiveMainFrame(), &request_1);
  GetPermissionRequestManager()->AddRequest(prerender_frame, request_2.get());

  base::RunLoop().RunUntilIdle();

  // Permission request from main frame should be granted, similar request
  // from prerender should be denied.
  EXPECT_TRUE(request_1.granted());
  EXPECT_TRUE(request_2->cancelled());
  EXPECT_TRUE(deleted_observer.deleted());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerWithPrerenderingTest,
                       PrerenderLoadsWhileRequestsPending) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/empty.html");
  GURL prerender_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL next_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);
  ASSERT_EQ(GetActiveMainFrame()->GetLastCommittedURL(), initial_url);

  permissions::MockPermissionRequest request_1(
      permissions::RequestType::kNotifications);
  permissions::MockPermissionRequest request_2(
      permissions::RequestType::kGeolocation);
  GetPermissionRequestManager()->AddRequest(GetActiveMainFrame(), &request_1);
  GetPermissionRequestManager()->AddRequest(GetActiveMainFrame(), &request_2);

  prerender_test_helper().AddPrerender(prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_test_helper().GetHostForUrl(prerender_url);
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);

  // Prerender's navigation should not cancel pending primary main frame
  // permission requests.
  EXPECT_FALSE(request_1.cancelled());
  EXPECT_FALSE(request_2.cancelled());

  // Navigate primary main frame.
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), next_url), nullptr);

  // Primary main frame navigation should cancel pending permission requests.
  EXPECT_TRUE(request_1.cancelled());
  EXPECT_TRUE(request_2.cancelled());
}

class PermissionRequestManagerWithFencedFrameTest
    : public PermissionRequestManagerBrowserTest {
 public:
  PermissionRequestManagerWithFencedFrameTest() = default;
  ~PermissionRequestManagerWithFencedFrameTest() override = default;

  PermissionRequestManagerWithFencedFrameTest(
      const PermissionRequestManagerWithFencedFrameTest&) = delete;
  PermissionRequestManagerWithFencedFrameTest& operator=(
      const PermissionRequestManagerWithFencedFrameTest&) = delete;

 protected:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestManagerWithFencedFrameTest,
                       GetCurrentPosition) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Load a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  const char kQueryPermission[] = R"(
      (async () => {
        const status = await navigator.permissions.query({name: 'geolocation'});
        return status.state;
      })();
    )";

  // The result of query 'geolocation' permission in the fenced frame should
  // be 'denied'.
  EXPECT_EQ("denied", content::EvalJs(fenced_frame_host, kQueryPermission));

  const char kQueryCurrentPosition[] = R"(
      (async () => {
        return await new Promise(resolve => {
          navigator.geolocation.getCurrentPosition(
              () => resolve('granted'), () => resolve('denied'));
        });
      })();
    )";

  bubble_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ONCE);

  // The getCurrentPosition() call in the fenced frame should be denied.
  EXPECT_EQ("denied",
            content::EvalJs(fenced_frame_host, kQueryCurrentPosition));
  // The permission prompt should not be shown.
  EXPECT_EQ(0, bubble_factory()->TotalRequestCount());
}

// Tests that the permission request for a fenced frame is blocked
// when the permission is requested thru PermissionControllerDelegate.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerWithFencedFrameTest,
                       RequestPermissionThruDelegate) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server.Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  GURL initial_url = https_server.GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a fenced frame.
  GURL fenced_frame_url = https_server.GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  // The permission request is denied because it's from the fenced frame.
  const char kExpectedConsolePattern[] =
      "*blocked because it was requested inside a fenced frame*";
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetFilter(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         const content::WebContentsConsoleObserver::Message& message) {
        return message.source_frame == render_frame_host;
      },
      fenced_frame_host->GetOutermostMainFrame()));
  console_observer.SetPattern(kExpectedConsolePattern);

  base::MockOnceCallback<void(blink::mojom::PermissionStatus)> callback;
  EXPECT_CALL(callback, Run(blink::mojom::PermissionStatus::DENIED));

  content::PermissionController* permission_controller =
      browser()->profile()->GetPermissionController();
  permission_controller->RequestPermissionFromCurrentDocument(
      fenced_frame_host,
      content::PermissionRequestDescription(blink::PermissionType::SENSORS,
                                            /* user_gesture = */ true),
      callback.Get());
  ASSERT_TRUE(console_observer.Wait());
  ASSERT_EQ(1u, console_observer.messages().size());
}

IN_PROC_BROWSER_TEST_F(
    PermissionRequestManagerWithBackForwardCacheUnblockBrowserTest,
    PendingRequestsDoNotDisableBackForwardCache) {
  content::BackForwardCacheDisabledTester back_forward_cache_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-after-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();
  content::RenderFrameHostWrapper rfh_a(GetActiveMainFrame());
  content::RenderFrameHost* main_frame = GetActiveMainFrame();
  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int main_frame_routing_id = main_frame->GetRoutingID();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("b.com", "/title1.html"), 1);
  // A goes into bfcache.
  EXPECT_FALSE(back_forward_cache_tester.IsDisabledForFrameWithReason(
      main_frame_process_id, main_frame_routing_id,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kPermissionRequestManager)));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  EXPECT_FALSE(bubble_factory()->is_visible());
}

IN_PROC_BROWSER_TEST_F(
    PermissionRequestManagerWithBackForwardCacheUnblockBrowserTest,
    PermissionRequestsCancelledInBackForwardCache) {
  content::BackForwardCacheDisabledTester back_forward_cache_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("/title1.html"), 1);
  // Create a geolocation permission request.
  permissions::MockPermissionRequest request_1(
      permissions::RequestType::kGeolocation);
  GetPermissionRequestManager()->AddRequest(GetActiveMainFrame(), &request_1);
  bubble_factory()->WaitForPermissionBubble();

  content::RenderFrameHostWrapper rfh_a(GetActiveMainFrame());
  content::RenderFrameHost* main_frame = GetActiveMainFrame();
  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int main_frame_routing_id = main_frame->GetRoutingID();
  // Request is not cancelled.
  EXPECT_FALSE(request_1.cancelled());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("b.com", "/title1.html"), 1);
  // A goes into bfcache.
  EXPECT_FALSE(back_forward_cache_tester.IsDisabledForFrameWithReason(
      main_frame_process_id, main_frame_routing_id,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kPermissionRequestManager)));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  // Request should be cancelled.
  EXPECT_TRUE(request_1.cancelled());
  EXPECT_FALSE(bubble_factory()->is_visible());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(request_1.cancelled());
}

}  // anonymous namespace
