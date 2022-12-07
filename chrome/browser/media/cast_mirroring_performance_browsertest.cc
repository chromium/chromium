// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/cast_mirroring_service_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/audio_bus.h"
#include "media/base/video_frame.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/barcode.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/in_process_receiver.h"
#include "media/cast/test/utility/net_utility.h"
#include "media/cast/test/utility/standalone_cast_environment.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_server_socket.h"
#include "sandbox/policy/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"
#include "third_party/jsoncpp/source/include/json/writer.h"
#include "third_party/openscreen/src/cast/streaming/answer_messages.h"
#include "third_party/openscreen/src/cast/streaming/offer_messages.h"
#include "third_party/openscreen/src/cast/streaming/ssrc.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/gl/gl_switches.h"

namespace {

// Number of events to trim from the beginning and end. These events don't
// contribute anything toward stable measurements: A brief moment of startup
// "jank" is acceptable, and shutdown may result in missing events (e.g., if
// streaming stops a few frames before capture stops).
constexpr int kTrimEvents = 24;  // 1 sec at 24fps, or 0.4 sec at 60 fps.

// Minimum number of events required for a reasonable analysis.
constexpr int kMinDataPointsForFullRun = 100;  // 1s of audio, ~5s at 24fps.

// Minimum number of events required for data analysis in a non-performance run.
constexpr int kMinDataPointsForQuickRun = 3;

// These are how long the browser is run with trace event recording taking
// place.
constexpr base::TimeDelta kFullRunObservationPeriod = base::Seconds(15);
constexpr base::TimeDelta kQuickRunObservationPeriod = base::Seconds(4);

constexpr char kMetricPrefixCastV2[] = "CastV2.";
constexpr char kMetricTimeBetweenCapturesMs[] = "time_between_captures";
constexpr char kMetricAvSyncMs[] = "av_sync";
constexpr char kMetricAbsAvSyncMs[] = "abs_av_sync";
constexpr char kMetricAudioJitterMs[] = "audio_jitter";
constexpr char kMetricVideoJitterMs[] = "video_jitter";
constexpr char kMetricPlayoutResolutionLines[] = "playout_resolution";
constexpr char kMetricResolutionChangesCount[] = "resolution_changes";
constexpr char kMetricFrameDropRatePercent[] = "frame_drop_rate";
constexpr char kMetricTotalLatencyMs[] = "total_latency";
constexpr char kMetricCaptureDurationMs[] = "capture_duration";
constexpr char kMetricSendToRendererMs[] = "send_to_renderer";
constexpr char kMetricEncodeMs[] = "encode";
constexpr char kMetricTransmitMs[] = "transmit";
constexpr char kMetricDecodeMs[] = "decode";
constexpr char kMetricCastLatencyMs[] = "cast_latency";

constexpr char kTestPageLocation[] =
    "/cast/cast_mirroring_performance_browsertest.html";

constexpr base::StringPiece kFullPerformanceRunSwitch = "full-performance-run";

// The test receiver and senders should share the target playout delay.
constexpr int kTargetPlayoutDelayMs = 400;  // milliseconds

perf_test::PerfResultReporter SetUpCastV2Reporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixCastV2, story);
  reporter.RegisterImportantMetric(kMetricTimeBetweenCapturesMs, "ms");
  reporter.RegisterImportantMetric(kMetricAvSyncMs, "ms");
  reporter.RegisterImportantMetric(kMetricAbsAvSyncMs, "ms");
  reporter.RegisterImportantMetric(kMetricAudioJitterMs, "ms");
  reporter.RegisterImportantMetric(kMetricVideoJitterMs, "ms");
  reporter.RegisterImportantMetric(kMetricPlayoutResolutionLines, "lines");
  reporter.RegisterImportantMetric(kMetricResolutionChangesCount, "count");
  reporter.RegisterImportantMetric(kMetricFrameDropRatePercent, "percent");
  reporter.RegisterImportantMetric(kMetricTotalLatencyMs, "ms");
  reporter.RegisterImportantMetric(kMetricCaptureDurationMs, "ms");
  reporter.RegisterImportantMetric(kMetricSendToRendererMs, "ms");
  reporter.RegisterImportantMetric(kMetricEncodeMs, "ms");
  reporter.RegisterImportantMetric(kMetricTransmitMs, "ms");
  reporter.RegisterImportantMetric(kMetricDecodeMs, "ms");
  reporter.RegisterImportantMetric(kMetricCastLatencyMs, "ms");
  return reporter;
}

std::string VectorToString(const std::vector<double>& values) {
  CHECK(values.size());
  std::string csv;
  for (const auto& val : values) {
    csv += base::NumberToString(val) + ",";
  }
  // Strip off trailing comma.
  csv.pop_back();
  return csv;
}

void MaybeAddResultList(const perf_test::PerfResultReporter& reporter,
                        const std::string& metric,
                        const std::vector<double>& values) {
  if (values.size() == 0) {
    LOG(ERROR) << "No events for " << metric;
    return;
  }
  reporter.AddResultList(metric, VectorToString(values));
}

// A convenience macro to run a gtest expectation in the "full performance run"
// setting, or else a warning that something is not being entirely tested in the
// "CQ run" setting. This is required because the test runs in the CQ may not be
// long enough to collect sufficient tracing data; and, unfortunately, there's
// nothing we can do about that.
#define EXPECT_FOR_PERFORMANCE_RUN(expr)           \
  if (!(expr)) {                                   \
    const char* out = #expr;                       \
    if (is_full_performance_run_) {                \
      LOG(ERROR) << "Failure: " << out;            \
    } else {                                       \
      LOG(WARNING) << "Allowing failure: " << out; \
    }                                              \
  }

