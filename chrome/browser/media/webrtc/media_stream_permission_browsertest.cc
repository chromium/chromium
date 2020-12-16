// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

// MediaStreamPermissionTest ---------------------------------------------------

class MediaStreamPermissionTest : public WebRtcTestBase {
 public:
  MediaStreamPermissionTest() {}
  ~MediaStreamPermissionTest() override {}

  // InProcessBrowserTest:
  void SetUp() override {
    WebRtcTestBase::SetUp();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // This test expects to run with fake devices but real UI.
    EXPECT_TRUE(
        command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream));
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream))
        << "Since this test tests the UI we want the real UI!";
  }

 protected:
  content::WebContents* LoadTestPageInTab() {
    return LoadTestPageInBrowser(browser());
  }

  content::WebContents* LoadTestPageInIncognitoTab() {
    return LoadTestPageInBrowser(CreateIncognitoBrowser());
  }

  // Returns the URL of the main test page.
  GURL test_page_url() const {
    const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";
    return embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage);
  }

 private:
  content::WebContents* LoadTestPageInBrowser(Browser* browser) {
    if (!embedded_test_server()->Started()) {
      EXPECT_TRUE(embedded_test_server()->Start());
    }

    // Uses the default server.
    GURL url = test_page_url();

    EXPECT_TRUE(network::IsUrlPotentiallyTrustworthy(url));

    ui_test_utils::NavigateToURL(browser, url);
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  DISALLOW_COPY_AND_ASSIGN(MediaStreamPermissionTest);
};

// Actual tests ---------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest, TestAllowingUserMedia) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_TRUE(GetUserMediaAndAccept(tab_contents));
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest, TestDenyingUserMedia) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  GetUserMediaAndDeny(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest, TestDismissingRequest) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  GetUserMediaAndDismiss(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestDenyingUserMediaIncognito) {
  content::WebContents* tab_contents = LoadTestPageInIncognitoTab();
  GetUserMediaAndDeny(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestSecureOriginDenyIsSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_TRUE(network::IsUrlPotentiallyTrustworthy(
      tab_contents->GetLastCommittedURL()));

  GetUserMediaAndDeny(tab_contents);
  GetUserMediaAndExpectAutoDenyWithoutPrompt(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestSecureOriginAcceptIsSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();
  EXPECT_TRUE(network::IsUrlPotentiallyTrustworthy(
      tab_contents->GetLastCommittedURL()));

  EXPECT_TRUE(GetUserMediaAndAccept(tab_contents));
  GetUserMediaAndExpectAutoAcceptWithoutPrompt(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest, TestDismissIsNotSticky) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaAndDismiss(tab_contents);
  GetUserMediaAndDismiss(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       TestDenyingThenClearingStickyException) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaAndDeny(tab_contents);
  GetUserMediaAndExpectAutoDenyWithoutPrompt(tab_contents);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  settings_map->ClearSettingsForOneType(ContentSettingsType::MEDIASTREAM_MIC);
  settings_map->ClearSettingsForOneType(
      ContentSettingsType::MEDIASTREAM_CAMERA);

  GetUserMediaAndDeny(tab_contents);
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       DenyingMicDoesNotCauseStickyDenyForCameras) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                             kAudioOnlyCallConstraints);
  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAccept(
      tab_contents, kVideoOnlyCallConstraints));
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       DenyingCameraDoesNotCauseStickyDenyForMics) {
  content::WebContents* tab_contents = LoadTestPageInTab();

  GetUserMediaWithSpecificConstraintsAndDeny(tab_contents,
                                             kVideoOnlyCallConstraints);
  EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAccept(
      tab_contents, kAudioOnlyCallConstraints));
}

IN_PROC_BROWSER_TEST_F(MediaStreamPermissionTest,
                       DenyingPermissionStopsStreamWhenRelevant) {
  struct {
    std::string constraints;
    ContentSettingsType setting_to_clear;
    bool should_video_stop;
  } kTests[] = {
      {kAudioVideoCallConstraints, ContentSettingsType::MEDIASTREAM_CAMERA,
       true},
      {kAudioVideoCallConstraints, ContentSettingsType::MEDIASTREAM_MIC, true},
      {kVideoOnlyCallConstraints, ContentSettingsType::MEDIASTREAM_CAMERA,
       true},
      {kVideoOnlyCallConstraints, ContentSettingsType::MEDIASTREAM_MIC, false},
  };

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  for (const auto& kTest : kTests) {
    content::WebContents* tab_contents = LoadTestPageInTab();

    EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
        tab_contents, kTest.constraints));

    StartDetectingVideo(tab_contents, "local-view");
    EXPECT_TRUE(WaitForVideoToPlay(tab_contents));

    settings_map->ClearSettingsForOneType(kTest.setting_to_clear);

    // Let all the cross-thread tasks do their work.
    base::RunLoop().RunUntilIdle();

    StartDetectingVideo(tab_contents, "local-view");

    if (kTest.should_video_stop) {
      EXPECT_TRUE(WaitForVideoToStop(tab_contents));
    } else {
      EXPECT_TRUE(WaitForVideoToPlay(tab_contents));
    }

    // Clean up settings for the following tests.
    settings_map->ClearSettingsForOneType(ContentSettingsType::MEDIASTREAM_MIC);
    settings_map->ClearSettingsForOneType(
        ContentSettingsType::MEDIASTREAM_CAMERA);
  }
}
