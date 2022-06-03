// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/test_stats_dictionary.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "testing/perf/perf_test.h"
#include "ui/gl/gl_switches.h"

namespace {

static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_video_display_perf_test.html";
static const char kInboundRtp[] = "inbound-rtp";
static const char kOutboundRtp[] = "outbound-rtp";

// Sums up "RTC[In/Out]boundRTPStreamStats.bytes_[received/sent]" values.
double GetTotalRTPStreamBytes(content::TestStatsReportDictionary* report,
                              const char* type,
                              const char* media_type) {
  DCHECK(type == kInboundRtp || type == kOutboundRtp);
  const char* bytes_name =
      (type == kInboundRtp) ? "bytesReceived" : "bytesSent";
  double total_bytes = 0.0;
  report->ForEach([&type, &bytes_name, &media_type,
                   &total_bytes](const content::TestStatsDictionary& stats) {
    if (stats.GetString("type") == type &&
        stats.GetString("mediaType") == media_type) {
      total_bytes += stats.GetNumber(bytes_name);
    }
  });
  return total_bytes;
}

double GetVideoBytesSent(content::TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kOutboundRtp, "video");
}

double GetVideoBytesReceived(content::TestStatsReportDictionary* report) {
  return GetTotalRTPStreamBytes(report, kInboundRtp, "video");
}

}  // anonymous namespace

namespace content {

// Tests the performance of WebRTC peer connection with high bitrate
//
// This test creates a WebRTC peer connection between two tabs and sets a very
// high target bitrate to observe any perf regressions/improvements for such
// cases. In order to achieve this, we use a fake codec that creates a dummy
// output for the given bitrate.
class WebRtcVideoHighBitrateBrowserTest : public WebRtcTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeCodecForPeerConnection);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

 protected:
  void SetDefaultVideoTargetBitrate(content::WebContents* tab,
                                    int bits_per_second) {
    EXPECT_EQ("ok", ExecuteJavascript(
                        base::StringPrintf("setDefaultVideoTargetBitrate(%d)",
                                           bits_per_second),
                        tab));
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcVideoHighBitrateBrowserTest,
                       MANUAL_HighBitrateEncodeDecode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_GE(TestTimeouts::test_launcher_timeout().InSeconds(), 30)
      << "This is a long-running test; you must specify "
         "--test-launcher-timeout to have a value of at least 30000.";
  ASSERT_GE(TestTimeouts::action_max_timeout().InSeconds(), 30)
      << "This is a long-running test; you must specify "
         "--ui-test-action-max-timeout to have a value of at least 30000.";
  ASSERT_LT(TestTimeouts::action_max_timeout(),
            TestTimeouts::test_launcher_timeout())
      << "action_max_timeout needs to be strictly-less-than "
         "test_launcher_timeout";

  content::WebContents* left_tab =
      OpenPageAndGetUserMediaInNewTabWithConstraints(
          embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage),
          "{audio: true, video: true}");
  content::WebContents* right_tab =
      OpenPageAndGetUserMediaInNewTabWithConstraints(
          embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage),
          "{audio: true, video: false}");
  SetupPeerconnectionWithLocalStream(left_tab);
  SetupPeerconnectionWithLocalStream(right_tab);
  const int target_bits_per_second = 80000;
  SetDefaultVideoTargetBitrate(left_tab, target_bits_per_second);
  SetDefaultVideoTargetBitrate(right_tab, target_bits_per_second);
  NegotiateCall(left_tab, right_tab);

  // Run the connection a bit to ramp up.
  test::SleepInJavascript(left_tab, 10000);

  scoped_refptr<TestStatsReportDictionary> sender_report =
      GetStatsReportDictionary(left_tab);
  const double video_bytes_sent_before = GetVideoBytesSent(sender_report.get());
  scoped_refptr<TestStatsReportDictionary> receiver_report =
      GetStatsReportDictionary(right_tab);
  const double video_bytes_received_before =
      GetVideoBytesReceived(receiver_report.get());

  // Collect stats.
  const double duration_in_seconds = 5.0;
  test::SleepInJavascript(
      left_tab, duration_in_seconds * base::Time::kMillisecondsPerSecond);

  sender_report = GetStatsReportDictionary(left_tab);
  const double video_bytes_sent_after = GetVideoBytesSent(sender_report.get());
  receiver_report = GetStatsReportDictionary(right_tab);
  const double video_bytes_received_after =
      GetVideoBytesReceived(receiver_report.get());

  const double video_send_rate =
      (video_bytes_sent_after - video_bytes_sent_before) / duration_in_seconds;
  const double video_receive_rate =
      (video_bytes_received_after - video_bytes_received_before) /
      duration_in_seconds;

  perf_test::PrintResult("video", "", "send_rate", video_send_rate,
                         "bytes/second", false);
  perf_test::PrintResult("video", "", "receive_rate", video_receive_rate,
                         "bytes/second", false);

  HangUp(left_tab);
  HangUp(right_tab);
}

}  // namespace content
