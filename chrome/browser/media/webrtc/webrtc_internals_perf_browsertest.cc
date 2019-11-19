// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_perf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/public/common/features.h"

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";

std::string MakePerfTestLabel(std::string base, bool opus_dtx) {
  if (opus_dtx) {
    return base + "_with_opus_dtx";
  }
  return base;
}

// Performance browsertest for WebRTC. This test is manual since it takes long
// to execute and requires the reference files provided by the webrtc.DEPS
// solution (which is only available on WebRTC internal bots).
// Gets its metrics from "chrome://webrtc-internals".
class WebRtcInternalsPerfBrowserTest : public WebRtcTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Play a suitable, somewhat realistic video file.
    base::FilePath input_video = test::GetReferenceFilesDir()
        .Append(test::kReferenceFileName360p)
        .AddExtension(test::kY4mFileExtension);
    command_line->AppendSwitchPath(switches::kUseFileForFakeVideoCapture,
                                   input_video);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

  // Tries to extract data from peerConnectionDataStore in the webrtc-internals
  // tab. The caller owns the parsed data. Returns NULL on failure.
  base::DictionaryValue* GetWebrtcInternalsData(
      content::WebContents* webrtc_internals_tab) {
    std::string all_stats_json = ExecuteJavascript(
        "window.domAutomationController.send("
        "    JSON.stringify(peerConnectionDataStore));",
        webrtc_internals_tab);

    std::unique_ptr<base::Value> parsed_json =
        base::JSONReader::ReadDeprecated(all_stats_json);
    base::DictionaryValue* result;
    if (parsed_json.get() && parsed_json->GetAsDictionary(&result)) {
      ignore_result(parsed_json.release());
      return result;
    }

    return NULL;
  }

  const base::DictionaryValue* GetDataOnPeerConnection(
      const base::DictionaryValue* all_data,
      int peer_connection_index) {
    base::DictionaryValue::Iterator iterator(*all_data);

    for (int i = 0; i < peer_connection_index && !iterator.IsAtEnd();
        --peer_connection_index) {
      iterator.Advance();
    }

    const base::DictionaryValue* result;
    if (!iterator.IsAtEnd() && iterator.value().GetAsDictionary(&result))
      return result;

    return NULL;
  }

  std::unique_ptr<base::DictionaryValue> MeasureWebRtcInternalsData(
      int duration_msec) {
    chrome::AddTabAt(browser(), GURL(), -1, true);
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://webrtc-internals"));
    content::WebContents* webrtc_internals_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    // TODO(https://crbug.com/1004239): Stop relying on the legacy getStats()
    // API.
    ChangeToLegacyGetStats(webrtc_internals_tab);
    test::SleepInJavascript(webrtc_internals_tab, duration_msec);

    return std::unique_ptr<base::DictionaryValue>(
        GetWebrtcInternalsData(webrtc_internals_tab));
  }

  void RunsAudioVideoCall60SecsAndLogsInternalMetrics(
      const std::string& video_codec,
      bool prefer_hw_video_codec = false,
      const std::string& video_codec_profile = std::string(),
      const std::string& video_codec_print_modifier = std::string()) {
    ASSERT_TRUE(test::HasReferenceFilesInCheckout());
    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_GE(TestTimeouts::test_launcher_timeout().InSeconds(), 100)
        << "This is a long-running test; you must specify "
           "--test-launcher-timeout to have a value of at least 100000.";
    ASSERT_GE(TestTimeouts::action_max_timeout().InSeconds(), 100)
        << "This is a long-running test; you must specify "
           "--ui-test-action-max-timeout to have a value of at least 100000.";
    ASSERT_LT(TestTimeouts::action_max_timeout(),
              TestTimeouts::test_launcher_timeout())
        << "action_max_timeout needs to be strictly-less-than "
           "test_launcher_timeout";

    content::WebContents* left_tab =
        OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    content::WebContents* right_tab =
        OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

    SetupPeerconnectionWithLocalStream(left_tab);
    SetupPeerconnectionWithLocalStream(right_tab);

    if (!video_codec.empty()) {
      SetDefaultVideoCodec(left_tab, video_codec, prefer_hw_video_codec,
                           video_codec_profile);
      SetDefaultVideoCodec(right_tab, video_codec, prefer_hw_video_codec,
                           video_codec_profile);
    }
    NegotiateCall(left_tab, right_tab);

    StartDetectingVideo(left_tab, "remote-view");
    StartDetectingVideo(right_tab, "remote-view");

    WaitForVideoToPlay(left_tab);
    WaitForVideoToPlay(right_tab);

    // Let values stabilize, bandwidth ramp up, etc.
    test::SleepInJavascript(left_tab, 60000);

    // Start measurements.
    std::unique_ptr<base::DictionaryValue> all_data =
        MeasureWebRtcInternalsData(10000);
    ASSERT_TRUE(all_data.get() != NULL);

    const base::DictionaryValue* first_pc_dict =
        GetDataOnPeerConnection(all_data.get(), 0);
    ASSERT_TRUE(first_pc_dict != NULL);
    const std::string print_modifier = video_codec_print_modifier.empty()
                                           ? video_codec
                                           : video_codec_print_modifier;
    test::PrintBweForVideoMetrics(*first_pc_dict, "", print_modifier);
    test::PrintMetricsForAllStreams(*first_pc_dict, "", print_modifier);

    HangUp(left_tab);
    HangUp(right_tab);
  }

  void RunsOneWayCall60SecsAndLogsInternalMetrics(
      const std::string& video_codec,
      bool opus_dtx) {
    ASSERT_TRUE(test::HasReferenceFilesInCheckout());
    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_GE(TestTimeouts::test_launcher_timeout().InSeconds(), 100)
        << "This is a long-running test; you must specify "
           "--test-launcher-timeout to have a value of at least 100000.";
    ASSERT_GE(TestTimeouts::action_max_timeout().InSeconds(), 100)
        << "This is a long-running test; you must specify "
           "--ui-test-action-max-timeout to have a value of at least 100000.";
    ASSERT_LT(TestTimeouts::action_max_timeout(),
              TestTimeouts::test_launcher_timeout())
        << "action_max_timeout needs to be strictly-less-than "
           "test_launcher_timeout";

    content::WebContents* left_tab =
        OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    content::WebContents* right_tab =
        OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

    SetupPeerconnectionWithLocalStream(left_tab);
    SetupPeerconnectionWithoutLocalStream(right_tab);

    if (!video_codec.empty()) {
      SetDefaultVideoCodec(left_tab, video_codec, false /* prefer_hw_codec */);
      SetDefaultVideoCodec(right_tab, video_codec, false /* prefer_hw_codec */);
    }
    if (opus_dtx) {
      EnableOpusDtx(left_tab);
      EnableOpusDtx(right_tab);
    }
    NegotiateCall(left_tab, right_tab);

    // Remote video will only play in one tab since the call is one-way.
    StartDetectingVideo(right_tab, "remote-view");
    WaitForVideoToPlay(right_tab);

    // Let values stabilize, bandwidth ramp up, etc.
    test::SleepInJavascript(left_tab, 60000);

    std::unique_ptr<base::DictionaryValue> all_data =
        MeasureWebRtcInternalsData(10000);
    ASSERT_TRUE(all_data.get() != NULL);

    // This assumes the sending peer connection is always listed first in the
    // data store, and the receiving second.
    const base::DictionaryValue* first_pc_dict =
        GetDataOnPeerConnection(all_data.get(), 0);
    ASSERT_TRUE(first_pc_dict != NULL);
    test::PrintBweForVideoMetrics(
        *first_pc_dict, MakePerfTestLabel("_sendonly", opus_dtx), video_codec);
    test::PrintMetricsForSendStreams(
        *first_pc_dict, MakePerfTestLabel("_sendonly", opus_dtx), video_codec);

    const base::DictionaryValue* second_pc_dict =
        GetDataOnPeerConnection(all_data.get(), 1);
    ASSERT_TRUE(second_pc_dict != NULL);
    test::PrintBweForVideoMetrics(
        *second_pc_dict, MakePerfTestLabel("_recvonly", opus_dtx), video_codec);
    test::PrintMetricsForRecvStreams(
        *second_pc_dict, MakePerfTestLabel("_recvonly", opus_dtx), video_codec);

    HangUp(left_tab);
    HangUp(right_tab);
  }
};

