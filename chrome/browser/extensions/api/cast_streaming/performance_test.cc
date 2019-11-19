// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_performance_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/tracing.h"
#include "content/public/common/content_switches.h"
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
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_server_socket.h"
#include "testing/perf/perf_result_reporter.h"

namespace {

// Number of events to trim from the begining and end. These events don't
// contribute anything toward stable measurements: A brief moment of startup
// "jank" is acceptable, and shutdown may result in missing events (e.g., if
// streaming stops a few frames before capture stops).
constexpr int kTrimEvents = 24;  // 1 sec at 24fps, or 0.4 sec at 60 fps.

// Minimum number of events required for a reasonable analysis.
constexpr int kMinDataPointsForFullRun = 100;  // 1s of audio, ~5s at 24fps.

// Minimum number of events required for data analysis in a non-performance run.
constexpr int kMinDataPointsForQuickRun = 3;

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
#define EXPECT_FOR_PERFORMANCE_RUN(expr)             \
  if (!(expr)) {                                     \
    const char *_out = #expr;                        \
    if (is_full_performance_run()) {                 \
      LOG(ERROR) << "Failure: " << _out;             \
    } else {                                         \
      LOG(WARNING) << "Allowing failure: " << _out;  \
    }                                                \
  }

enum TestFlags {
  kSmallWindow = 1 << 2,      // Window size: 1 = 800x600, 0 = 2000x1000
  k24fps = 1 << 3,            // Use 24 fps video.
  k30fps = 1 << 4,            // Use 30 fps video.
  k60fps = 1 << 5,            // Use 60 fps video (captured at 30 fps).
  kProxyWifi = 1 << 6,        // Run UDP through UDPProxy wifi profile.
  kProxySlow = 1 << 7,        // Run UDP through UDPProxy slow profile.
  kProxyBad = 1 << 8,         // Run UDP through UDPProxy bad profile.
  kSlowClock = 1 << 9,        // Receiver clock is 10 seconds slow.
  kFastClock = 1 << 10,       // Receiver clock is 10 seconds fast.
  kAutoThrottling = 1 << 11,  // Use auto-resolution/framerate throttling.
};

// These are just for testing! Use cryptographically-secure random keys in
// production code!
static constexpr char kAesKey[16] = {0, 1, 2,  3,  4,  5,  6,  7,
                                     8, 9, 10, 11, 12, 13, 14, 15};
static constexpr char kAesIvMask[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                        7,  6,  5,  4,  3,  2,  1, 0};

media::cast::FrameReceiverConfig WithAesKeyAndIvSet(
    const media::cast::FrameReceiverConfig& config) {
  media::cast::FrameReceiverConfig result = config;
  result.aes_key = std::string(kAesKey, kAesKey + sizeof(kAesKey));
  result.aes_iv_mask = std::string(kAesIvMask, kAesIvMask + sizeof(kAesIvMask));
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
      const double skew = delta < base::TimeDelta() ? 0.999950 : 1.000050;
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
      bool is_full_performance_run)
      : InProcessReceiver(
            cast_environment,
            local_end_point,
            net::IPEndPoint(),
            WithAesKeyAndIvSet(media::cast::GetDefaultAudioReceiverConfig()),
            WithAesKeyAndIvSet(media::cast::GetDefaultVideoReceiverConfig())),
        is_full_performance_run_(is_full_performance_run) {}

  typedef std::map<uint16_t, base::TimeTicks> TimeMap;

  bool is_full_performance_run() const { return is_full_performance_run_; }

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
         i != audio_frame_times.end();
         ++i) {
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
                                     audio_frame->frames(),
                                     &frame_no)) {
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

  DISALLOW_COPY_AND_ASSIGN(TestPatternReceiver);
};

