// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/test_stats_dictionary.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

const char kInboundRtp[] = "inbound-rtp";
const char kOutboundRtp[] = "outbound-rtp";

enum class GetStatsVariation {
  PROMISE_BASED,
  CALLBACK_BASED
};

// Sums up "RTC[In/Out]boundRTPStreamStats.bytes_[received/sent]" values.
double GetTotalRTPStreamBytes(
    TestStatsReportDictionary* report, const char* type,
    const char* media_type) {
  DCHECK(type == kInboundRtp || type == kOutboundRtp);
  const char* bytes_name =
      (type == kInboundRtp) ? "bytesReceived" : "bytesSent";
  double total_bytes = 0.0;
  report->ForEach([&type, &bytes_name, &media_type, &total_bytes](
      const TestStatsDictionary& stats) {
    if (stats.GetString("type") == type &&
        stats.GetString("mediaType") == media_type) {
      total_bytes += stats.GetNumber(bytes_name);
    }
  });
  return total_bytes;
}

double GetAudioBytesSent(TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kOutboundRtp, "audio");
}

double GetAudioBytesReceived(TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kInboundRtp, "audio");
}

double GetVideoBytesSent(TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kOutboundRtp, "video");
}

double GetVideoBytesReceived(TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kInboundRtp, "video");
}