enum TestFlags {
  kSmallWindow = 1 << 2,  // Window size: 1 = 800x600, 0 = 2000x1000
  kProxyWifi = 1 << 6,    // Run UDP through UDPProxy wifi profile.
  kProxySlow = 1 << 7,    // Run UDP through UDPProxy slow profile.
  kProxyBad = 1 << 8,     // Run UDP through UDPProxy bad profile.
  kSlowClock = 1 << 9,    // Receiver clock is 10 seconds slow.
  kFastClock = 1 << 10,   // Receiver clock is 10 seconds fast.
};

struct SharedSenderReceiverConfig {
  std::string aes_key;
  std::string aes_iv_mask;
  openscreen::cast::Ssrc receiver_ssrc;
  openscreen::cast::Ssrc sender_ssrc;
};

struct SharedSenderReceiverConfigs {
  SharedSenderReceiverConfig audio;
  SharedSenderReceiverConfig video;
};

media::cast::FrameReceiverConfig WithSharedConfig(
    const media::cast::FrameReceiverConfig& config,
    const SharedSenderReceiverConfig& shared) {
  media::cast::FrameReceiverConfig result = config;
  result.aes_key = shared.aes_key;
  result.aes_iv_mask = shared.aes_iv_mask;
  result.receiver_ssrc = shared.receiver_ssrc;
  result.sender_ssrc = shared.sender_ssrc;
  result.rtp_max_delay_ms = kTargetPlayoutDelayMs;
  return result;
}

void ContinueBrowserFor(base::TimeDelta duration) {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}

using TraceAnalyzerUniquePtr = std::unique_ptr<trace_analyzer::TraceAnalyzer>;

void QueryTraceEvents(trace_analyzer::TraceAnalyzer* analyzer,
                      base::StringPiece event_name,
                      trace_analyzer::TraceEventVector* events) {
  const trace_analyzer::Query kQuery =
      trace_analyzer::Query::EventNameIs(std::string(event_name)) &&
      (trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN) ||
       trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_BEGIN) ||
       trace_analyzer::Query::EventPhaseIs(
           TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN) ||
       trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_FLOW_BEGIN) ||
       trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_INSTANT) ||
       trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_COMPLETE));
  analyzer->FindEvents(kQuery, events);
}

std::string MakeBase64EncodedGZippedString(const std::string& input) {
  std::string gzipped_input;
  compression::GzipCompress(input, &gzipped_input);
  std::string result;
  base::Base64Encode(gzipped_input, &result);

  // Break up the string with newlines to make it easier to handle in the
  // console logs.
  constexpr size_t kMaxLineWidth = 80;
  std::string formatted_result;
  formatted_result.reserve(result.size() + 1 + (result.size() / kMaxLineWidth));
  for (std::string::size_type src_pos = 0; src_pos < result.size();
       src_pos += kMaxLineWidth) {
    formatted_result.append(result, src_pos, kMaxLineWidth);
    formatted_result.append(1, '\n');
  }
  return formatted_result;
}

TraceAnalyzerUniquePtr TraceAndObserve(
    bool is_full_performance_run,
    const std::string& category_patterns,
    const std::vector<base::StringPiece>& event_names,
    int required_event_count) {
  const base::TimeDelta observation_period = is_full_performance_run
                                                 ? kFullRunObservationPeriod
                                                 : kQuickRunObservationPeriod;

  VLOG(1) << "Starting tracing...";
  {
    // Wait until all child processes have ACK'ed that they are now tracing.
    base::trace_event::TraceConfig trace_config(
        category_patterns, base::trace_event::RECORD_CONTINUOUSLY);
    base::RunLoop run_loop;
    const bool did_begin_tracing = tracing::BeginTracingWithTraceConfig(
        trace_config, run_loop.QuitClosure());
    CHECK(did_begin_tracing);
    run_loop.Run();
  }

  VLOG(1) << "Running browser for " << observation_period.InSecondsF()
          << " sec...";
  ContinueBrowserFor(observation_period);

  VLOG(1) << "Observation period has completed. Ending tracing...";
  std::string json_events;
  const bool success = tracing::EndTracing(&json_events);
  CHECK(success);

  std::unique_ptr<trace_analyzer::TraceAnalyzer> result(
      trace_analyzer::TraceAnalyzer::Create(json_events));
  result->AssociateAsyncBeginEndEvents();
  bool have_enough_events = true;
  for (const auto& event_name : event_names) {
    trace_analyzer::TraceEventVector events;
    QueryTraceEvents(result.get(), event_name, &events);
    VLOG(1) << "Collected " << events.size() << " events ("
            << required_event_count << " required) for: " << event_name;
    if (static_cast<int>(events.size()) < required_event_count) {
      have_enough_events = false;
    }
  }
  LOG_IF(WARNING, !have_enough_events) << "Insufficient data collected.";

  VLOG_IF(2, result) << "Dump of trace events (trace_events.json.gz.b64):\n"
                     << MakeBase64EncodedGZippedString(json_events);
  return result;
}

