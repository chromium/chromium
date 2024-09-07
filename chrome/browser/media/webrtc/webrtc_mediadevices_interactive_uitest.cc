// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_MAC)
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace {

const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";
const char kClearCookiesPage[] = "/clear_cookies";

const char kDeviceKindAudioInput[] = "audioinput";
const char kDeviceKindVideoInput[] = "videoinput";
const char kDeviceKindAudioOutput[] = "audiooutput";

}  // namespace

// Integration test for WebRTC enumerateDevices. It always uses fake devices.
// It needs to be a browser test (and not content browser test) to be able to
// test that labels are cleared or not depending on if access to devices has
// been granted.
class WebRtcMediaDevicesInteractiveUITest
    : public WebRtcTestBase,
      public testing::WithParamInterface<bool> {
 public:
  WebRtcMediaDevicesInteractiveUITest()
      : has_audio_output_devices_initialized_(false),
        has_audio_output_devices_(false) {
    if (IsMediaDeviceIdRandomSaltsPerStorageKeyEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kUserMediaCaptureOnFocus,
           media_device_salt::kMediaDeviceIdPartitioning,
           media_device_salt::kMediaDeviceIdRandomSaltsPerStorageKey},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kUserMediaCaptureOnFocus},
          {media_device_salt::kMediaDeviceIdPartitioning,
           media_device_salt::kMediaDeviceIdRandomSaltsPerStorageKey});
    }
  }

  bool IsMediaDeviceIdRandomSaltsPerStorageKeyEnabled() { return GetParam(); }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));
  }

 protected:
  // This is used for media devices and sources.
  struct MediaDeviceInfo {
    std::string device_id;  // Domain specific device ID.
    std::string kind;
    std::string label;
    std::string group_id;
  };

  void EnumerateDevices(content::WebContents* tab,
                        std::vector<MediaDeviceInfo>* devices) {
    std::string devices_as_json = ExecuteJavascript("enumerateDevices()", tab);
    EXPECT_FALSE(devices_as_json.empty());

    ASSERT_OK_AND_ASSIGN(
        auto parsed_json,
        base::JSONReader::ReadAndReturnValueWithError(
            devices_as_json, base::JSON_ALLOW_TRAILING_COMMAS));
    ASSERT_TRUE(parsed_json.is_list());
    ASSERT_FALSE(parsed_json.GetList().empty());
    bool found_audio_input = false;
    bool found_video_input = false;

    for (const auto& value : parsed_json.GetList()) {
      const base::Value::Dict* dict = value.GetIfDict();
      ASSERT_TRUE(dict);
      MediaDeviceInfo device;
      ASSERT_TRUE(dict->FindString("deviceId"));
      device.device_id = *dict->FindString("deviceId");

      ASSERT_TRUE(dict->FindString("kind"));
      device.kind = *dict->FindString("kind");

      ASSERT_TRUE(dict->FindString("label"));
      device.label = *dict->FindString("label");

      ASSERT_TRUE(dict->FindString("groupId"));
      device.group_id = *dict->FindString("groupId");

      // Should be HMAC SHA256.
      if (!media::AudioDeviceDescription::IsDefaultDevice(device.device_id) &&
          !(device.device_id ==
            media::AudioDeviceDescription::kCommunicationsDeviceId)) {
        EXPECT_EQ(64ul, device.device_id.length());
        EXPECT_TRUE(
            base::ContainsOnlyChars(device.device_id, "0123456789abcdef"));
      }

      EXPECT_TRUE(device.kind == kDeviceKindAudioInput ||
                  device.kind == kDeviceKindVideoInput ||
                  device.kind == kDeviceKindAudioOutput);
      if (device.kind == kDeviceKindAudioInput) {
        found_audio_input = true;
      } else if (device.kind == kDeviceKindVideoInput) {
        found_video_input = true;
      }

      devices->push_back(device);
    }

    EXPECT_TRUE(found_audio_input);
    EXPECT_TRUE(found_video_input);
  }

  static void CheckEnumerationsAreDifferent(
      const std::vector<MediaDeviceInfo>& devices,
      const std::vector<MediaDeviceInfo>& devices2) {
    for (auto& device : devices) {
      bool found = base::Contains(devices2, device.device_id,
                                  &MediaDeviceInfo::device_id);
      if (device.device_id == media::AudioDeviceDescription::kDefaultDeviceId ||
          device.device_id ==
              media::AudioDeviceDescription::kCommunicationsDeviceId) {
        EXPECT_TRUE(found);
      } else {
        EXPECT_FALSE(found);
      }

      EXPECT_FALSE(base::Contains(devices2, device.group_id,
                                  &MediaDeviceInfo::group_id));
    }
  }

  bool has_audio_output_devices_initialized_;
  bool has_audio_output_devices_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       EnumerateDevicesWithoutAccess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  // Label, deviceId and groupId should be empty if access has not been allowed.
  for (const auto& device_info : devices) {
    EXPECT_TRUE(device_info.label.empty());
    EXPECT_TRUE(device_info.device_id.empty());
    EXPECT_TRUE(device_info.group_id.empty());
  }
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       EnumerateDevicesWithAccess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(GetUserMediaAndAccept(tab));

  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  // Labels, deviceId and groupId should be non-empty if access has been
  // allowed.
  for (const auto& device_info : devices) {
    EXPECT_TRUE(!device_info.label.empty());
    EXPECT_TRUE(!device_info.device_id.empty());
    EXPECT_TRUE(!device_info.group_id.empty());
  }
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       GetUserMediaOnUnFocusedTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, url,
                                     ui::PAGE_TRANSITION_LINK, true));

  content::WebContents* focused_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::WebContents* unfocused_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_TRUE(GetUserMediaAndAccept(focused_tab));
  GetUserMediaReturnsFalseIfWaitIsTooLong(unfocused_tab,
                                          kAudioVideoCallConstraints);
}

