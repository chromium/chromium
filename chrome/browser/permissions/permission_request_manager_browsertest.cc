// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_permission_request.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/permissions/mock_permission_request.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_features.h"
#include "chrome/browser/permissions/permission_request_impl.h"
#include "chrome/browser/permissions/permission_request_manager_test_api.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const char* kPermissionsKillSwitchFieldStudy =
    PermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* kPermissionsKillSwitchBlockedValue =
    PermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";

class PermissionRequestManagerBrowserTest : public InProcessBrowserTest {
 public:
  PermissionRequestManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kBlockRepeatedNotificationPermissionPrompts);
  }

  ~PermissionRequestManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PermissionRequestManager* manager = GetPermissionRequestManager();
    mock_permission_prompt_factory_.reset(
        new MockPermissionPromptFactory(manager));

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    mock_permission_prompt_factory_.reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable fake devices so we can test getUserMedia() on devices without
    // physical media devices.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

  PermissionRequestManager* GetPermissionRequestManager() {
    return PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  MockPermissionPromptFactory* bubble_factory() {
    return mock_permission_prompt_factory_.get();
  }

  void EnableKillSwitch(ContentSettingsType content_settings_type) {
    std::map<std::string, std::string> params;
    params[PermissionUtil::GetPermissionString(content_settings_type)] =
        kPermissionsKillSwitchBlockedValue;
    variations::AssociateVariationParams(
        kPermissionsKillSwitchFieldStudy, kPermissionsKillSwitchTestGroup,
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
        PermissionRequestManager::AutoResponseType::DENY_ALL);

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
        PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockPermissionPromptFactory> mock_permission_prompt_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestManagerBrowserTest);
};

// Harness for testing permissions dialogs invoked by PermissionRequestManager.
// Uses a "real" PermissionPromptFactory rather than a mock.
class PermissionDialogTest
    : public SupportsTestDialog<PermissionRequestManagerBrowserTest> {
 public:
  PermissionDialogTest() {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Skip super: It will install a mock permission UI factory, but for this
    // test we want to show "real" UI.
    ui_test_utils::NavigateToURL(browser(), GetUrl());
  }

  GURL GetUrl() { return GURL("https://example.com"); }

  PermissionRequest* MakeRegisterProtocolHandlerRequest();
  PermissionRequest* MakePermissionRequest(ContentSettingsType permission);

  // TestBrowserDialog:
  void ShowUi(const std::string& name) override;

  // Holds requests that do not delete themselves.
  std::vector<std::unique_ptr<PermissionRequest>> owned_requests_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionDialogTest);
};

PermissionRequest* PermissionDialogTest::MakeRegisterProtocolHandlerRequest() {
  std::string protocol = "mailto";
  bool user_gesture = true;
  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, GetUrl());
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  // Deleted in RegisterProtocolHandlerPermissionRequest::RequestFinished().
  return new RegisterProtocolHandlerPermissionRequest(registry, handler,
                                                      GetUrl(), user_gesture);
}

PermissionRequest* PermissionDialogTest::MakePermissionRequest(
    ContentSettingsType permission) {
  bool user_gesture = true;
  auto decided = [](ContentSetting) {};
  auto cleanup = [] {};  // Leave cleanup to test harness destructor.
  owned_requests_.push_back(std::make_unique<PermissionRequestImpl>(
      GetUrl(), permission, user_gesture, base::Bind(decided),
      base::Bind(cleanup)));
  return owned_requests_.back().get();
}

