// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/json/json_reader.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gl/gl_switches.h"

using trace_analyzer::TraceEvent;
using trace_analyzer::TraceEventVector;
using trace_analyzer::Query;

namespace {

// Trace events.
static const char kStartRenderEventName[] =
    "RemoteVideoSourceDelegate::RenderFrame";
static const char kEnqueueFrameEventName[] =
    "WebMediaPlayerMSCompositor::EnqueueFrame";
static const char kSetFrameEventName[] =
    "WebMediaPlayerMSCompositor::SetCurrentFrame";
static const char kGetFrameEventName[] =
    "WebMediaPlayerMSCompositor::GetCurrentFrame";
static const char kVideoResourceEventName[] =
    "VideoResourceUpdater::ObtainFrameResources";
static const char kVsyncEventName[] = "Display::DrawAndSwap";

// VideoFrameSubmitter dumps the delay from the handover of a decoded remote
// VideoFrame from webrtc to the moment the OS acknowledges the swap buffers.
static const char kVideoFrameSubmitterEventName[] = "VideoFrameSubmitter";

static const char kEventMatchKey[] = "Timestamp";
static const char kTestResultString[] = "TestVideoDisplayPerf";
static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_video_display_perf_test.html";

struct VideoDisplayPerfTestConfig {
  int width;
  int height;
  int fps;
  bool disable_render_smoothness_algorithm;
};

void CalculateMeanAndMax(const std::vector<double>& inputs,
                         double* mean,
                         double* std_dev,
                         double* max) {
  double sum = 0.0;
  double sqr_sum = 0.0;
  double max_so_far = 0.0;
  size_t count = inputs.size();
  for (const auto& input : inputs) {
    sum += input;
    sqr_sum += input * input;
    max_so_far = std::max(input, max_so_far);
  }
  *max = max_so_far;
  *mean = sum / count;
  *std_dev = sqrt(std::max(0.0, count * sqr_sum - sum * sum)) / count;
}

void PrintMeanAndMax(const std::string& var_name,
                     const std::string& name_modifier,
                     const std::vector<double>& vars) {
  double mean = 0.0;
  double std_dev = 0.0;
  double max = 0.0;
  CalculateMeanAndMax(vars, &mean, &std_dev, &max);
  perf_test::PrintResultMeanAndError(
      kTestResultString, name_modifier, var_name + " Mean",
      base::StringPrintf("%.0lf,%.0lf", mean, std_dev), "μs", true);
  perf_test::PrintResult(kTestResultString, name_modifier, var_name + " Max",
                         base::StringPrintf("%.0lf", max), "μs", true);
}

void FindEvents(trace_analyzer::TraceAnalyzer* analyzer,
                const std::string& event_name,
                const Query& base_query,
                TraceEventVector* events) {
  Query query = Query::EventNameIs(event_name) && base_query;
  analyzer->FindEvents(query, events);
}

void AssociateEvents(trace_analyzer::TraceAnalyzer* analyzer,
                     const std::vector<std::string>& event_names,
                     const std::string& match_string,
                     const Query& base_query) {
  for (size_t i = 0; i < event_names.size() - 1; ++i) {
    Query begin = Query::EventNameIs(event_names[i]);
    Query end = Query::EventNameIs(event_names[i + 1]);
    Query match(Query::EventArg(match_string) == Query::OtherArg(match_string));
    analyzer->AssociateEvents(begin, end, base_query && match);
  }
}

content::WebContents* OpenWebrtcInternalsTab(Browser* browser) {
  chrome::AddTabAt(browser, GURL(), -1, true);
  ui_test_utils::NavigateToURL(browser, GURL("chrome://webrtc-internals"));
  return browser->tab_strip_model()->GetActiveWebContents();
}

std::vector<double> ParseGoogMaxDecodeFromWebrtcInternalsTab(
    const std::string& webrtc_internals_stats_json) {
  std::vector<double> goog_decode_ms;

  std::unique_ptr<base::Value> parsed_json =
      base::JSONReader::ReadDeprecated(webrtc_internals_stats_json);
  base::DictionaryValue* dictionary = nullptr;
  if (!parsed_json.get() || !parsed_json->GetAsDictionary(&dictionary))
    return goog_decode_ms;
  ignore_result(parsed_json.release());

  // |dictionary| should have exactly two entries, one per ssrc.
  if (!dictionary || dictionary->size() != 2u)
    return goog_decode_ms;

  // Only a given |dictionary| entry will have a "stats" entry that has a key
  // that ends with "recv-googMaxDecodeMs" inside (it will start with the ssrc
  // id, but we don't care about that). Then collect the string of "values" out
  // of that key and convert those into the |goog_decode_ms| vector of doubles.
  for (const auto& dictionary_entry : *dictionary) {
    for (const auto& ssrc_entry : dictionary_entry.second->DictItems()) {
      if (ssrc_entry.first != "stats")
        continue;

      for (const auto& stat_entry : ssrc_entry.second.DictItems()) {
        if (!base::EndsWith(stat_entry.first, "recv-googMaxDecodeMs",
                            base::CompareCase::SENSITIVE)) {
          continue;
        }
        base::Value* values_entry = stat_entry.second.FindKey({"values"});
        if (!values_entry)
          continue;
        base::StringTokenizer values_tokenizer(values_entry->GetString(),
                                               "[,]");
        while (values_tokenizer.GetNext()) {
          if (values_tokenizer.token_is_delim())
            continue;
          goog_decode_ms.push_back(atof(values_tokenizer.token().c_str()) *
                                   base::Time::kMicrosecondsPerMillisecond);
        }
      }
    }
  }
  return goog_decode_ms;
}

}  // anonymous namespace