// Flakes on Linux TSan Tests; crbug.com/1396123.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_GetUserMediaTabRegainsFocus DISABLED_GetUserMediaTabRegainsFocus
#else
#define MAYBE_GetUserMediaTabRegainsFocus GetUserMediaTabRegainsFocus
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       MAYBE_GetUserMediaTabRegainsFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, url,
                                     ui::PAGE_TRANSITION_LINK, true));

  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  GetUserMediaReturnsFalseIfWaitIsTooLong(tab, kAudioVideoCallConstraints);
  // |tab| gains focus.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
      tab, kAudioVideoCallConstraints));
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       DeviceIdSameGroupIdDiffersAcrossTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetUserMediaAndAccept(tab1));
  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab1, &devices);

  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
      tab2, kAudioVideoCallConstraints));
  std::vector<MediaDeviceInfo> devices2;
  EnumerateDevices(tab2, &devices2);

  EXPECT_NE(tab1, tab2);
  EXPECT_EQ(devices.size(), devices2.size());
  for (auto& device : devices) {
    EXPECT_TRUE(base::Contains(devices2, device.device_id,
                               &MediaDeviceInfo::device_id));

    EXPECT_FALSE(
        base::Contains(devices2, device.group_id, &MediaDeviceInfo::group_id));
  }
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       DeviceIdDiffersAfterClearingCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(GetUserMediaAndAccept(tab));

  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  auto* remover = browser()->profile()->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);
  completion_observer.BlockUntilCompletion();

  std::vector<MediaDeviceInfo> devices2;
  EnumerateDevices(tab, &devices2);

  EXPECT_EQ(devices.size(), devices2.size());
  CheckEnumerationsAreDifferent(devices, devices2);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       DeviceIdDiffersAcrossTabsWithCookiesDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled,
                                               true);
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(GetUserMediaAndAccept(tab1));

  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab1, &devices);

  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
      tab2, kAudioVideoCallConstraints));
  std::vector<MediaDeviceInfo> devices2;
  EnumerateDevices(tab2, &devices2);

  EXPECT_NE(tab1, tab2);
  EXPECT_EQ(devices.size(), devices2.size());
  CheckEnumerationsAreDifferent(devices, devices2);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       DeviceIdDiffersSameTabAfterReloadWithCookiesDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled,
                                               true);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(GetUserMediaAndAccept(tab));
  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  tab = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
      tab, kAudioVideoCallConstraints));
  std::vector<MediaDeviceInfo> devices2;
  EnumerateDevices(tab, &devices2);

  EXPECT_EQ(devices.size(), devices2.size());
  CheckEnumerationsAreDifferent(devices, devices2);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       DeviceIdDiffersAfterClearSiteDataHeader) {
  if (!IsMediaDeviceIdRandomSaltsPerStorageKeyEnabled()) {
    return;
  }
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().path() == kClearCookiesPage) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->AddCustomHeader("Clear-Site-Data", "\"cookies\"");
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html");
          response->set_content(std::string());
          return response;
        }

        // Use the default handler for other requests.
        return nullptr;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage)));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(GetUserMediaAndAccept(tab));
  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kClearCookiesPage)));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage)));

  std::vector<MediaDeviceInfo> devices2;
  EnumerateDevices(tab, &devices2);
  EXPECT_EQ(devices.size(), devices2.size());
  CheckEnumerationsAreDifferent(devices, devices2);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       PRE_SaltsDeletedForSessionOnlyCookies) {
  if (!IsMediaDeviceIdRandomSaltsPerStorageKeyEnabled()) {
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  media_device_salt::MediaDeviceSaltService* salt_service =
      MediaDeviceSaltServiceFactory::GetForBrowserContext(browser()->profile());
  base::test::TestFuture<std::vector<blink::StorageKey>> keys_future;
  salt_service->GetAllStorageKeys(keys_future.GetCallback());
  EXPECT_FALSE(keys_future.Get().empty());
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       SaltsDeletedForSessionOnlyCookies) {
  if (!IsMediaDeviceIdRandomSaltsPerStorageKeyEnabled()) {
    return;
  }
  media_device_salt::MediaDeviceSaltService* salt_service =
      MediaDeviceSaltServiceFactory::GetForBrowserContext(browser()->profile());
  base::test::TestFuture<std::vector<blink::StorageKey>> keys_future;
  salt_service->GetAllStorageKeys(keys_future.GetCallback());
  EXPECT_TRUE(keys_future.Get().empty());
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesInteractiveUITest,
                       NoPersistentSaltStoredWithCookiesDisabled) {
  if (!IsMediaDeviceIdRandomSaltsPerStorageKeyEnabled()) {
    return;
  }
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled,
                                               true);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  media_device_salt::MediaDeviceSaltService* salt_service =
      MediaDeviceSaltServiceFactory::GetForBrowserContext(browser()->profile());
  base::test::TestFuture<std::vector<blink::StorageKey>> keys_future;
  salt_service->GetAllStorageKeys(keys_future.GetCallback());
  EXPECT_TRUE(keys_future.Get().empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcMediaDevicesInteractiveUITest,
                         testing::Bool());

class WebRtcMediaDevicesPrerenderingBrowserTest
    : public WebRtcMediaDevicesInteractiveUITest {
 public:
  WebRtcMediaDevicesPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &WebRtcMediaDevicesPrerenderingBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~WebRtcMediaDevicesPrerenderingBrowserTest() override = default;

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_P(WebRtcMediaDevicesPrerenderingBrowserTest,
                       EnumerateDevicesInPrerendering) {
#if BUILDFLAG(IS_MAC)
  // Test will fail if the window it's running in contains the mouse pointer.
  // Here we warp the cursor, hopefully, out of the window.
  CGWarpMouseCursorPosition({0, 0});
#endif
  ASSERT_TRUE(embedded_test_server()->Start());

  // Loads a simple page as a primary page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage);
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  base::RunLoop run_loop;
  permissions::PermissionRequestObserver observer(web_contents());
  prerender_rfh->ExecuteJavaScriptForTests(
      u"doGetUserMedia({audio: true, video: true});", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);

  // The prerendering page should not show a permission request's bubble UI.
  EXPECT_FALSE(observer.request_shown());

  // Activates the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  observer.Wait();

  // Makes sure that the page is activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());

  // The prerendered page should show the permission request's bubble UI.
  EXPECT_TRUE(observer.request_shown());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcMediaDevicesPrerenderingBrowserTest,
                         testing::Bool());