class CastV2PerformanceTest : public TabCapturePerformanceTestBase,
                              public testing::WithParamInterface<int> {
 public:
  CastV2PerformanceTest() = default;
  ~CastV2PerformanceTest() override = default;

  bool HasFlag(TestFlags flag) const {
    return (GetParam() & flag) == flag;
  }

  std::string GetSuffixForTestFlags() const {
    std::string suffix;
    // Note: Add "_gpu" tag for backwards-compatibility with existing
    // Performance Dashboard timeseries data.
    suffix += "gpu";
    if (HasFlag(kSmallWindow))
      suffix += "_small";
    if (HasFlag(k24fps))
      suffix += "_24fps";
    if (HasFlag(k30fps))
      suffix += "_30fps";
    if (HasFlag(k60fps))
      suffix += "_60fps";
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
    if (HasFlag(kAutoThrottling))
      suffix += "_autothrottling";
    return suffix;
  }

  int get_fps() const {
    if (HasFlag(k24fps))
      return 24;
    if (HasFlag(k30fps))
      return 30;
    if (HasFlag(k60fps))
      return 60;
    NOTREACHED();
    return 0;
  }

  void SetUp() override {
    // Produce the full HTML test page with the barcode video embedded within
    // (as a data URI).
    const base::FilePath video_file =
        GetApiTestDataDir()
            .AppendASCII("cast_streaming")
            .AppendASCII(
                base::StringPrintf("test_video_%dfps.webm", get_fps()));
    std::string file_contents;
    const bool success = base::ReadFileToString(video_file, &file_contents);
    CHECK(success) << "Failed to load video at: " << video_file.AsUTF8Unsafe();
    std::string video_in_base64;
    base::Base64Encode(file_contents, &video_in_base64);
    test_page_html_ =
        base::StrCat({"<html><body>\n"
                      "<video width='100%' height='100%'>\n"
                      "  <source src='data:video/webm;base64,",
                      video_in_base64,
                      "'>\n"
                      "</video>\n"
                      "</body></html>"});

    TabCapturePerformanceTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (HasFlag(kSmallWindow)) {
      command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");
    } else {
      command_line->AppendSwitchASCII(switches::kWindowSize, "2000,1500");
    }

    TabCapturePerformanceTestBase::SetUpCommandLine(command_line);
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
           j != events[i]->arg_numbers.end();
           ++j) {
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
    const int trim_count = is_full_performance_run() ? kTrimEvents : 0;
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
    EXPECT_FOR_PERFORMANCE_RUN((is_full_performance_run()
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

    const int trim_count = is_full_performance_run() ? kTrimEvents : 0;
    std::vector<double> deltas;
    for (int i = trim_count + 1;
         i < static_cast<int>(events.size()) - trim_count; ++i) {
      double delta_micros = events[i]->timestamp - events[i - 1]->timestamp;
      deltas.push_back(delta_micros / 1000.0);
    }
    return deltas;
  }

 protected:
  // The complete HTML test web page without any external dependencies,
  // including the entire barcode video as an embedded data URI. Populated in
  // SetUp().
  std::string test_page_html_;

  // While the source video frame rate may vary (24, 30, or 60 FPS), the maximum
  // capture frame rate is always fixed at 30 FPS. This allows testing of the
  // entire system when it is forced to perform a 60→30 frame rate conversion.
  static constexpr int kMaxCaptureFrameRate = 30;
};

}  // namespace

// TODO(https://crbug.com/974427) Disabled due to flakiness.
IN_PROC_BROWSER_TEST_P(CastV2PerformanceTest, DISABLED_Performance) {
  net::IPEndPoint receiver_end_point = media::cast::test::GetFreeLocalPort();
  VLOG(1) << "Got local UDP port for testing: "
          << receiver_end_point.ToString();

  // Start the in-process receiver that examines audio/video for the expected
  // test patterns.
  base::TimeDelta delta = base::TimeDelta::FromSeconds(0);
  if (HasFlag(kFastClock)) {
    delta = base::TimeDelta::FromSeconds(10);
  }
  if (HasFlag(kSlowClock)) {
    delta = base::TimeDelta::FromSeconds(-10);
  }
  scoped_refptr<media::cast::StandaloneCastEnvironment> cast_environment(
      new SkewedCastEnvironment(delta));
  auto receiver = std::make_unique<TestPatternReceiver>(
      cast_environment, receiver_end_point, is_full_performance_run());
  receiver->Start();

  // Create a proxy for the UDP packets that simulates certain network
  // environments.
  std::unique_ptr<media::cast::test::UDPProxy> udp_proxy;
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
    receiver_end_point = proxy_end_point;
  }

  // Load the extension and test page, and tell the extension to start tab
  // capture + Cast Streaming.
  LoadExtension(GetApiTestDataDir()
                    .AppendASCII("cast_streaming")
                    .AppendASCII("perftest_extension"));
  NavigateToTestPage(test_page_html_);
  const base::Value response = SendMessageToExtension(base::StringPrintf(
      "{start:true, enableAutoThrottling:%s, maxFrameRate:%d, recvPort:%d,"
      " aesKey:'%s', aesIvMask:'%s'}",
      HasFlag(kAutoThrottling) ? "true" : "false", kMaxCaptureFrameRate,
      receiver_end_point.port(),
      base::HexEncode(kAesKey, sizeof(kAesKey)).c_str(),
      base::HexEncode(kAesIvMask, sizeof(kAesIvMask)).c_str()));
  const std::string* reason = response.FindStringKey("reason");
  ASSERT_TRUE(response.FindBoolKey("success").value_or(false))
      << (reason ? *reason : std::string("<MISSING REASON>"));

  // Now that capture has started, start playing the barcode video in the test
  // page.
  const std::string javascript_to_play_video(
      "new Promise((resolve) => {\n"
      "  const video = document.getElementsByTagName('video')[0];\n"
      "  video.addEventListener('playing', () => { resolve(true); });\n"
      "  video.play();\n"
      "})");
  LOG(INFO) << "Starting playback of barcode video...";
  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      javascript_to_play_video));

  // Observe the running browser for a while, collecting a trace.
  TraceAnalyzerUniquePtr analyzer = TraceAndObserve(
      "gpu.capture,cast_perf_test",
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
      is_full_performance_run() ? (2 * kTrimEvents + kMinDataPointsForFullRun)
                                : kMinDataPointsForQuickRun);

  // Shut down the receiver and all the CastEnvironment threads.
  VLOG(1) << "Shutting-down receiver and CastEnvironment...";
  receiver->Stop();
  cast_environment->Shutdown();

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

  receiver->Analyze(GetSuffixForTestFlags());

  AnalyzeLatency(analyzer.get());
}

#if !defined(OS_CHROMEOS) || !defined(MEMORY_SANITIZER)
INSTANTIATE_TEST_SUITE_P(,
                         CastV2PerformanceTest,
                         testing::Values(k24fps,
                                         k30fps,
                                         k60fps,
                                         k30fps | kProxyWifi,
                                         k30fps | kProxyBad,
                                         k30fps | kSlowClock,
                                         k30fps | kFastClock,
                                         k30fps | kProxyWifi | kAutoThrottling,
                                         k30fps | kProxySlow | kAutoThrottling,
                                         k30fps | kProxyBad | kAutoThrottling));
#endif