// Tests the performance of Chrome displaying remote video.
//
// This test creates a WebRTC peer connection between two tabs and measures the
// trace events listed in the beginning of this file on the tab receiving
// remote video. In order to cut down from the encode cost, the tab receiving
// remote video does not send any video to its peer.
//
// This test traces certain categories for a period of time. It follows the
// lifetime of a single video frame by synchronizing on the timestamps values
// attached to trace events. Then, it calculates the duration and related stats.

// TODO(https://crbug.com/993020): Fix flakes on Windows bots.
#if defined(OS_WIN)
#define MAYBE_WebRtcVideoDisplayPerfBrowserTest \
  DISABLED_WebRtcVideoDisplayPerfBrowserTest
#else
#define MAYBE_WebRtcVideoDisplayPerfBrowserTest \
  WebRtcVideoDisplayPerfBrowserTest
#endif
class MAYBE_WebRtcVideoDisplayPerfBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<
          std::tuple<gfx::Size /* resolution */,
                     int /* fps */,
                     bool /* disable_render_smoothness_algorithm */>> {
 public:
  MAYBE_WebRtcVideoDisplayPerfBrowserTest() {
    const auto& params = GetParam();
    const gfx::Size& resolution = std::get<0>(params);
    test_config_ = {resolution.width(), resolution.height(),
                    std::get<1>(params), std::get<2>(params)};
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    command_line->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("fps=%d", test_config_.fps));
    if (test_config_.disable_render_smoothness_algorithm)
      command_line->AppendSwitch(switches::kDisableRTCSmoothnessAlgorithm);
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  void TestVideoDisplayPerf(const std::string& video_codec) {
    ASSERT_TRUE(embedded_test_server()->Start());
    // chrome:webrtc-internals doesn't start tracing anything until the
    // connection(s) are up.
    content::WebContents* webrtc_internals_tab =
        OpenWebrtcInternalsTab(browser());
    EXPECT_TRUE(content::ExecuteScript(
        webrtc_internals_tab,
        "currentGetStatsMethod = OPTION_GETSTATS_LEGACY"));

    content::WebContents* left_tab =
        OpenPageAndGetUserMediaInNewTabWithConstraints(
            embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage),
            base::StringPrintf(
                "{audio: true, video: {mandatory: {minWidth: %d, maxWidth: %d, "
                "minHeight: %d, maxHeight: %d}}}",
                test_config_.width, test_config_.width, test_config_.height,
                test_config_.height));
    content::WebContents* right_tab =
        OpenPageAndGetUserMediaInNewTabWithConstraints(
            embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage),
            "{audio: true, video: false}");
    const int process_id =
        right_tab->GetRenderViewHost()->GetProcess()->GetProcess().Pid();

    const std::string disable_cpu_adaptation_constraint(
        "{'optional': [{'googCpuOveruseDetection': false}]}");
    SetupPeerconnectionWithConstraintsAndLocalStream(
        left_tab, disable_cpu_adaptation_constraint);
    SetupPeerconnectionWithConstraintsAndLocalStream(
        right_tab, disable_cpu_adaptation_constraint);

    if (!video_codec.empty()) {
      constexpr bool kPreferHwVideoCodec = true;
      SetDefaultVideoCodec(left_tab, video_codec, kPreferHwVideoCodec);
      SetDefaultVideoCodec(right_tab, video_codec, kPreferHwVideoCodec);
    }
    NegotiateCall(left_tab, right_tab);

    StartDetectingVideo(right_tab, "remote-view");
    WaitForVideoToPlay(right_tab);
    // Run the connection a bit to ramp up.
    test::SleepInJavascript(left_tab, 10000);

    ASSERT_TRUE(tracing::BeginTracing("media,viz,webrtc"));
    // Run the connection for 5 seconds to collect metrics.
    test::SleepInJavascript(left_tab, 5000);

    const std::string webrtc_internals_stats_json = ExecuteJavascript(
        "window.domAutomationController.send("
        "    JSON.stringify(peerConnectionDataStore));",
        webrtc_internals_tab);
    webrtc_decode_latencies_ =
        ParseGoogMaxDecodeFromWebrtcInternalsTab(webrtc_internals_stats_json);
    chrome::CloseWebContents(browser(), webrtc_internals_tab, false);

    std::string json_events;
    ASSERT_TRUE(tracing::EndTracing(&json_events));
    std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer(
        trace_analyzer::TraceAnalyzer::Create(json_events));
    analyzer->AssociateAsyncBeginEndEvents();

    HangUp(left_tab);
    HangUp(right_tab);
    chrome::CloseWebContents(browser(), left_tab, false);
    chrome::CloseWebContents(browser(), right_tab, false);

    ASSERT_TRUE(CalculatePerfResults(analyzer.get(), process_id));
    PrintResults(video_codec);
  }

 private:
  bool CalculatePerfResults(trace_analyzer::TraceAnalyzer* analyzer,
                            int render_process_id) {
    Query match_process_id = Query::EventPidIs(render_process_id);
    const std::vector<std::string> chain_of_events = {
        kStartRenderEventName, kEnqueueFrameEventName, kSetFrameEventName,
        kGetFrameEventName, kVideoResourceEventName};
    AssociateEvents(analyzer, chain_of_events,
                    kEventMatchKey, match_process_id);

    TraceEventVector start_render_events;
    FindEvents(analyzer, kStartRenderEventName, match_process_id,
               &start_render_events);
    if (start_render_events.empty())
      return false;

    // We are only interested in vsync events coming after the first render
    // event. Earlier ones are already missed.
    Query after_first_render_event =
        Query::EventTime() >
        Query::Double(start_render_events.front()->timestamp);
    TraceEventVector vsync_events;
    FindEvents(analyzer, kVsyncEventName, after_first_render_event,
               &vsync_events);
    if (vsync_events.empty())
      return false;

    size_t found_vsync_index = 0;
    size_t skipped_frame_count = 0;
    for (const auto* event : start_render_events) {
      const double start = event->timestamp;

      const TraceEvent* enqueue_frame_event = event->other_event;
      if (!enqueue_frame_event) {
        skipped_frame_count++;
        continue;
      }
      const double enqueue_frame_duration =
          enqueue_frame_event->timestamp - start;

      const TraceEvent* set_frame_event = enqueue_frame_event->other_event;
      if (!set_frame_event) {
        skipped_frame_count++;
        continue;
      }
      const double set_frame_duration =
          set_frame_event->timestamp - enqueue_frame_event->timestamp;

      const TraceEvent* get_frame_event = set_frame_event->other_event;
      if (!get_frame_event) {
        skipped_frame_count++;
        continue;
      }
      const double get_frame_duration =
          get_frame_event->timestamp - set_frame_event->timestamp;

      const TraceEvent* video_resource_event = get_frame_event->other_event;
      if (!video_resource_event) {
        skipped_frame_count++;
        continue;
      }
      const double resource_ready_duration =
          video_resource_event->timestamp - get_frame_event->timestamp;

      // We try to find the closest vsync event after video resource is ready.
      const bool found_vsync = FindFirstOf(
          vsync_events,
          Query::EventTime() > Query::Double(video_resource_event->timestamp +
                                             video_resource_event->duration),
          found_vsync_index, &found_vsync_index);
      if (!found_vsync) {
        skipped_frame_count++;
        continue;
      }
      const double vsync_duration = vsync_events[found_vsync_index]->timestamp -
                                    video_resource_event->timestamp;
      const double total_duration =
          vsync_events[found_vsync_index]->timestamp - start;

      enqueue_frame_durations_.push_back(enqueue_frame_duration);
      set_frame_durations_.push_back(set_frame_duration);
      get_frame_durations_.push_back(get_frame_duration);
      resource_ready_durations_.push_back(resource_ready_duration);
      vsync_durations_.push_back(vsync_duration);
      total_controlled_durations_.push_back(total_duration -
                                            set_frame_duration);
      total_durations_.push_back(total_duration);
    }

    if (start_render_events.size() == skipped_frame_count)
      return false;

    // Calculate the percentage by dividing by the number of frames received.
    skipped_frame_percentage_ =
        100.0 * skipped_frame_count / start_render_events.size();

    // |kVideoFrameSubmitterEventName| is in itself an ASYNC latency measurement
    // from the point where the remote video decode is available (i.e.
    // kStartRenderEventName) until the platform-dependent swap buffers, so by
    // definition is larger than the |total_duration|.
    TraceEventVector video_frame_submitter_events;
    analyzer->FindEvents(Query::MatchAsyncBeginWithNext() &&
                             Query::EventNameIs(kVideoFrameSubmitterEventName),
                         &video_frame_submitter_events);
    for (const auto* event : video_frame_submitter_events) {
      // kVideoFrameSubmitterEventName is divided into a BEGIN, a PAST and an
      // END steps. AssociateAsyncBeginEndEvents paired BEGIN with PAST, but we
      // have to get to the END. Note that if there's no intermediate PAST, it
      // means this wasn't a remote feed VideoFrame, we should not have those in
      // this test. If there's no END, then tracing was cut short.
      if (!event->has_other_event() ||
          event->other_event->phase != TRACE_EVENT_PHASE_ASYNC_STEP_PAST ||
          !event->other_event->has_other_event()) {
        continue;
      }
      const auto begin = event->timestamp;
      const auto end = event->other_event->other_event->timestamp;
      video_frame_submmitter_latencies_.push_back(end - begin);
    }

    return true;
  }

  void PrintResults(const std::string& video_codec) {
    std::string smoothness_indicator =
        test_config_.disable_render_smoothness_algorithm ? "_DisableSmoothness"
                                                         : "";
    std::string name_modifier = base::StringPrintf(
        "%s_%dp%df%s", video_codec.c_str(), test_config_.height,
        test_config_.fps, smoothness_indicator.c_str());
    perf_test::PrintResult(
        kTestResultString, name_modifier, "Skipped frames",
        base::StringPrintf("%.2lf", skipped_frame_percentage_), "percent",
        true);
    // We identify intervals in a way that can help us easily bisect the source
    // of added latency in case of a regression. From these intervals, "Render
    // Algorithm" can take random amount of times based on the vsync cycle it is
    // closest to. Therefore, "Total Controlled Latency" refers to the total
    // times without that section for semi-consistent results.
    PrintMeanAndMax("Passing to Render Algorithm Latency", name_modifier,
                    enqueue_frame_durations_);
    PrintMeanAndMax("Render Algorithm Latency", name_modifier,
                    set_frame_durations_);
    PrintMeanAndMax("Compositor Picking Frame Latency", name_modifier,
                    get_frame_durations_);
    PrintMeanAndMax("Compositor Resource Preparation Latency", name_modifier,
                    resource_ready_durations_);
    PrintMeanAndMax("Vsync Latency", name_modifier, vsync_durations_);
    PrintMeanAndMax("Total Controlled Latency", name_modifier,
                    total_controlled_durations_);
    PrintMeanAndMax("Total Latency", name_modifier, total_durations_);

    PrintMeanAndMax("Post-decode-to-raster latency", name_modifier,
                    video_frame_submmitter_latencies_);
    PrintMeanAndMax("WebRTC decode latency", name_modifier,
                    webrtc_decode_latencies_);
  }

  VideoDisplayPerfTestConfig test_config_;
  // Containers for test results.
  double skipped_frame_percentage_ = 0;
  std::vector<double> enqueue_frame_durations_;
  std::vector<double> set_frame_durations_;
  std::vector<double> get_frame_durations_;
  std::vector<double> resource_ready_durations_;
  std::vector<double> vsync_durations_;
  std::vector<double> total_controlled_durations_;
  std::vector<double> total_durations_;

  // These two put together represent the whole delay from encoded video frames
  // to OS swap buffers call (or callback, depending on the platform).
  std::vector<double> video_frame_submmitter_latencies_;
  std::vector<double> webrtc_decode_latencies_;
};

