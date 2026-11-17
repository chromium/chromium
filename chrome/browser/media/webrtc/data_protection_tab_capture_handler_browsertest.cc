// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/data_protection_tab_capture_handler.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "media/mojo/mojom/display_media_information.mojom.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

namespace {

class DataProtectionTabCaptureHandlerBrowserTest : public InProcessBrowserTest {
 public:
  DataProtectionTabCaptureHandlerBrowserTest() = default;
  ~DataProtectionTabCaptureHandlerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    media_id_ = content::DesktopMediaID(
        content::DesktopMediaID::TYPE_WEB_CONTENTS, 123);
  }

  void SetScreenshotBlockedForHost(const std::string& host) {
    data_controls::SetDataControls(
        GetBrowserWindowInterface()->GetProfile()->GetPrefs(),
        {base::StringPrintf(R"(
          {
            "name":"block_screenshots",
            "rule_id":"1234",
            "sources":{"urls":["%s"]},
            "restrictions":[{"class": "SCREENSHOT", "level": "BLOCK"}]
          }
        )",
                            host.c_str())});
  }

  content::WebContents* GetActiveWebContents() {
    return GetBrowserWindowInterface()
        ->GetTabStripModel()
        ->GetActiveWebContents();
  }

  GURL GetAllowedUrl() const {
    return embedded_test_server()->GetURL("allowed.com", "/title1.html");
  }

  GURL GetBlockedUrl(const std::string& path = "/title2.html") const {
    return embedded_test_server()->GetURL("blocked.com", path);
  }

 protected:
  content::DesktopMediaID media_id_;
  base::MockRepeatingCallback<void(const content::DesktopMediaID&,
                                   blink::mojom::MediaStreamStateChange)>
      mock_callback_;
};

IN_PROC_BROWSER_TEST_F(DataProtectionTabCaptureHandlerBrowserTest,
                       NullWebContents) {
  EXPECT_CALL(mock_callback_, Run).Times(0);
  DataProtectionTabCaptureHandler handler(nullptr, media_id_,
                                          mock_callback_.Get());
  EXPECT_EQ(handler.media_id(), media_id_);
}

IN_PROC_BROWSER_TEST_F(DataProtectionTabCaptureHandlerBrowserTest,
                       NonTabWebContents) {
  EXPECT_CALL(mock_callback_, Run).Times(0);
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          GetBrowserWindowInterface()->GetProfile()));
  DataProtectionTabCaptureHandler handler(contents.get(), media_id_,
                                          mock_callback_.Get());
  EXPECT_EQ(handler.media_id(), media_id_);
}

IN_PROC_BROWSER_TEST_F(DataProtectionTabCaptureHandlerBrowserTest,
                       InitialStateAllowed) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetAllowedUrl()));
  content::WebContents* contents = GetActiveWebContents();

  EXPECT_CALL(mock_callback_, Run).Times(0);
  DataProtectionTabCaptureHandler handler(contents, media_id_,
                                          mock_callback_.Get());
  EXPECT_EQ(handler.media_id(), media_id_);
}