class SkewedCastEnvironment : public media::cast::StandaloneCastEnvironment {
 public:
  explicit SkewedCastEnvironment(const base::TimeDelta& delta)
      : StandaloneCastEnvironment(),
        skewed_clock_(base::DefaultTickClock::GetInstance()) {
    // If testing with a receiver clock that is ahead or behind the sender
    // clock, fake a clock that is offset and also ticks at a rate of 50 parts
    // per million faster or slower than the local sender's clock. This is the
    // worst-case scenario for skew in-the-wild.
    if (!delta.is_zero()) {
      const double skew = delta.is_negative() ? 0.999950 : 1.000050;
      skewed_clock_.SetSkew(skew, delta);
    }
    clock_ = &skewed_clock_;
  }

 protected:
  ~SkewedCastEnvironment() override {}

 private:
  media::cast::test::SkewedTickClock skewed_clock_;
};

// We log one of these for each call to OnAudioFrame/OnVideoFrame.
struct TimeData {
  TimeData(uint16_t frame_no_, base::TimeTicks playout_time_)
      : frame_no(frame_no_), playout_time(playout_time_) {}
  // The unit here is video frames, for audio data there can be duplicates.
  // This was decoded from the actual audio/video data.
  uint16_t frame_no;
  // This is when we should play this data, according to the sender.
  base::TimeTicks playout_time;
};

// TODO(hubbe): Move to media/cast to use for offline log analysis.
class MeanAndError {
 public:
  explicit MeanAndError(const std::vector<double>& values) {
    double sum = 0.0;
    double sqr_sum = 0.0;
    num_values_ = values.size();
    if (num_values_ > 0) {
      for (size_t i = 0; i < num_values_; i++) {
        sum += values[i];
        sqr_sum += values[i] * values[i];
      }
      mean_ = sum / num_values_;
      std_dev_ =
          sqrt(std::max(0.0, num_values_ * sqr_sum - sum * sum)) / num_values_;
    } else {
      mean_ = NAN;
      std_dev_ = NAN;
    }
  }

  void SetMeanAsAbsoluteValue() { mean_ = std::abs(mean_); }

  std::string AsString() const {
    return base::StringPrintf("%f,%f", mean_, std_dev_);
  }

 private:
  size_t num_values_;
  double mean_;
  double std_dev_;
};

// This function checks how smooth the data in |data| is.
// It computes the average error of deltas and the average delta.
// If data[x] == x * A + B, then this function returns zero.
// The unit is milliseconds.
static std::vector<double> AnalyzeJitter(const std::vector<TimeData>& data) {
  VLOG(0) << "Jitter analysis on " << data.size() << " values.";
  std::vector<double> deltas;
  double sum = 0.0;
  for (size_t i = 1; i < data.size(); i++) {
    double delta =
        (data[i].playout_time - data[i - 1].playout_time).InMillisecondsF();
    deltas.push_back(delta);
    sum += delta;
  }
  if (deltas.empty()) {
    // Not enough data. Don't do the following calculation, to avoid a
    // divide-by-zero.
  } else {
    double mean = sum / deltas.size();
    for (size_t i = 0; i < deltas.size(); i++) {
      deltas[i] = fabs(mean - deltas[i]);
    }
  }

  return deltas;
}

// An in-process Cast receiver that examines the audio/video frames being
// received and logs some data about each received audio/video frame.
class TestPatternReceiver : public media::cast::InProcessReceiver {
 public:
  explicit TestPatternReceiver(
      const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
      const net::IPEndPoint& local_end_point,
      bool is_full_performance_run,
      const SharedSenderReceiverConfigs& configs)
      : InProcessReceiver(
            cast_environment,
            local_end_point,
            net::IPEndPoint(),
            WithSharedConfig(media::cast::GetDefaultAudioReceiverConfig(),
                             configs.audio),
            WithSharedConfig(media::cast::GetDefaultVideoReceiverConfig(),
                             configs.video)),
        is_full_performance_run_(is_full_performance_run) {}

  TestPatternReceiver(const TestPatternReceiver&) = delete;
  TestPatternReceiver& operator=(const TestPatternReceiver&) = delete;

  typedef std::map<uint16_t, base::TimeTicks> TimeMap;

  // Build a map from frame ID (as encoded in the audio and video data)
  // to the rtp timestamp for that frame. Note that there will be multiple
  // audio frames which all have the same frame ID. When that happens we
  // want the minimum rtp timestamp, because that audio frame is supposed
  // to play at the same time that the corresponding image is presented.
  void MapFrameTimes(const std::vector<TimeData>& events, TimeMap* map) {
    const int trim_count = is_full_performance_run_ ? kTrimEvents : 0;
    for (int i = trim_count; i < static_cast<int>(events.size()) - trim_count;
         i++) {
      base::TimeTicks& frame_tick = (*map)[events[i].frame_no];
      if (frame_tick.is_null()) {
        frame_tick = events[i].playout_time;
      } else {
        frame_tick = std::min(events[i].playout_time, frame_tick);
      }
    }
  }