// This is manual for its long execution time.

IN_PROC_BROWSER_TEST_F(
    WebRtcInternalsPerfBrowserTest,
    MANUAL_RunsAudioVideoCall60SecsAndLogsInternalMetricsVp8) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioVideoCall60SecsAndLogsInternalMetrics("VP8");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcInternalsPerfBrowserTest,
    MANUAL_RunsAudioVideoCall60SecsAndLogsInternalMetricsVp9) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioVideoCall60SecsAndLogsInternalMetrics("VP9");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcInternalsPerfBrowserTest,
    MANUAL_RunsAudioVideoCall60SecsAndLogsInternalMetricsVp9Profile2) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioVideoCall60SecsAndLogsInternalMetrics(
      "VP9", true /* prefer_hw_video_codec */,
      WebRtcTestBase::kVP9Profile2Specifier, "VP9p2");
}

#if BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(
    WebRtcInternalsPerfBrowserTest,
    MANUAL_RunsAudioVideoCall60SecsAndLogsInternalMetricsH264) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Only run test if run-time feature corresponding to |rtc_use_h264| is on.
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebRtcH264WithOpenH264FFmpeg)) {
    LOG(WARNING)
        << "Run-time feature WebRTC-H264WithOpenH264FFmpeg disabled. "
           "Skipping WebRtcInternalsPerfBrowserTest."
           "MANUAL_RunsAudioVideoCall60SecsAndLogsInternalMetricsH264 (test "
           "\"OK\")";
    return;
  }
  RunsAudioVideoCall60SecsAndLogsInternalMetrics(
      "H264", true /* prefer_hw_video_codec */);
}

#endif  // BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(
    WebRtcInternalsPerfBrowserTest,
    MANUAL_RunsOneWayCall60SecsAndLogsInternalMetricsDefault) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsOneWayCall60SecsAndLogsInternalMetrics("", false);
}

IN_PROC_BROWSER_TEST_F(
    WebRtcInternalsPerfBrowserTest,
    MANUAL_RunsOneWayCall60SecsAndLogsInternalMetricsWithOpusDtx) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsOneWayCall60SecsAndLogsInternalMetrics("", true);
}