IN_PROC_BROWSER_TEST_F(DataProtectionTabCaptureHandlerBrowserTest,
                       InitialStateBlocked) {
  SetScreenshotBlockedForHost("blocked.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetBlockedUrl()));
  content::WebContents* contents = GetActiveWebContents();

  EXPECT_CALL(mock_callback_,
              Run(media_id_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);

  DataProtectionTabCaptureHandler handler(contents, media_id_,
                                          mock_callback_.Get());
}

IN_PROC_BROWSER_TEST_F(DataProtectionTabCaptureHandlerBrowserTest,
                       TransitionAllowedToBlockedAndBack) {
  SetScreenshotBlockedForHost("blocked.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetAllowedUrl()));
  content::WebContents* contents = GetActiveWebContents();

  DataProtectionTabCaptureHandler handler(contents, media_id_,
                                          mock_callback_.Get());

  // Navigating to blocked URL should trigger PAUSE.
  EXPECT_CALL(mock_callback_,
              Run(media_id_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetBlockedUrl()));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_);

  // Navigating back to allowed URL should trigger PLAY.
  EXPECT_CALL(mock_callback_,
              Run(media_id_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetAllowedUrl()));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_);
}

IN_PROC_BROWSER_TEST_F(DataProtectionTabCaptureHandlerBrowserTest,
                       EdgeTriggeredDeduplication) {
  SetScreenshotBlockedForHost("blocked.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetAllowedUrl()));
  content::WebContents* contents = GetActiveWebContents();

  DataProtectionTabCaptureHandler handler(contents, media_id_,
                                          mock_callback_.Get());

  // First navigation to blocked URL triggers PAUSE.
  EXPECT_CALL(mock_callback_,
              Run(media_id_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetBlockedUrl("/title1.html")));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_);

  // Second navigation to another blocked URL should NOT trigger another PAUSE.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetBlockedUrl("/title2.html")));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_);

  // Navigation back to allowed URL triggers PLAY.
  EXPECT_CALL(mock_callback_,
              Run(media_id_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      GetBrowserWindowInterface(),
      embedded_test_server()->GetURL("allowed.com", "/title1.html")));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_);

  // Second navigation to another allowed URL should NOT trigger another PLAY.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      GetBrowserWindowInterface(),
      embedded_test_server()->GetURL("allowed.com", "/title2.html")));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_);
}

IN_PROC_BROWSER_TEST_F(DataProtectionTabCaptureHandlerBrowserTest,
                       DestructionUnregistersCallback) {
  SetScreenshotBlockedForHost("blocked.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetAllowedUrl()));
  content::WebContents* contents = GetActiveWebContents();

  auto handler = std::make_unique<DataProtectionTabCaptureHandler>(
      contents, media_id_, mock_callback_.Get());

  // Destroy the handler.
  handler.reset();

  // Subsequent navigation to blocked URL must NOT invoke the callback.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetBlockedUrl()));
}

// Sharing the browser's own window must pause, and the window must be blocked
// in the picker, while it shows a page that disallows screenshots.
IN_PROC_BROWSER_TEST_F(DataProtectionTabCaptureHandlerBrowserTest,
                       WindowCapturePausedWhileProtected) {
  SetScreenshotBlockedForHost("blocked.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetAllowedUrl()));

  // Start a window capture of this browser window, like a getDisplayMedia()
  // window share from a tab of the same browser.
  const content::DesktopMediaID window_media_id =
      content::DesktopMediaID::RegisterNativeWindow(
          content::DesktopMediaID::TYPE_WINDOW,
          GetBrowserWindowInterface()->GetWindow()->GetNativeWindow());
  blink::MediaStreamDevice device(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
      window_media_id.ToString(), "window");
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      media::mojom::DisplayCaptureSurfaceType::WINDOW,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr, /*initial_zoom_level=*/100);
  blink::mojom::StreamDevices devices;
  devices.video_device = std::move(device);
  std::unique_ptr<content::MediaStreamUI> ui =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          ->RegisterMediaStream(GetActiveWebContents(), devices);
  ui->OnStarted(base::RepeatingClosure(),
                content::MediaStreamUI::SourceCallback(), /*label=*/"label",
                /*screen_capture_ids=*/{window_media_id}, mock_callback_.Get());

  EXPECT_CALL(mock_callback_,
              Run(window_media_id, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetBlockedUrl()));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_);
  EXPECT_TRUE(IsWindowSharingBlockedByDataProtection(window_media_id));

  EXPECT_CALL(mock_callback_,
              Run(window_media_id, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GetAllowedUrl()));
  testing::Mock::VerifyAndClearExpectations(&mock_callback_);
  EXPECT_FALSE(IsWindowSharingBlockedByDataProtection(window_media_id));
}

}  // namespace