  void Analyze(const std::string& story) {
    // First, find the minimum rtp timestamp for each audio and video frame.
    // Note that the data encoded in the audio stream contains video frame
    // numbers. So in a 30-fps video stream, there will be 1/30s of "1", then
    // 1/30s of "2", etc.
    TimeMap audio_frame_times, video_frame_times;
    MapFrameTimes(audio_events_, &audio_frame_times);
    const int min_data_points = is_full_performance_run_
                                    ? kMinDataPointsForFullRun
                                    : kMinDataPointsForQuickRun;
    EXPECT_FOR_PERFORMANCE_RUN(min_data_points <=
                               static_cast<int>(audio_frame_times.size()));
    MapFrameTimes(video_events_, &video_frame_times);

    EXPECT_FOR_PERFORMANCE_RUN(min_data_points <=
                               static_cast<int>(video_frame_times.size()));
    std::vector<double> deltas;
    for (TimeMap::const_iterator i = audio_frame_times.begin();
         i != audio_frame_times.end(); ++i) {
      TimeMap::const_iterator j = video_frame_times.find(i->first);
      if (j != video_frame_times.end()) {
        deltas.push_back((i->second - j->second).InMillisecondsF());
      }
    }

    EXPECT_FOR_PERFORMANCE_RUN(min_data_points <=
                               static_cast<int>(deltas.size()));

    auto reporter = SetUpCastV2Reporter(story);
    MaybeAddResultList(reporter, kMetricAvSyncMs, deltas);
    // Close to zero is better (av_sync can be negative).
    if (deltas.size()) {
      MeanAndError av_sync(deltas);
      av_sync.SetMeanAsAbsoluteValue();
      reporter.AddResultMeanAndError(kMetricAbsAvSyncMs, av_sync.AsString());
    }
    // lower is better.
    MaybeAddResultList(reporter, kMetricAudioJitterMs,
                       AnalyzeJitter(audio_events_));
    // lower is better.
    MaybeAddResultList(reporter, kMetricVideoJitterMs,
                       AnalyzeJitter(video_events_));

    // Mean resolution of video at receiver. Lower stddev is better, while the
    // mean should be something reasonable given the network constraints
    // (usually 480 lines or more). Note that this is the video resolution at
    // the receiver, but changes originate on the sender side.
    std::vector<double> slice_for_analysis;
    const int trim_count = is_full_performance_run_ ? kTrimEvents : 0;
    if (static_cast<int>(video_frame_lines_.size()) > trim_count * 2) {
      slice_for_analysis.reserve(video_frame_lines_.size() - trim_count * 2);
      EXPECT_FOR_PERFORMANCE_RUN(
          min_data_points <= static_cast<int>(slice_for_analysis.capacity()));
      std::transform(video_frame_lines_.begin() + trim_count,
                     video_frame_lines_.end() - trim_count,
                     std::back_inserter(slice_for_analysis),
                     [](int lines) { return static_cast<double>(lines); });
    }
    MaybeAddResultList(reporter, kMetricPlayoutResolutionLines,
                       slice_for_analysis);

    // Number of resolution changes. Lower is better (and 1 is ideal). Zero
    // indicates a lack of data.
    int last_lines = -1;
    int change_count = 0;
    for (int i = trim_count;
         i < static_cast<int>(video_frame_lines_.size()) - trim_count; ++i) {
      if (video_frame_lines_[i] != last_lines) {
        ++change_count;
        last_lines = video_frame_lines_[i];
      }
    }
    EXPECT_FOR_PERFORMANCE_RUN(change_count > 0);
    reporter.AddResult(kMetricResolutionChangesCount,
                       static_cast<size_t>(change_count));
  }

 private:
  // Invoked by InProcessReceiver for each received audio frame.
  void OnAudioFrame(std::unique_ptr<media::AudioBus> audio_frame,
                    base::TimeTicks playout_time,
                    bool is_continuous) override {
    CHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    if (audio_frame->frames() <= 0) {
      NOTREACHED() << "OnAudioFrame called with no samples?!?";
      return;
    }

    TRACE_EVENT_INSTANT1("cast_perf_test", "AudioFramePlayout",
                         TRACE_EVENT_SCOPE_THREAD, "playout_time",
                         (playout_time - base::TimeTicks()).InMicroseconds());

    // Note: This is the number of the video frame that this audio belongs to.
    uint16_t frame_no;
    if (media::cast::DecodeTimestamp(audio_frame->channel(0),
                                     audio_frame->frames(), &frame_no)) {
      audio_events_.push_back(TimeData(frame_no, playout_time));
    } else {
      DVLOG(2) << "Failed to decode audio timestamp!";
    }
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks playout_time,
                    bool is_continuous) override {
    CHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    TRACE_EVENT_INSTANT1("cast_perf_test", "VideoFramePlayout",
                         TRACE_EVENT_SCOPE_THREAD, "playout_time",
                         (playout_time - base::TimeTicks()).InMicroseconds());

    uint16_t frame_no;
    if (media::cast::test::DecodeBarcode(*video_frame, &frame_no)) {
      video_events_.push_back(TimeData(frame_no, playout_time));
    } else {
      DVLOG(2) << "Failed to decode barcode!";
    }

    video_frame_lines_.push_back(video_frame->visible_rect().height());
  }

  const bool is_full_performance_run_;

  std::vector<TimeData> audio_events_;
  std::vector<TimeData> video_events_;

  // The height (number of lines) of each video frame received.
  std::vector<int> video_frame_lines_;
};

class TestCompleteObserver {
 public:
  MOCK_METHOD(void, TestComplete, ());
};