void PermissionDialogTest::ShowUi(const std::string& name) {
  constexpr const char* kMultipleName = "multiple";
  constexpr struct {
    const char* name;
    ContentSettingsType type;
  } kNameToType[] = {
      {"flash", ContentSettingsType::PLUGINS},
      {"geolocation", ContentSettingsType::GEOLOCATION},
      {"protected_media", ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER},
      {"notifications", ContentSettingsType::NOTIFICATIONS},
      {"mic", ContentSettingsType::MEDIASTREAM_MIC},
      {"camera", ContentSettingsType::MEDIASTREAM_CAMERA},
      {"protocol_handlers", ContentSettingsType::PROTOCOL_HANDLERS},
      {"midi", ContentSettingsType::MIDI_SYSEX},
      {kMultipleName, ContentSettingsType::DEFAULT}};
  const auto* it = std::begin(kNameToType);
  for (; it != std::end(kNameToType); ++it) {
    if (name == it->name)
      break;
  }
  if (it == std::end(kNameToType)) {
    ADD_FAILURE() << "Unknown: " << name;
    return;
  }
  PermissionRequestManager* manager = GetPermissionRequestManager();
  switch (it->type) {
    case ContentSettingsType::PROTOCOL_HANDLERS:
      manager->AddRequest(MakeRegisterProtocolHandlerRequest());
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      // TODO(tapted): Prompt for downloading multiple files.
      break;
    case ContentSettingsType::DURABLE_STORAGE:
      // TODO(tapted): Prompt for quota request.
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::MIDI_SYSEX:
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::GEOLOCATION:
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:  // ChromeOS only.
    case ContentSettingsType::PPAPI_BROKER:
    case ContentSettingsType::PLUGINS:  // Flash.
      manager->AddRequest(MakePermissionRequest(it->type));
      break;
    case ContentSettingsType::DEFAULT:
      // Permissions to request for a "multiple" request. Only mic/camera
      // requests are grouped together.
      EXPECT_EQ(kMultipleName, name);
      manager->AddRequest(
          MakePermissionRequest(ContentSettingsType::MEDIASTREAM_MIC));
      manager->AddRequest(
          MakePermissionRequest(ContentSettingsType::MEDIASTREAM_CAMERA));

      break;
    default:
      ADD_FAILURE() << "Not a permission type, or one that doesn't prompt.";
      return;
  }
  base::RunLoop().RunUntilIdle();
}

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
#if defined(OS_WIN)
#define MAYBE_NavTwice DISABLED_NavTwice
#else
#define MAYBE_NavTwice NavTwice
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest, MAYBE_NavTwice) {
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
#if defined(OS_WIN)
#define MAYBE_NavTwiceWithHash DISABLED_NavTwiceWithHash
#else
#define MAYBE_NavTwiceWithHash NavTwiceWithHash
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       MAYBE_NavTwiceWithHash) {
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
      browser(),
      embedded_test_server()->GetURL("/empty.html"),
      1);

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/empty.html#0"),
      1);

  // Request 'geolocation' permission.
  ExecuteScriptAndGetValue(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // SetUp() only creates a mock prompt factory for the first tab.
  MockPermissionPromptFactory* bubble_factory_0 = bubble_factory();
  std::unique_ptr<MockPermissionPromptFactory> bubble_factory_1(
      std::make_unique<MockPermissionPromptFactory>(
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

// Regularly timing out in Linux Debug Builds. https://crbug.com/931657
#if defined(OS_LINUX)
#define MAYBE_BackgroundTabNavigation DISABLED_BackgroundTabNavigation
#else
#define MAYBE_BackgroundTabNavigation BackgroundTabNavigation
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       MAYBE_BackgroundTabNavigation) {
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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

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
      PermissionRequestManager::AutoResponseType::DENY_ALL);

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
      PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
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

  content::RenderFrameHost* main_frame =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int main_frame_routing_id = main_frame->GetRoutingID();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("b.com", "/title1.html"), 1);
  EXPECT_TRUE(back_forward_cache_tester.IsDisabledForFrameWithReason(
      main_frame_process_id, main_frame_routing_id,
      "PermissionRequestManager"));
}

