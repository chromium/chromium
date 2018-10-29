// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

static const char kKeygenAlgorithmRsa[] =
    "{ name: \"RSASSA-PKCS1-v1_5\", modulusLength: 2048, publicExponent: "
    "new Uint8Array([1, 0, 1]), hash: \"SHA-256\" }";
static const char kKeygenAlgorithmEcdsa[] =
    "{ name: \"ECDSA\", namedCurve: \"P-256\" }";

// TODO(crbug.com/898546): Many WebRtcBrowserTests flakily leak.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_WebRtcBrowserTest DISABLED_WebRtcBrowserTest
#else
#define MAYBE_WebRtcBrowserTest WebRtcBrowserTest
#endif

// Top-level integration test for WebRTC. It always uses fake devices; see
// WebRtcWebcamBrowserTest for a test that acquires any real webcam on the
// system.
class MAYBE_WebRtcBrowserTest : public WebRtcTestBase {
 public:
  MAYBE_WebRtcBrowserTest() : left_tab_(nullptr), right_tab_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Always use fake devices.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // Flag used by TestWebAudioMediaStream to force garbage collection.
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }

  void RunsAudioVideoWebRTCCallInTwoTabs(
      const std::string& video_codec = WebRtcTestBase::kUseDefaultVideoCodec,
      bool prefer_hw_video_codec = false,
      const std::string& offer_cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen,
      const std::string& answer_cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen) {
    StartServerAndOpenTabs();

    SetupPeerconnectionWithLocalStream(left_tab_, offer_cert_keygen_alg);
    SetupPeerconnectionWithLocalStream(right_tab_, answer_cert_keygen_alg);

    if (!video_codec.empty()) {
      SetDefaultVideoCodec(left_tab_, video_codec, prefer_hw_video_codec);
      SetDefaultVideoCodec(right_tab_, video_codec, prefer_hw_video_codec);
    }
    NegotiateCall(left_tab_, right_tab_);

    DetectVideoAndHangUp();
  }

  void RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(
      const std::string& cert_keygen_alg =
          WebRtcTestBase::kUseDefaultCertKeygen) {
    StartServerAndOpenTabs();

    // Generate and clone a certificate, resulting in JavaScript variable
    // |gCertificateClone| being set to the resulting clone.
    DeleteDatabase(left_tab_);
    OpenDatabase(left_tab_);
    GenerateAndCloneCertificate(left_tab_, cert_keygen_alg);
    CloseDatabase(left_tab_);
    DeleteDatabase(left_tab_);

    SetupPeerconnectionWithCertificateAndLocalStream(
        left_tab_, "gCertificateClone");
    SetupPeerconnectionWithLocalStream(right_tab_, cert_keygen_alg);

    NegotiateCall(left_tab_, right_tab_);
    VerifyLocalDescriptionContainsCertificate(left_tab_, "gCertificate");

    DetectVideoAndHangUp();
  }

 protected:
  void StartServerAndOpenTabs() {
    ASSERT_TRUE(embedded_test_server()->Start());
    left_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    right_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  }

  void DetectVideoAndHangUp() {
    StartDetectingVideo(left_tab_, "remote-view");
    StartDetectingVideo(right_tab_, "remote-view");
#if !defined(OS_MACOSX)
    // Video is choppy on Mac OS X. http://crbug.com/443542.
    WaitForVideoToPlay(left_tab_);
    WaitForVideoToPlay(right_tab_);
#endif
    HangUp(left_tab_);
    HangUp(right_tab_);
  }

  content::WebContents* left_tab_;
  content::WebContents* right_tab_;
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsVP8) {
  RunsAudioVideoWebRTCCallInTwoTabs("VP8");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsVP9) {
  RunsAudioVideoWebRTCCallInTwoTabs("VP9");
}