class CastV2PerformanceTest : public InProcessBrowserTest,
                              public testing::WithParamInterface<int> {
 public:
  CastV2PerformanceTest() = default;
  ~CastV2PerformanceTest() override = default;

  bool HasFlag(TestFlags flag) const { return (GetParam() & flag) == flag; }

  std::string GetSuffixForTestFlags() const {
    std::string suffix;
    // Note: Add "_gpu" tag for backwards-compatibility with existing
    // Performance Dashboard timeseries data.
    suffix += "gpu";
    if (HasFlag(kSmallWindow))
      suffix += "_small";
    if (HasFlag(kProxyWifi))
      suffix += "_wifi";
    if (HasFlag(kProxySlow))
      suffix += "_slowwifi";
    if (HasFlag(kProxyBad))
      suffix += "_bad";
    if (HasFlag(kSlowClock))
      suffix += "_slow";
    if (HasFlag(kFastClock))
      suffix += "_fast";
    return suffix;
  }

  void SetUp() override {
    // Necessary for screen capture.
    EnablePixelOutput();

    feature_list_.InitWithFeatures(
        {
            features::kAudioServiceSandbox,
            features::kAudioServiceLaunchOnStartup,
            features::kAudioServiceOutOfProcess,
        },
        {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    best_effort_fence_.emplace();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    is_full_performance_run_ =
        command_line->HasSwitch(kFullPerformanceRunSwitch);

    if (HasFlag(kSmallWindow)) {
      command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");
    } else {
      command_line->AppendSwitchASCII(switches::kWindowSize, "2000,1500");
    }
  }

  // The key contains the name of the argument and the argument.
  typedef std::pair<std::string, double> EventMapKey;
  typedef std::map<EventMapKey, const trace_analyzer::TraceEvent*> EventMap;

  // Make events findable by their arguments, for instance, if an
  // event has a "timestamp": 238724 argument, the map will contain
  // pair<"timestamp", 238724> -> &event.  All arguments are indexed.
  void IndexEvents(trace_analyzer::TraceAnalyzer* analyzer,
                   const std::string& event_name,
                   EventMap* event_map) {
    trace_analyzer::TraceEventVector events;
    QueryTraceEvents(analyzer, event_name, &events);
    for (size_t i = 0; i < events.size(); i++) {
      std::map<std::string, double>::const_iterator j;
      for (j = events[i]->arg_numbers.begin();
           j != events[i]->arg_numbers.end(); ++j) {
        (*event_map)[*j] = events[i];
      }
    }
  }

  // Look up an event in |event_map|. The return event will have the same
  // value for the argument |key_name| as |prev_event|.
  const trace_analyzer::TraceEvent* FindNextEvent(
      const EventMap& event_map,
      const trace_analyzer::TraceEvent* prev_event,
      std::string key_name) {
    const auto arg_it = prev_event->arg_numbers.find(key_name);
    if (arg_it == prev_event->arg_numbers.end())
      return nullptr;
    const EventMapKey& key = *arg_it;
    const auto event_it = event_map.find(key);
    if (event_it == event_map.end())
      return nullptr;
    return event_it->second;
  }

  void NavigateToTestPagePath(const std::string& path) const {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), https_server_->GetURL(path)));
  }

  // Given a vector of vector of data, extract the difference between
  // two columns (|col_a| and |col_b|) and output the result as a
  // performance metric.
  void OutputMeasurement(const std::vector<std::vector<double>>& data,
                         const std::string& metric,
                         int col_a,
                         int col_b) {
    std::vector<double> tmp;
    for (size_t i = 0; i < data.size(); i++) {
      tmp.push_back((data[i][col_b] - data[i][col_a]) / 1000.0);
    }
    auto reporter = SetUpCastV2Reporter(GetSuffixForTestFlags());
    MaybeAddResultList(reporter, metric, tmp);
  }

  // Analyze the latency of each frame as it goes from capture to playout. The
  // event tracing system is used to track the frames.
  void AnalyzeLatency(trace_analyzer::TraceAnalyzer* analyzer) {
    // Retrieve and index all "checkpoint" events related to frames progressing
    // from start to finish.
    trace_analyzer::TraceEventVector capture_events;
    // Sender side:
    QueryTraceEvents(analyzer, "Capture", &capture_events);
    EventMap onbuffer, sink, inserted, encoded, transmitted, decoded, done;
    IndexEvents(analyzer, "OnBufferReceived", &onbuffer);
    IndexEvents(analyzer, "ConsumeVideoFrame", &sink);
    IndexEvents(analyzer, "InsertRawVideoFrame", &inserted);
    IndexEvents(analyzer, "VideoFrameEncoded", &encoded);
    // Receiver side:
    IndexEvents(analyzer, "PullEncodedVideoFrame", &transmitted);
    IndexEvents(analyzer, "VideoFrameDecoded", &decoded);
    IndexEvents(analyzer, "VideoFramePlayout", &done);

    // Analyzing latency is non-trivial, because only the frame timestamps
    // uniquely identify frames AND the timestamps take varying forms throughout
    // the pipeline (TimeTicks, TimeDelta, RtpTimestamp, etc.). Luckily, each
    // neighboring stage in the pipeline knows about the timestamp from the
    // prior stage, in whatever form it had, and so it's possible to track
    // specific frames all the way from capture until playout at the receiver.
    std::vector<std::pair<EventMap*, std::string>> event_maps;
    event_maps.push_back(std::make_pair(&onbuffer, "time_delta"));
    event_maps.push_back(std::make_pair(&sink, "time_delta"));
    event_maps.push_back(std::make_pair(&inserted, "timestamp"));
    event_maps.push_back(std::make_pair(&encoded, "rtp_timestamp"));
    event_maps.push_back(std::make_pair(&transmitted, "rtp_timestamp"));
    event_maps.push_back(std::make_pair(&decoded, "rtp_timestamp"));
    event_maps.push_back(std::make_pair(&done, "playout_time"));

    // For each "begin capture" event, search for all the events following it,
    // producing a matrix of when each frame reached each pipeline checkpoint.
    // See the "cheat sheet" below for a description of each pipeline
    // checkpoint.
    const int trim_count = is_full_performance_run_ ? kTrimEvents : 0;
    EXPECT_FOR_PERFORMANCE_RUN((trim_count * 2) <=
                               static_cast<int>(capture_events.size()));
    std::vector<std::vector<double>> traced_frames;
    for (int i = trim_count;
         i < static_cast<int>(capture_events.size()) - trim_count; i++) {
      std::vector<double> times;
      const trace_analyzer::TraceEvent* event = capture_events[i];
      if (!event->other_event)
        continue;  // Begin capture event without a corresponding end event.
      times.push_back(event->timestamp);  // begin capture
      event = event->other_event;
      times.push_back(event->timestamp);  // end capture
      const trace_analyzer::TraceEvent* prev_event = event;
      for (size_t j = 0; j < event_maps.size(); j++) {
        event = FindNextEvent(*event_maps[j].first, prev_event,
                              event_maps[j].second);
        if (!event)
          break;  // Missing an event: The frame was dropped along the way.
        prev_event = event;
        times.push_back(event->timestamp);
      }
      if (event) {
        // Successfully traced frame from beginning to end.
        traced_frames.push_back(std::move(times));
      }
    }

    // Report the fraction of captured frames that were dropped somewhere along
    // the way (i.e., before playout at the receiver).
    const int capture_event_count = capture_events.size() - 2 * trim_count;
    EXPECT_FOR_PERFORMANCE_RUN((is_full_performance_run_
                                    ? kMinDataPointsForFullRun
                                    : kMinDataPointsForQuickRun) <=
                               capture_event_count);
    const double success_percent =
        (capture_event_count == 0)
            ? NAN
            : (100.0 * traced_frames.size() / capture_event_count);
    auto reporter = SetUpCastV2Reporter(GetSuffixForTestFlags());
    reporter.AddResult(kMetricFrameDropRatePercent, 100 - success_percent);

    // Report the latency between various pairs of checkpoints in the pipeline.
    // Lower latency is better for all of these measurements.
    //
    // Cheat sheet:
    //   0 = Sender: capture begin
    //   1 = Sender: capture end
    //   2 = Sender: memory buffer reached the render process
    //   3 = Sender: frame routed to Cast RTP consumer
    //   4 = Sender: frame reached VideoSender::InsertRawVideoFrame()
    //   5 = Sender: frame encoding complete, queueing for transmission
    //   6 = Receiver: frame fully received from network
    //   7 = Receiver: frame decoded
    //   8 = Receiver: frame played out
    OutputMeasurement(traced_frames, kMetricTotalLatencyMs, 0, 8);
    OutputMeasurement(traced_frames, kMetricCaptureDurationMs, 0, 1);
    OutputMeasurement(traced_frames, kMetricSendToRendererMs, 1, 3);
    OutputMeasurement(traced_frames, kMetricEncodeMs, 3, 5);
    OutputMeasurement(traced_frames, kMetricTransmitMs, 5, 6);
    OutputMeasurement(traced_frames, kMetricDecodeMs, 6, 7);
    OutputMeasurement(traced_frames, kMetricCastLatencyMs, 3, 8);
  }

  std::vector<double> AnalyzeTraceDistance(
      trace_analyzer::TraceAnalyzer* analyzer,
      const std::string& event_name) {
    trace_analyzer::TraceEventVector events;
    QueryTraceEvents(analyzer, event_name, &events);

    const int trim_count = is_full_performance_run_ ? kTrimEvents : 0;
    std::vector<double> deltas;
    for (int i = trim_count + 1;
         i < static_cast<int>(events.size()) - trim_count; ++i) {
      double delta_micros = events[i]->timestamp - events[i - 1]->timestamp;
      deltas.push_back(delta_micros / 1000.0);
    }
    return deltas;
  }

  void StartReceiver(const net::IPEndPoint& endpoint,
                     const SharedSenderReceiverConfigs& shared_configs) {
    // Start the in-process receiver that examines audio/video for the expected
    // test patterns.
    base::TimeDelta delta = base::Seconds(0);
    if (HasFlag(kFastClock)) {
      delta = base::Seconds(10);
    }
    if (HasFlag(kSlowClock)) {
      delta = base::Seconds(-10);
    }

    cast_environment_ = base::MakeRefCounted<SkewedCastEnvironment>(delta);
    receiver_ = std::make_unique<TestPatternReceiver>(
        cast_environment_, endpoint, is_full_performance_run_, shared_configs);
    receiver_->Start();
  }

 protected:
  // Ensure best effort tasks are not required for this test to pass.
  absl::optional<base::ThreadPoolInstance::ScopedBestEffortExecutionFence>
      best_effort_fence_;

  // HTTPS server for loading pages from the test data dir.
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  // Cast environment used for the Receiver.
  scoped_refptr<media::cast::StandaloneCastEnvironment> cast_environment_;

  // Test receiver.
  std::unique_ptr<TestPatternReceiver> receiver_;

  // Whether this is a full performance run, or if we just want to do a sanity
  // check.
  bool is_full_performance_run_ = true;

  // Manages the audio service feature set.
  base::test::ScopedFeatureList feature_list_;
};