// Performance browsertest for WebRTC. This test is manual since it takes long
// to execute and requires the reference files provided by the webrtc.DEPS
// solution (which is only available on WebRTC internal bots).
// Gets its metrics from the standards conformant "RTCPeerConnection.getStats".
class WebRtcStatsPerfBrowserTest : public WebRtcTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
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

  void StartCall(const std::string& audio_codec,
                 const std::string& video_codec,
                 bool prefer_hw_video_codec,
                 const std::string& video_codec_profile) {
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

    left_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    right_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);

    SetupPeerconnectionWithLocalStream(left_tab_);
    SetupPeerconnectionWithLocalStream(right_tab_);
    SetDefaultAudioCodec(left_tab_, audio_codec);
    SetDefaultAudioCodec(right_tab_, audio_codec);
    SetDefaultVideoCodec(left_tab_, video_codec, prefer_hw_video_codec,
                         video_codec_profile);
    SetDefaultVideoCodec(right_tab_, video_codec, prefer_hw_video_codec,
                         video_codec_profile);
    CreateDataChannel(left_tab_, "data");
    CreateDataChannel(right_tab_, "data");
    NegotiateCall(left_tab_, right_tab_);
    StartDetectingVideo(left_tab_, "remote-view");
    StartDetectingVideo(right_tab_, "remote-view");
    WaitForVideoToPlay(left_tab_);
    WaitForVideoToPlay(right_tab_);
  }

  void EndCall() {
    if (left_tab_)
      HangUp(left_tab_);
    if (right_tab_)
      HangUp(right_tab_);
  }

  void RunsAudioAndVideoCallCollectingMetricsWithAudioCodec(
      const std::string& audio_codec) {
    RunsAudioAndVideoCallCollectingMetrics(
        audio_codec, kUseDefaultVideoCodec, false /* prefer_hw_video_codec */,
        "" /* video_codec_profile */, "" /* video_codec_print_modifier */);
  }

  void RunsAudioAndVideoCallCollectingMetricsWithVideoCodec(
      const std::string& video_codec,
      bool prefer_hw_video_codec = false,
      const std::string& video_codec_profile = std::string(),
      const std::string& video_codec_print_modifier = std::string()) {
    RunsAudioAndVideoCallCollectingMetrics(
        kUseDefaultAudioCodec, video_codec, prefer_hw_video_codec,
        video_codec_profile, video_codec_print_modifier);
  }

  void RunsAudioAndVideoCallCollectingMetrics(
      const std::string& audio_codec,
      const std::string& video_codec,
      bool prefer_hw_video_codec,
      const std::string& video_codec_profile,
      const std::string& video_codec_print_modifier) {
    StartCall(audio_codec, video_codec, prefer_hw_video_codec,
              video_codec_profile);

    // Call for 60 seconds so that values may stabilize, bandwidth ramp up, etc.
    test::SleepInJavascript(left_tab_, 60000);

    // The ramp-up may vary greatly and impact the resulting total bytes, to get
    // reliable measurements we do two measurements, at 60 and 70 seconds and
    // look at the average bytes/second in that window.
    double audio_bytes_sent_before = 0.0;
    double audio_bytes_received_before = 0.0;
    double video_bytes_sent_before = 0.0;
    double video_bytes_received_before = 0.0;

    scoped_refptr<TestStatsReportDictionary> report =
        GetStatsReportDictionary(left_tab_);
    if (audio_codec != kUseDefaultAudioCodec) {
      audio_bytes_sent_before = GetAudioBytesSent(report.get());
      audio_bytes_received_before = GetAudioBytesReceived(report.get());

    }
    if (video_codec != kUseDefaultVideoCodec) {
      video_bytes_sent_before = GetVideoBytesSent(report.get());
      video_bytes_received_before = GetVideoBytesReceived(report.get());
    }

    double measure_duration_seconds = 10.0;
    test::SleepInJavascript(left_tab_, static_cast<int>(
        measure_duration_seconds * base::Time::kMillisecondsPerSecond));

    report = GetStatsReportDictionary(left_tab_);
    if (audio_codec != kUseDefaultAudioCodec) {
      double audio_bytes_sent_after = GetAudioBytesSent(report.get());
      double audio_bytes_received_after = GetAudioBytesReceived(report.get());

      double audio_send_rate =
          (audio_bytes_sent_after - audio_bytes_sent_before) /
          measure_duration_seconds;
      double audio_receive_rate =
          (audio_bytes_received_after - audio_bytes_received_before) /
          measure_duration_seconds;

      std::string audio_codec_modifier = "_" + audio_codec;
      perf_test::PrintResult(
          "audio", audio_codec_modifier, "send_rate", audio_send_rate,
          "bytes/second", false);
      perf_test::PrintResult(
          "audio", audio_codec_modifier, "receive_rate", audio_receive_rate,
          "bytes/second", false);
    }
    if (video_codec != kUseDefaultVideoCodec) {
      double video_bytes_sent_after = GetVideoBytesSent(report.get());
      double video_bytes_received_after = GetVideoBytesReceived(report.get());

      double video_send_rate =
          (video_bytes_sent_after - video_bytes_sent_before) /
          measure_duration_seconds;
      double video_receive_rate =
          (video_bytes_received_after - video_bytes_received_before) /
          measure_duration_seconds;

      std::string video_codec_modifier =
          "_" + (video_codec_print_modifier.empty()
                     ? video_codec
                     : video_codec_print_modifier);
      perf_test::PrintResult("video", video_codec_modifier, "send_rate",
                             video_send_rate, "bytes/second", false);
      perf_test::PrintResult(
          "video", video_codec_modifier, "receive_rate", video_receive_rate,
          "bytes/second", false);
    }

    EndCall();
  }

  void RunsAudioAndVideoCallMeasuringGetStatsPerformance(
      GetStatsVariation variation) {
    EXPECT_TRUE(base::TimeTicks::IsHighResolution());

    StartCall(kUseDefaultAudioCodec, kUseDefaultVideoCodec,
              false /* prefer_hw_video_codec */, "");

    double invocation_time = 0.0;
    switch (variation) {
      case GetStatsVariation::PROMISE_BASED:
        invocation_time = (MeasureGetStatsPerformance(left_tab_) +
                           MeasureGetStatsPerformance(right_tab_)) / 2.0;
        break;
      case GetStatsVariation::CALLBACK_BASED:
        invocation_time =
            (MeasureGetStatsCallbackPerformance(left_tab_) +
             MeasureGetStatsCallbackPerformance(right_tab_)) / 2.0;
        break;
    }
    perf_test::PrintResult(
        "getStats",
        (variation == GetStatsVariation::PROMISE_BASED) ?
            "_promise" : "_callback",
        "invocation_time",
        invocation_time,
        "milliseconds",
        false);

    EndCall();
  }

 private:
  content::WebContents* left_tab_ = nullptr;
  content::WebContents* right_tab_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_AudioCodec_opus) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallCollectingMetricsWithAudioCodec("opus");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_AudioCodec_ISAC) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallCollectingMetricsWithAudioCodec("ISAC");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_AudioCodec_G722) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallCollectingMetricsWithAudioCodec("G722");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_AudioCodec_PCMU) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallCollectingMetricsWithAudioCodec("PCMU");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_AudioCodec_PCMA) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallCollectingMetricsWithAudioCodec("PCMA");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_VideoCodec_VP8) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallCollectingMetricsWithVideoCodec("VP8");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_VideoCodec_VP9) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallCollectingMetricsWithVideoCodec("VP9");
}

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_VideoCodec_VP9Profile2) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallCollectingMetricsWithVideoCodec(
      "VP9", true /* prefer_hw_video_codec */,
      WebRtcTestBase::kVP9Profile2Specifier, "VP9p2");
}

#if BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallCollectingMetrics_VideoCodec_H264) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Only run test if run-time feature corresponding to |rtc_use_h264| is on.
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebRtcH264WithOpenH264FFmpeg)) {
    LOG(WARNING) << "Run-time feature WebRTC-H264WithOpenH264FFmpeg disabled. "
                    "Skipping WebRtcPerfBrowserTest."
                    "MANUAL_RunsAudioAndVideoCallCollectingMetrics_VideoCodec_"
                    "H264 (test "
                    "\"OK\")";
    return;
  }
  RunsAudioAndVideoCallCollectingMetricsWithVideoCodec(
      "H264", true /* prefer_hw_video_codec */);
}

#endif  // BUILDFLAG(RTC_USE_H264)

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallMeasuringGetStatsPerformance_Promise) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallMeasuringGetStatsPerformance(
      GetStatsVariation::PROMISE_BASED);
}

IN_PROC_BROWSER_TEST_F(
    WebRtcStatsPerfBrowserTest,
    MANUAL_RunsAudioAndVideoCallMeasuringGetStatsPerformance_Callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  RunsAudioAndVideoCallMeasuringGetStatsPerformance(
      GetStatsVariation::CALLBACK_BASED);
}

}  // namespace

}  // namespace content