#if BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsH264) {
  // Only run test if run-time feature corresponding to |rtc_use_h264| is on.
  if (!base::FeatureList::IsEnabled(content::kWebRtcH264WithOpenH264FFmpeg)) {
    LOG(WARNING) << "Run-time feature WebRTC-H264WithOpenH264FFmpeg disabled. "
        "Skipping WebRtcBrowserTest.RunsAudioVideoWebRTCCallInTwoTabsH264 "
        "(test \"OK\")";
    return;
  }

#if defined(OS_MACOSX)
  // TODO(jam): this test fails with network service only on 10.12.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      base::mac::IsOS10_12()) {
    return;
  }
#endif

  RunsAudioVideoWebRTCCallInTwoTabs("H264", true /* prefer_hw_video_codec */);
}

#endif  // BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, TestWebAudioMediaStream) {
  // This tests against crash regressions for the WebAudio-MediaStream
  // integration.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/webrtc/webaudio_crash.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // A sleep is necessary to be able to detect the crash.
  test::SleepInJavascript(tab, 1000);

  ASSERT_FALSE(tab->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferRsaAnswerRsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    false /* prefer_hw_video_codec */,
                                    kKeygenAlgorithmRsa, kKeygenAlgorithmRsa);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferEcdsaAnswerEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(
      WebRtcTestBase::kUseDefaultVideoCodec, false /* prefer_hw_video_codec */,
      kKeygenAlgorithmEcdsa, kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebRtcBrowserTest,
    RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateRsa) {
  RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(kKeygenAlgorithmRsa);
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebRtcBrowserTest,
    RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificateEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabsWithClonedCertificate(kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferRsaAnswerEcdsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    false /* prefer_hw_video_codec */,
                                    kKeygenAlgorithmRsa, kKeygenAlgorithmEcdsa);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsOfferEcdsaAnswerRsa) {
  RunsAudioVideoWebRTCCallInTwoTabs(WebRtcTestBase::kUseDefaultVideoCodec,
                                    false /* prefer_hw_video_codec */,
                                    kKeygenAlgorithmEcdsa, kKeygenAlgorithmRsa);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       RunsAudioVideoWebRTCCallInTwoTabsGetStatsCallback) {
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  NegotiateCall(left_tab_, right_tab_);

  VerifyStatsGeneratedCallback(left_tab_);

  DetectVideoAndHangUp();
}

#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_RunsAudioVideoWebRTCCallInTwoTabsGetStatsPromise \
  DISABLED_RunsAudioVideoWebRTCCallInTwoTabsGetStatsPromise
#else
#define MAYBE_RunsAudioVideoWebRTCCallInTwoTabsGetStatsPromise \
  RunsAudioVideoWebRTCCallInTwoTabsGetStatsPromise
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_RunsAudioVideoWebRTCCallInTwoTabsGetStatsPromise) {
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  CreateDataChannel(left_tab_, "data");
  CreateDataChannel(right_tab_, "data");
  NegotiateCall(left_tab_, right_tab_);

  std::set<std::string> missing_expected_stats;
  for (const std::string& type : GetWhitelistedStatsTypes(left_tab_)) {
    missing_expected_stats.insert(type);
  }
  for (const std::string& type : VerifyStatsGeneratedPromise(left_tab_)) {
    missing_expected_stats.erase(type);
  }
  for (const std::string& type : missing_expected_stats) {
    EXPECT_TRUE(false) << "Expected stats dictionary is missing: " << type;
  }

  DetectVideoAndHangUp();
}

IN_PROC_BROWSER_TEST_F(
    MAYBE_WebRtcBrowserTest,
    RunsAudioVideoWebRTCCallInTwoTabsEmitsGatheringStateChange) {
  StartServerAndOpenTabs();
  SetupPeerconnectionWithLocalStream(left_tab_);
  SetupPeerconnectionWithLocalStream(right_tab_);
  NegotiateCall(left_tab_, right_tab_);

  std::string ice_gatheringstate =
      ExecuteJavascript("getLastGatheringState()", left_tab_);

  EXPECT_EQ("complete", ice_gatheringstate);
  DetectVideoAndHangUp();
}
