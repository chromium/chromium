// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_browsertest_perf.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/perf/perf_test.h"

namespace {

std::string Statistic(const std::string& statistic, const std::string& bucket) {
  // A ssrc stats key will be on the form stats.<bucket>-<key>.values.
  // This will give a json "path" which will dig into the time series for the
  // specified statistic. Buckets can be for instance ssrc_1212344, bweforvideo,
  // and will each contain a bunch of statistics relevant to their nature.
  // Each peer connection has a number of such buckets.
  return base::StringPrintf("stats.%s-%s.values", bucket.c_str(),
                            statistic.c_str());
}

void MaybePrintResultsForAudioReceive(const std::string& ssrc,
                                      const base::Value::Dict& pc_dict,
                                      const std::string& modifier) {
  const std::string* value =
      pc_dict.FindStringByDottedPath(Statistic("audioOutputLevel", ssrc));
  if (!value) {
    // Not an audio receive stream.
    return;
  }

  value = pc_dict.FindStringByDottedPath(Statistic("packetsLost", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("audio_misc", modifier, "packets_lost", *value,
                         "frames", false);
  value = pc_dict.FindStringByDottedPath(Statistic("googJitterReceived", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("audio_rx", modifier, "goog_jitter_recv", *value, "ms",
                         false);

  value = pc_dict.FindStringByDottedPath(Statistic("googExpandRate", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("audio_rates", modifier, "goog_expand_rate", *value,
                         "%", false);
  value =
      pc_dict.FindStringByDottedPath(Statistic("googSpeechExpandRate", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("audio_rates", modifier, "goog_speech_expand_rate",
                         *value, "%", false);
  value = pc_dict.FindStringByDottedPath(
      Statistic("googSecondaryDecodedRate", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("audio_rates", modifier, "goog_secondary_decoded_rate",
                         *value, "%", false);
}

void MaybePrintResultsForAudioSend(const std::string& ssrc,
                                   const base::Value::Dict& pc_dict,
                                   const std::string& modifier) {
  const std::string* value =
      pc_dict.FindStringByDottedPath(Statistic("audioInputLevel", ssrc));
  if (!value) {
    // Not an audio send stream.
    return;
  }

  value = pc_dict.FindStringByDottedPath(Statistic("googJitterReceived", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("audio_tx", modifier, "goog_jitter_recv", *value, "ms",
                         false);
  value = pc_dict.FindStringByDottedPath(Statistic("googRtt", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("audio_tx", modifier, "goog_rtt", *value, "ms", false);
  value =
      pc_dict.FindStringByDottedPath(Statistic("packetsSentPerSecond", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("audio_tx", modifier, "packets_sent_per_second",
                         *value, "packets", false);
}

void MaybePrintResultsForVideoSend(const std::string& ssrc,
                                   const base::Value::Dict& pc_dict,
                                   const std::string& modifier) {
  const std::string* value =
      pc_dict.FindStringByDottedPath(Statistic("googFrameRateSent", ssrc));
  if (!value) {
    // Not a video send stream.
    return;
  }

  // Graph these by unit: the dashboard expects all stats in one graph to have
  // the same unit (e.g. ms, fps, etc). Most graphs, like video_fps, will also
  // be populated by the counterparts on the video receiving side.
  perf_test::PrintResult("video_fps", modifier, "goog_frame_rate_sent", *value,
                         "fps", false);
  value = pc_dict.FindStringByDottedPath(Statistic("googFrameRateInput", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_fps", modifier, "goog_frame_rate_input", *value,
                         "fps", false);

  value = pc_dict.FindStringByDottedPath(Statistic("googFirsReceived", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_misc", modifier, "goog_firs_recv", *value, "",
                         false);
  value = pc_dict.FindStringByDottedPath(Statistic("googNacksReceived", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_misc", modifier, "goog_nacks_recv", *value, "",
                         false);

  value = pc_dict.FindStringByDottedPath(Statistic("googFrameWidthSent", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_resolution", modifier, "goog_frame_width_sent",
                         *value, "pixels", false);
  value =
      pc_dict.FindStringByDottedPath(Statistic("googFrameHeightSent", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_resolution", modifier, "goog_frame_height_sent",
                         *value, "pixels", false);

  value = pc_dict.FindStringByDottedPath(Statistic("googAvgEncodeMs", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_tx", modifier, "goog_avg_encode_ms", *value,
                         "ms", false);
  value = pc_dict.FindStringByDottedPath(Statistic("googRtt", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_tx", modifier, "goog_rtt", *value, "ms", false);

  value =
      pc_dict.FindStringByDottedPath(Statistic("googEncodeUsagePercent", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_cpu_usage", modifier,
                         "goog_encode_usage_percent", *value, "%", false);
}

void MaybePrintResultsForVideoReceive(const std::string& ssrc,
                                      const base::Value::Dict& pc_dict,
                                      const std::string& modifier) {
  const std::string* value =
      pc_dict.FindStringByDottedPath(Statistic("googFrameRateReceived", ssrc));
  if (!value) {
    // Not a video receive stream.
    return;
  }

  perf_test::PrintResult("video_fps", modifier, "goog_frame_rate_recv", *value,
                         "fps", false);
  value =
      pc_dict.FindStringByDottedPath(Statistic("googFrameRateOutput", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_fps", modifier, "goog_frame_rate_output",
                         *value, "fps", false);

  value = pc_dict.FindStringByDottedPath(Statistic("packetsLost", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_misc", modifier, "packets_lost", *value,
                         "frames", false);

  value =
      pc_dict.FindStringByDottedPath(Statistic("googFrameWidthReceived", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_resolution", modifier, "goog_frame_width_recv",
                         *value, "pixels", false);
  value = pc_dict.FindStringByDottedPath(
      Statistic("googFrameHeightReceived", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_resolution", modifier, "goog_frame_height_recv",
                         *value, "pixels", false);

  value = pc_dict.FindStringByDottedPath(Statistic("googCurrentDelayMs", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_rx", modifier, "goog_current_delay_ms", *value,
                         "ms", false);
  value = pc_dict.FindStringByDottedPath(Statistic("googTargetDelayMs", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_rx", modifier, "goog_target_delay_ms", *value,
                         "ms", false);
  value = pc_dict.FindStringByDottedPath(Statistic("googDecodeMs", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_rx", modifier, "goog_decode_ms", *value, "ms",
                         false);
  value = pc_dict.FindStringByDottedPath(Statistic("googMaxDecodeMs", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_rx", modifier, "goog_max_decode_ms", *value,
                         "ms", false);
  value = pc_dict.FindStringByDottedPath(Statistic("googJitterBufferMs", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_rx", modifier, "goog_jitter_buffer_ms", *value,
                         "ms", false);
  value = pc_dict.FindStringByDottedPath(Statistic("googRenderDelayMs", ssrc));
  ASSERT_TRUE(value);
  perf_test::PrintResult("video_rx", modifier, "goog_render_delay_ms", *value,
                         "ms", false);
}

std::string ExtractSsrcIdentifier(const std::string& key) {
  // Example key: ssrc_1234-someStatName. Grab the part before the dash.
  size_t key_start_pos = 0;
  size_t key_end_pos = key.find("-");
  CHECK(key_end_pos != std::string::npos) << "Could not parse key " << key;
  return key.substr(key_start_pos, key_end_pos - key_start_pos);
}

// Returns the set of unique ssrc identifiers in the call (e.g. ssrc_1234,
// ssrc_12356, etc). |stats_dict| is the .stats dict from one peer connection.
std::set<std::string> FindAllSsrcIdentifiers(
    const base::Value::Dict& stats_dict) {
  std::set<std::string> result;
  for (auto kv : stats_dict) {
    if (kv.first.find("ssrc_") != std::string::npos)
      result.insert(ExtractSsrcIdentifier(kv.first));
  }
  return result;
}

}  // namespace

namespace test {

void PrintBweForVideoMetrics(const base::Value::Dict& pc_dict,
                             const std::string& modifier,
                             const std::string& video_codec) {
  std::string video_modifier =
      video_codec.empty() ? modifier : modifier + "_" + video_codec;
  const std::string kBweStatsKey = "bweforvideo";
  const std::string* value = pc_dict.FindStringByDottedPath(
      Statistic("googAvailableSendBandwidth", kBweStatsKey));
  ASSERT_TRUE(value);
  perf_test::PrintResult("bwe_stats", video_modifier, "available_send_bw",
                         *value, "bit/s", false);
  value = pc_dict.FindStringByDottedPath(
      Statistic("googAvailableReceiveBandwidth", kBweStatsKey));
  ASSERT_TRUE(value);
  perf_test::PrintResult("bwe_stats", video_modifier, "available_recv_bw",
                         *value, "bit/s", false);
  value = pc_dict.FindStringByDottedPath(
      Statistic("googTargetEncBitrate", kBweStatsKey));
  ASSERT_TRUE(value);
  perf_test::PrintResult("bwe_stats", video_modifier, "target_enc_bitrate",
                         *value, "bit/s", false);
  value = pc_dict.FindStringByDottedPath(
      Statistic("googActualEncBitrate", kBweStatsKey));
  ASSERT_TRUE(value);
  perf_test::PrintResult("bwe_stats", video_modifier, "actual_enc_bitrate",
                         *value, "bit/s", false);
  value = pc_dict.FindStringByDottedPath(
      Statistic("googTransmitBitrate", kBweStatsKey));
  ASSERT_TRUE(value);
  perf_test::PrintResult("bwe_stats", video_modifier, "transmit_bitrate",
                         *value, "bit/s", false);
}

void PrintMetricsForAllStreams(const base::Value::Dict& pc_dict,
                               const std::string& modifier,
                               const std::string& video_codec) {
  PrintMetricsForSendStreams(pc_dict, modifier, video_codec);
  PrintMetricsForRecvStreams(pc_dict, modifier, video_codec);
}

void PrintMetricsForSendStreams(const base::Value::Dict& pc_dict,
                                const std::string& modifier,
                                const std::string& video_codec) {
  std::string video_modifier =
      video_codec.empty() ? modifier : modifier + "_" + video_codec;
  const base::Value::Dict* stats_dict = pc_dict.FindDict("stats");
  ASSERT_TRUE(stats_dict);
  std::set<std::string> ssrc_identifiers = FindAllSsrcIdentifiers(*stats_dict);

  auto ssrc_iterator = ssrc_identifiers.begin();
  for (; ssrc_iterator != ssrc_identifiers.end(); ++ssrc_iterator) {
    const std::string& ssrc = *ssrc_iterator;
    MaybePrintResultsForAudioSend(ssrc, pc_dict, modifier);
    MaybePrintResultsForVideoSend(ssrc, pc_dict, video_modifier);
  }
}

void PrintMetricsForRecvStreams(const base::Value::Dict& pc_dict,
                                const std::string& modifier,
                                const std::string& video_codec) {
  std::string video_modifier =
      video_codec.empty() ? modifier : modifier + "_" + video_codec;
  const base::Value::Dict* stats_dict = pc_dict.FindDict("stats");
  ASSERT_TRUE(stats_dict);
  std::set<std::string> ssrc_identifiers = FindAllSsrcIdentifiers(*stats_dict);

  auto ssrc_iterator = ssrc_identifiers.begin();
  for (; ssrc_iterator != ssrc_identifiers.end(); ++ssrc_iterator) {
    const std::string& ssrc = *ssrc_iterator;
    MaybePrintResultsForAudioReceive(ssrc, pc_dict, modifier);
    MaybePrintResultsForVideoReceive(ssrc, pc_dict, video_modifier);
  }
}

constexpr char kMetricPrefixVideoQualityTest[] =
    "WebRtcVideoQualityBrowserTest.";
constexpr char kMetricUniqueFramesCount[] = "Unique_frames_count";
constexpr char kMetricPsnrUnitless[] = "PSNR";
constexpr char kMetricSsimUnitless[] = "SSIM";
constexpr char kMetricMaxRepeatedCount[] = "Max_repeated";
constexpr char kMetricMaxSkippedCount[] = "Max_skipped";
constexpr char kMetricTotalSkippedCount[] = "Total_skipped";
constexpr char kMetricDecodeErrorsReferenceCount[] = "Decode_errors_reference";
constexpr char kMetricDecodeErrorsTestCount[] = "Decode_errors_test";

bool ParseDoubleFromOutput(const std::string& line,
                           const char* expected_label,
                           double* value) {
  auto fields = base::SplitString(line, " ", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
  auto actual_label = std::string(expected_label) + ":";
  if (fields.size() < 4 || fields[1] != actual_label) {
    LOG(ERROR) << "Expected line with " << actual_label
               << " and four or five space-separated fields, got " << line;
    return false;
  }
  if (!base::StringToDouble(fields[3], value)) {
    LOG(ERROR) << "Expected " << fields[3] << " to be an int";
    return false;
  }
  return true;
}

bool ParseListFromOutput(const std::string& line,
                         const char* expected_label,
                         std::string* value) {
  auto fields = base::SplitString(line, " ", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
  auto actual_label = std::string(expected_label) + ":";
  if (fields.size() < 4 || fields[1] != actual_label) {
    LOG(ERROR) << "Expected line with " << actual_label
               << " and four or five space-separated fields, got " << line;
    return false;
  }

  // Strip out [].
  std::string values = fields[3];
  if (values.length() < 2) {
    LOG(ERROR) << "Malformed values, expected [val1, val2], got " << values;
    return false;
  }
  *value = values.substr(1, values.length() - 2);
  return true;
}

bool WriteCompareVideosOutputAsHistogram(const std::string& test_label,
                                         const std::string& output) {
  perf_test::PerfResultReporter reporter(kMetricPrefixVideoQualityTest,
                                         test_label);

  reporter.RegisterFyiMetric(kMetricUniqueFramesCount, "count");
  reporter.RegisterFyiMetric(kMetricPsnrUnitless, "unitless");
  reporter.RegisterFyiMetric(kMetricSsimUnitless, "unitless");
  reporter.RegisterFyiMetric(kMetricMaxRepeatedCount, "count");
  reporter.RegisterFyiMetric(kMetricMaxSkippedCount, "count");
  reporter.RegisterFyiMetric(kMetricTotalSkippedCount, "count");
  reporter.RegisterFyiMetric(kMetricDecodeErrorsReferenceCount, "count");
  reporter.RegisterFyiMetric(kMetricDecodeErrorsTestCount, "count");
  auto lines = base::SplitString(output, "\n", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);

  if (lines.size() == 12) {
    // Remove warning about colorspace conversion.
    lines.erase(lines.begin(), lines.begin() + 4);
  }

  if (lines.size() != 8) {
    LOG(ERROR) << "Expected 8 lines, got " << lines.size()
               << " lines. Output:\n\n"
               << output;
    return false;
  }

  double value;
  if (!ParseDoubleFromOutput(lines[0], kMetricUniqueFramesCount, &value))
    return false;
  reporter.AddResult(kMetricUniqueFramesCount, value);
  std::string value_list;
  if (!ParseListFromOutput(lines[1], kMetricPsnrUnitless, &value_list))
    return false;
  reporter.AddResultList(kMetricPsnrUnitless, value_list);
  if (!ParseListFromOutput(lines[2], kMetricSsimUnitless, &value_list))
    return false;
  reporter.AddResultList(kMetricSsimUnitless, value_list);
  if (!ParseDoubleFromOutput(lines[3], kMetricMaxRepeatedCount, &value))
    return false;
  reporter.AddResult(kMetricMaxRepeatedCount, value);
  if (!ParseDoubleFromOutput(lines[4], kMetricMaxSkippedCount, &value))
    return false;
  reporter.AddResult(kMetricMaxSkippedCount, value);
  if (!ParseDoubleFromOutput(lines[5], kMetricTotalSkippedCount, &value))
    return false;
  reporter.AddResult(kMetricTotalSkippedCount, value);
  if (!ParseDoubleFromOutput(lines[6], kMetricDecodeErrorsReferenceCount,
                             &value))
    return false;
  reporter.AddResult(kMetricDecodeErrorsReferenceCount, value);
  if (!ParseDoubleFromOutput(lines[7], kMetricDecodeErrorsTestCount, &value))
    return false;
  reporter.AddResult(kMetricDecodeErrorsTestCount, value);
  return true;
}

}  // namespace test