// For browser tests feature overrides need to be enabled during SetUp.
// Have a separate fixture based on which UI flavor needs to be enabled.
class PermissionRequestManagerBrowserTest_StaticIcon
    : public PermissionRequestManagerBrowserTest {
 public:
  void SetUp() override {
    base::FieldTrialParams params;
    params[kQuietNotificationPromptsUIFlavorParameterName] =
        kQuietNotificationPromptsStaticIcon;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kQuietNotificationPrompts, params);

    PermissionRequestManagerBrowserTest::SetUp();
  }
};

class PermissionRequestManagerBrowserTest_AnimatedIcon
    : public PermissionRequestManagerBrowserTest {
 public:
  void SetUp() override {
    base::FieldTrialParams params;
    params[kQuietNotificationPromptsUIFlavorParameterName] =
        kQuietNotificationPromptsAnimatedIcon;
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kQuietNotificationPrompts, params);

    PermissionRequestManagerBrowserTest::SetUp();
  }
};

// Re-enable when 1016233 is fixed.
// Quiet permission requests are cancelled when a new request is made.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest_StaticIcon,
                       DISABLED_QuietPendingRequestsKilledOnNewRequest) {
  // First add a quiet permission request. Ensure that this request is decided
  // by the end of this test.
  MockPermissionRequest request_quiet(
      "quiet", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(&request_quiet);
  base::RunLoop().RunUntilIdle();

  // Add a second permission request. This ones should cause the initial
  // request to be cancelled.
  MockPermissionRequest request_loud(
      "loud", PermissionRequestType::PERMISSION_GEOLOCATION,
      PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(&request_loud);
  base::RunLoop().RunUntilIdle();

  // The first dialog should now have been decided.
  EXPECT_TRUE(request_quiet.finished());
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Cleanup remaining request. And check that this was the last request.
  GetPermissionRequestManager()->Closing();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetPermissionRequestManager()->Requests().size());
}

// Re-enable when 1016233 is fixed.
// Quiet permission requests are cancelled when a new request is made.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest_AnimatedIcon,
                       DISABLED_QuietPendingRequestsKilledOnNewRequest) {
  // First add a quiet permission request. Ensure that this request is decided
  // by the end of this test.
  MockPermissionRequest request_quiet(
      "quiet", PermissionRequestType::PERMISSION_NOTIFICATIONS,
      PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(&request_quiet);
  base::RunLoop().RunUntilIdle();

  // Add a second permission request. This ones should cause the initial
  // request to be cancelled.
  MockPermissionRequest request_loud(
      "loud", PermissionRequestType::PERMISSION_GEOLOCATION,
      PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(&request_loud);
  base::RunLoop().RunUntilIdle();

  // The first dialog should now have been decided.
  EXPECT_TRUE(request_quiet.finished());
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Cleanup remaining request. And check that this was the last request.
  GetPermissionRequestManager()->Closing();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetPermissionRequestManager()->Requests().size());
}

// Two loud requests are simply queued one after another.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       LoudPendingRequestsQueued) {
  MockPermissionRequest request1(
      "request1", PermissionRequestType::PERMISSION_CLIPBOARD_READ,
      PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(&request1);
  base::RunLoop().RunUntilIdle();

  MockPermissionRequest request2("request2",
                                 PermissionRequestType::PERMISSION_GEOLOCATION,
                                 PermissionRequestGestureType::UNKNOWN);
  GetPermissionRequestManager()->AddRequest(&request2);
  base::RunLoop().RunUntilIdle();

  // Both requests are still pending (though only one is active).
  EXPECT_FALSE(request1.finished());
  EXPECT_FALSE(request2.finished());
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Close first request.
  GetPermissionRequestManager()->Closing();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(request1.finished());
  EXPECT_FALSE(request2.finished());
  EXPECT_EQ(1u, GetPermissionRequestManager()->Requests().size());

  // Close second request. No more requests pending
  GetPermissionRequestManager()->Closing();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(request1.finished());
  EXPECT_TRUE(request2.finished());
  EXPECT_EQ(0u, GetPermissionRequestManager()->Requests().size());
}

// Test bubbles showing when tabs move between windows. Simulates a situation
// that could result in permission bubbles not being dismissed, and a problem
// referencing a temporary drag window. See http://crbug.com/754552.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, SwitchBrowserWindow) {
  ShowUi("geolocation");
  TabStripModel* strip = browser()->tab_strip_model();

  // Drag out into a dragging window. E.g. see steps in [BrowserWindowController
  // detachTabsToNewWindow:..].
  std::vector<TabStripModelDelegate::NewStripContents> contentses(1);
  contentses.back().add_types = TabStripModel::ADD_ACTIVE;
  contentses.back().web_contents = strip->DetachWebContentsAt(0);
  Browser* dragging_browser = strip->delegate()->CreateNewStripWithContents(
      std::move(contentses), gfx::Rect(100, 100, 640, 480), false);

  // Attach the tab back to the original window. E.g. See steps in
  // [BrowserWindowController moveTabViews:..].
  TabStripModel* drag_strip = dragging_browser->tab_strip_model();
  std::unique_ptr<content::WebContents> removed_contents =
      drag_strip->DetachWebContentsAt(0);
  strip->InsertWebContentsAt(0, std::move(removed_contents),
                             TabStripModel::ADD_ACTIVE);

  // Clear the request. There should be no crash.
  test::PermissionRequestManagerTestApi(GetPermissionRequestManager())
      .SimulateWebContentsDestroyed();
  owned_requests_.clear();
}