INSTANTIATE_TEST_SUITE_P(WebRtcVideoDisplayPerfBrowserTests,
                         MAYBE_WebRtcVideoDisplayPerfBrowserTest,
                         testing::Combine(testing::Values(gfx::Size(1280, 720),
                                                          gfx::Size(1920,
                                                                    1080)),
                                          testing::Values(30, 60),
                                          testing::Bool()));

IN_PROC_BROWSER_TEST_P(MAYBE_WebRtcVideoDisplayPerfBrowserTest,
                       MANUAL_TestVideoDisplayPerfVP9) {
  TestVideoDisplayPerf("VP9");
}

#if BUILDFLAG(RTC_USE_H264)
IN_PROC_BROWSER_TEST_P(MAYBE_WebRtcVideoDisplayPerfBrowserTest,
                       MANUAL_TestVideoDisplayPerfH264) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebRtcH264WithOpenH264FFmpeg)) {
    LOG(WARNING) << "Run-time feature WebRTC-H264WithOpenH264FFmpeg disabled. "
                    "Skipping WebRtcVideoDisplayPerfBrowserTest.MANUAL_"
                    "TestVideoDisplayPerfH264 "
                    "(test \"OK\")";
    return;
  }
  TestVideoDisplayPerf("H264");
}
#endif  // BUILDFLAG(RTC_USE_H264)