class TestTabMirroringSession : public mirroring::mojom::SessionObserver,
                                public mirroring::mojom::CastMessageChannel {
 public:
  explicit TestTabMirroringSession(CastV2PerformanceTest* parent)
      : parent_(parent) {}

  // Start the mirroring session. Port is specified as well as endpoint, in
  // case the sender needs to connect to a proxy.
  void Start(content::WebContents* contents,
             const net::IPEndPoint& endpoint,
             int port) {
    ASSERT_TRUE(contents);

    receiver_endpoint_ = endpoint;
    udp_port_ = port;
    mirroring::CastMirroringServiceHost::GetForTab(
        contents, host_.BindNewPipeAndPassReceiver());

    mojo::PendingRemote<mirroring::mojom::SessionObserver> observer_remote;
    observer_receiver_.Bind(observer_remote.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<mirroring::mojom::CastMessageChannel> channel_remote;
    channel_receiver_.Bind(channel_remote.InitWithNewPipeAndPassReceiver());

    auto session_params = mirroring::mojom::SessionParameters::New(
        mirroring::mojom::SessionType::AUDIO_AND_VIDEO, endpoint.address(),
        "model_name", "friendly_name", "sender-123", "receiver-456",
        base::Milliseconds(kTargetPlayoutDelayMs),
        false /* is_remote_playback */, absl::nullopt /** refresh_interval */,
        false /** force_letterboxing */);
    host_->Start(std::move(session_params), std::move(observer_remote),
                 std::move(channel_remote),
                 channel_to_service_.BindNewPipeAndPassReceiver());
  }

  // SessionObserver implementation
  void OnError(mirroring::mojom::SessionError error) override {
    FAIL() << "Encountered session error: " << error;
  }

  void DidStart() override {}
  void DidStop() override {}
  void LogInfoMessage(const std::string& message) override {}
  void LogErrorMessage(const std::string& message) override {}
  void OnSourceChanged() override {}

  // CastMessageChannel implementation (inbound).
  void OnMessage(mirroring::mojom::CastMessagePtr message) override {
    Json::CharReaderBuilder rb;
    auto reader = std::unique_ptr<Json::CharReader>(rb.newCharReader());
    Json::Value root;

    const std::string& d = message->json_format_data;
    ASSERT_TRUE(reader->parse(d.data(), d.data() + d.length(), &root, nullptr));

    // Currently we only handle OFFER messages.
    if (root["type"].asString() != "OFFER") {
      return;
    }

    auto streams = GetStreamsFromOffer(root);
    SetSharedConfigs(streams);
    SendAnswer(root, std::move(message), streams);
    // The shared configs have been set as part of building the answer,
    // so we start the receiver now before sending the ANSWER message.
    parent_->StartReceiver(receiver_endpoint_, shared_configs_);
  }

  const net::IPEndPoint& receiver_endpoint() const {
    return receiver_endpoint_;
  }

 private:
  std::string ToString(const std::array<uint8_t, 16>& key) {
    return std::string(reinterpret_cast<const char*>(key.data()), key.size());
  }

  // Returns AUDIO + VIDEO
  std::pair<openscreen::cast::Stream, openscreen::cast::Stream>
  GetStreamsFromOffer(const Json::Value& offer_message_body) {
    openscreen::cast::Offer offer;
    EXPECT_TRUE(
        openscreen::cast::Offer::TryParse(offer_message_body["offer"], &offer)
            .ok());
    EXPECT_LT(0u, offer.audio_streams.size());
    EXPECT_LT(0u, offer.video_streams.size());
    return std::make_pair(offer.audio_streams[0].stream,
                          offer.video_streams[0].stream);
  }

  void SetSharedConfigs(const std::pair<openscreen::cast::Stream,
                                        openscreen::cast::Stream>& streams) {
    const auto& audio = streams.first;
    const auto& video = streams.second;
    shared_configs_.audio.aes_key = ToString(audio.aes_key);
    shared_configs_.audio.aes_iv_mask = ToString(audio.aes_iv_mask);
    shared_configs_.audio.sender_ssrc = audio.ssrc;
    shared_configs_.audio.receiver_ssrc = audio.ssrc + 1;
    shared_configs_.video.aes_key = ToString(video.aes_key);
    shared_configs_.video.aes_iv_mask = ToString(video.aes_iv_mask);
    shared_configs_.video.sender_ssrc = video.ssrc;
    shared_configs_.video.receiver_ssrc = video.ssrc + 1;
  }

  void SendAnswer(const Json::Value& offer_message_body,
                  mirroring::mojom::CastMessagePtr offer_message,
                  const std::pair<openscreen::cast::Stream,
                                  openscreen::cast::Stream>& streams) {
    openscreen::cast::Answer answer;
    answer.udp_port = udp_port_;

    answer.ssrcs.push_back(streams.first.ssrc + 1);
    answer.send_indexes.push_back(streams.first.index);
    answer.ssrcs.push_back(streams.second.ssrc + 1);
    answer.send_indexes.push_back(streams.second.index);

    ASSERT_TRUE(answer.IsValid());

    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(wb.newStreamWriter());

    Json::Value answer_message_body;
    answer_message_body["type"] = "ANSWER";
    answer_message_body["result"] = "ok";
    answer_message_body["answer"] = answer.ToJson();
    answer_message_body["seqNum"] = offer_message_body["seqNum"];
    std::ostringstream ssb;
    writer->write(answer_message_body, &ssb);

    VLOG(1) << "Sending ANSWER";
    offer_message->json_format_data = ssb.str();
    channel_to_service_->OnMessage(std::move(offer_message));
  }

  mojo::Remote<mirroring::mojom::MirroringServiceHost> host_;
  mojo::Remote<mirroring::mojom::CastMessageChannel> channel_to_service_;
  mojo::Receiver<mirroring::mojom::SessionObserver> observer_receiver_{this};
  mojo::Receiver<mirroring::mojom::CastMessageChannel> channel_receiver_{this};

  net::IPEndPoint receiver_endpoint_;
  int udp_port_ = -1;
  SharedSenderReceiverConfigs shared_configs_;

  const raw_ptr<CastV2PerformanceTest> parent_;
  scoped_refptr<media::cast::StandaloneCastEnvironment> cast_environment_;
};
}  // namespace