// crbug.com/989858
#if defined(OS_WIN)
#define MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest \
  DISABLED_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest
#else
#define MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest \
  ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest
#endif
// Regression test for https://crbug.com/933321.
IN_PROC_BROWSER_TEST_F(
    PermissionDialogTest,
    MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest) {
  ShowUi("geolocation");
  ASSERT_TRUE(VerifyUi());

  // Simulate a render process crash while the permission prompt is pending.
  content::RenderViewHost* render_view_host =
      browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();
  content::RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      render_process_host,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  ASSERT_TRUE(render_process_host->Shutdown(0));
  crash_observer.Wait();

  // The permission request is still pending, but the BrowserView's WebView is
  // now showing a crash overlay, so the permission prompt is hidden.
  //
  // Now close the tab. This will first detach the WebContents, causing the
  // WebView's crash overlay to be torn down, which, in turn, will temporarily
  // make the dying WebContents visible again, albeit without being attached to
  // any BrowserView.
  //
  // Wait until the WebContents, and with it, the PermissionRequestManager, is
  // gone, and make sure nothing crashes.
  content::WebContentsDestroyedWatcher web_contents_destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->tab_strip_model()->CloseAllTabs();
  web_contents_destroyed_watcher.Wait();
}

// Host wants to run flash.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeUi_flash) {
  ShowAndVerifyUi();
}

// Host wants to know your location.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

// Host wants to show notifications.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeUi_notifications) {
  ShowAndVerifyUi();
}

// Host wants to use your microphone.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeUi_mic) {
  ShowAndVerifyUi();
}

// Host wants to use your camera.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeUi_camera) {
  ShowAndVerifyUi();
}

// Host wants to open email links.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeUi_protocol_handlers) {
  ShowAndVerifyUi();
}

// Host wants to use your MIDI devices.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeUi_midi) {
  ShowAndVerifyUi();
}

// Shows a permissions bubble with multiple requests.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeUi_multiple) {
  ShowAndVerifyUi();
}

// ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER is ChromeOS only.
#if defined(OS_CHROMEOS)
#define MAYBE_InvokeUi_protected_media InvokeUi_protected_media
#else
#define MAYBE_InvokeUi_protected_media DISABLED_InvokeUi_protected_media
#endif
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, MAYBE_InvokeUi_protected_media) {
  ShowAndVerifyUi();
}

}  // anonymous namespace
