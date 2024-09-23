// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

// This tests the --disable-webrtc-encryption command line flag. Disabling
// encryption should only be possible on certain channels.

// NOTE: The test case for each channel will only be exercised when the browser
// is actually built for that channel. This is not ideal. One can test manually
// by e.g. faking the channel returned in chrome::GetChannel(). It's likely good
// to have the test anyway, even though a failure might not be detected until a
// branch has been promoted to another channel. The unit test for
// ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch tests for
// all channels however.
// TODO(grunell): Test the different channel cases for any build.
class WebRtcDisableEncryptionFlagBrowserTest : public WebRtcTestBase {
 public:
  WebRtcDisableEncryptionFlagBrowserTest() {}

  WebRtcDisableEncryptionFlagBrowserTest(
      const WebRtcDisableEncryptionFlagBrowserTest&) = delete;
  WebRtcDisableEncryptionFlagBrowserTest& operator=(
      const WebRtcDisableEncryptionFlagBrowserTest&) = delete;

  ~WebRtcDisableEncryptionFlagBrowserTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Disable encryption with the command line flag.
    command_line->AppendSwitch(switches::kDisableWebRtcEncryption);
  }
};

// Makes a call and checks that there's encryption or not in the SDP offer.
// TODO(crbug.com/40604406): De-flake this for ChromeOs.
// TODO(crbug.com/40636393): De-flake this for ASAN/MSAN Linux, also Windows
// TODO(crbug.com/40182777): De-flake this for MacOS.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    (BUILDFLAG(IS_LINUX) &&                                             \
     (defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER)))
#define MAYBE_VerifyEncryption DISABLED_VerifyEncryption
#else
#define MAYBE_VerifyEncryption VerifyEncryption
#endif
IN_PROC_BROWSER_TEST_F(WebRtcDisableEncryptionFlagBrowserTest,
                       MAYBE_VerifyEncryption) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* left_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  content::WebContents* right_tab =
      OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

  SetupPeerconnectionWithLocalStream(left_tab);
  SetupPeerconnectionWithLocalStream(right_tab);

  NegotiateCall(left_tab, right_tab);

  StartDetectingVideo(left_tab, "remote-view");
  StartDetectingVideo(right_tab, "remote-view");

  WaitForVideoToPlay(left_tab);
  WaitForVideoToPlay(right_tab);

  bool should_detect_encryption = true;
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::UNKNOWN ||
      channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEV) {
    should_detect_encryption = false;
  }
#if BUILDFLAG(IS_ANDROID)
  if (channel == version_info::Channel::BETA)
    should_detect_encryption = false;
#endif

  std::string expected_string = should_detect_encryption ?
    "crypto-seen" : "no-crypto-seen";

  ASSERT_EQ(expected_string,
            ExecuteJavascript("hasSeenCryptoInSdp()", left_tab));

  HangUp(left_tab);
  HangUp(right_tab);
}