IN_PROC_BROWSER_TEST_P(CastV2PerformanceTest, Performance) {
  net::IPEndPoint receiver_end_point = media::cast::test::GetFreeLocalPort();
  VLOG(1) << "Got local UDP endpoint for testing: "
          << receiver_end_point.ToString();

  // Create a proxy for the UDP packets that simulates certain network
  // environments.
  std::unique_ptr<media::cast::test::UDPProxy> udp_proxy;
  int udp_proxy_port = receiver_end_point.port();
  if (HasFlag(kProxyWifi) || HasFlag(kProxySlow) || HasFlag(kProxyBad)) {
    net::IPEndPoint proxy_end_point = media::cast::test::GetFreeLocalPort();
    if (HasFlag(kProxyWifi)) {
      udp_proxy = media::cast::test::UDPProxy::Create(
          proxy_end_point, receiver_end_point, media::cast::test::WifiNetwork(),
          media::cast::test::WifiNetwork(), nullptr);
    } else if (HasFlag(kProxySlow)) {
      udp_proxy = media::cast::test::UDPProxy::Create(
          proxy_end_point, receiver_end_point, media::cast::test::SlowNetwork(),
          media::cast::test::SlowNetwork(), nullptr);
    } else if (HasFlag(kProxyBad)) {
      udp_proxy = media::cast::test::UDPProxy::Create(
          proxy_end_point, receiver_end_point, media::cast::test::BadNetwork(),
          media::cast::test::BadNetwork(), nullptr);
    }

    VLOG(1) << "Setting UDP proxy port: " << udp_proxy_port;
    udp_proxy_port = proxy_end_point.port();
  }

  NavigateToTestPagePath(kTestPageLocation);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  TestTabMirroringSession session(this);
  session.Start(web_contents, receiver_end_point, udp_proxy_port);

  // Now that capture has started, start playing the barcode video in the test
  // page.
  content::SimulateMouseClick(web_contents, 0,
                              blink::WebMouseEvent::Button::kLeft);

  // Observe the running browser for a while, collecting a trace.
  TraceAnalyzerUniquePtr analyzer = TraceAndObserve(
      is_full_performance_run_, "gpu.capture,cast_perf_test",
      std::vector<base::StringPiece>{
          // From the Compositor/Capture pipeline...
          "Capture", "OnBufferReceived", "ConsumeVideoFrame",
          // From the Cast Sender's pipeline...
          "InsertRawVideoFrame", "VideoFrameEncoded",
          // From the Cast Receiver's pipeline...
          "PullEncodedVideoFrame", "VideoFrameDecoded",
          // From the TestPatternReceiver (see class above!)...
          "VideoFramePlayout", "AudioFramePlayout"},
      // In a full performance run, events will be trimmed from both ends of
      // trace. Otherwise, just require the bare-minimum to verify the stats
      // calculations will work.
      is_full_performance_run_ ? (2 * kTrimEvents + kMinDataPointsForFullRun)
                               : kMinDataPointsForQuickRun);

  // Shut down the receiver and all the CastEnvironment threads.
  VLOG(1) << "Shutting-down receiver and CastEnvironment...";
  receiver_->Stop();
  cast_environment_->Shutdown();

  VLOG(1) << "Analyzing trace events...";
  // This prints out the average time between capture events.
  // Depending on the test, the capture frame rate is capped (e.g., at 30fps,
  // this score cannot get any better than 33.33 ms). However, the measurement
  // is important since it provides a valuable check that capture can keep up
  // with the content's framerate.
  // Lower is better.
  auto reporter = SetUpCastV2Reporter(GetSuffixForTestFlags());
  MaybeAddResultList(reporter, kMetricTimeBetweenCapturesMs,
                     AnalyzeTraceDistance(analyzer.get(), "Capture"));

  receiver_->Analyze(GetSuffixForTestFlags());

  AnalyzeLatency(analyzer.get());
}

#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
// Skip test on Chrome OS MSAN.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CastV2PerformanceTest);
#else
// TODO(b/161545049): reenable FPS value features.
INSTANTIATE_TEST_SUITE_P(All,
                         CastV2PerformanceTest,
                         testing::Values(kSmallWindow,
                                         kProxyWifi,
                                         kProxyBad,
                                         kSlowClock,
                                         kFastClock,
                                         kProxySlow));
#endif
