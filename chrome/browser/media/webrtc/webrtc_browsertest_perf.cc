// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_browsertest_perf.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/perf/perf_test.h"

static std::string Statistic(const std::string& statistic,
                             const std::string& bucket) {
  // A ssrc stats key will be on the form stats.<bucket>-<key>.values.
  // This will give a json "path" which will dig into the time series for the
  // specified statistic. Buckets can be for instance ssrc_1212344, bweforvideo,
  // and will each contain a bunch of statistics relevant to their nature.
  // Each peer connection has a number of such buckets.
  return base::StringPrintf("stats.%s-%s.values", bucket.c_str(),
                            statistic.c_str());
}

static void MaybePrintResultsForAudioReceive(
    const std::string& ssrc, const base::DictionaryValue& pc_dict,
    const std::string& modifier) {
  std::string value;
  if (!pc_dict.GetString(Statistic("audioOutputLevel", ssrc), &value)) {
    // Not an audio receive stream.
    return;
  }

  EXPECT_TRUE(pc_dict.GetString(Statistic("packetsLost", ssrc), &value));
  perf_test::PrintResult(
      "audio_misc", modifier, "packets_lost", value, "frames", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googJitterReceived", ssrc), &value));
  perf_test::PrintResult(
      "audio_rx", modifier, "goog_jitter_recv", value, "ms", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googExpandRate", ssrc), &value));
  perf_test::PrintResult(
      "audio_rates", modifier, "goog_expand_rate", value, "%", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googSpeechExpandRate", ssrc), &value));
  perf_test::PrintResult(
      "audio_rates", modifier, "goog_speech_expand_rate", value, "%", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googSecondaryDecodedRate", ssrc), &value));
  perf_test::PrintResult(
      "audio_rates", modifier, "goog_secondary_decoded_rate", value, "%",
      false);
}

static void MaybePrintResultsForAudioSend(
    const std::string& ssrc, const base::DictionaryValue& pc_dict,
    const std::string& modifier) {
  std::string value;
  if (!pc_dict.GetString(Statistic("audioInputLevel", ssrc), &value)) {
    // Not an audio send stream.
    return;
  }

  EXPECT_TRUE(pc_dict.GetString(Statistic("googJitterReceived", ssrc), &value));
  perf_test::PrintResult(
      "audio_tx", modifier, "goog_jitter_recv", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRtt", ssrc), &value));
  perf_test::PrintResult(
      "audio_tx", modifier, "goog_rtt", value, "ms", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("packetsSentPerSecond", ssrc), &value));
  perf_test::PrintResult("audio_tx", modifier, "packets_sent_per_second", value,
                         "packets", false);
}

static void MaybePrintResultsForVideoSend(
    const std::string& ssrc, const base::DictionaryValue& pc_dict,
    const std::string& modifier) {
  std::string value;
  if (!pc_dict.GetString(Statistic("googFrameRateSent", ssrc), &value)) {
    // Not a video send stream.
    return;
  }

  // Graph these by unit: the dashboard expects all stats in one graph to have
  // the same unit (e.g. ms, fps, etc). Most graphs, like video_fps, will also
  // be populated by the counterparts on the video receiving side.
  perf_test::PrintResult(
      "video_fps", modifier, "goog_frame_rate_sent", value, "fps", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googFrameRateInput", ssrc), &value));
  perf_test::PrintResult(
      "video_fps", modifier, "goog_frame_rate_input", value, "fps", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googFirsReceived", ssrc), &value));
  perf_test::PrintResult(
      "video_misc", modifier, "goog_firs_recv", value, "", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googNacksReceived", ssrc), &value));
  perf_test::PrintResult(
      "video_misc", modifier, "goog_nacks_recv", value, "", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googFrameWidthSent", ssrc), &value));
  perf_test::PrintResult("video_resolution", modifier, "goog_frame_width_sent",
                         value, "pixels", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameHeightSent", ssrc), &value));
  perf_test::PrintResult("video_resolution", modifier, "goog_frame_height_sent",
                         value, "pixels", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googAvgEncodeMs", ssrc), &value));
  perf_test::PrintResult(
      "video_tx", modifier, "goog_avg_encode_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRtt", ssrc), &value));
  perf_test::PrintResult("video_tx", modifier, "goog_rtt", value, "ms", false);

  EXPECT_TRUE(pc_dict.GetString(
      Statistic("googEncodeUsagePercent", ssrc), &value));
  perf_test::PrintResult("video_cpu_usage", modifier,
                         "goog_encode_usage_percent", value, "%", false);
}

static void MaybePrintResultsForVideoReceive(
    const std::string& ssrc, const base::DictionaryValue& pc_dict,
    const std::string& modifier) {
  std::string value;
  if (!pc_dict.GetString(Statistic("googFrameRateReceived", ssrc), &value)) {
    // Not a video receive stream.
    return;
  }

  perf_test::PrintResult(
      "video_fps", modifier, "goog_frame_rate_recv", value, "fps", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameRateOutput", ssrc), &value));
  perf_test::PrintResult(
      "video_fps", modifier, "goog_frame_rate_output", value, "fps", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("packetsLost", ssrc), &value));
  perf_test::PrintResult("video_misc", modifier, "packets_lost", value,
                         "frames", false);

  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameWidthReceived", ssrc), &value));
  perf_test::PrintResult("video_resolution", modifier, "goog_frame_width_recv",
                         value, "pixels", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameHeightReceived", ssrc), &value));
  perf_test::PrintResult("video_resolution", modifier, "goog_frame_height_recv",
                         value, "pixels", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googCurrentDelayMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", modifier, "goog_current_delay_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googTargetDelayMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", modifier, "goog_target_delay_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googDecodeMs", ssrc), &value));
  perf_test::PrintResult("video_rx", modifier, "goog_decode_ms", value, "ms",
                         false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googMaxDecodeMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", modifier, "goog_max_decode_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googJitterBufferMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", modifier, "goog_jitter_buffer_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRenderDelayMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", modifier, "goog_render_delay_ms", value, "ms", false);
}

static std::string ExtractSsrcIdentifier(const std::string& key) {
  // Example key: ssrc_1234-someStatName. Grab the part before the dash.
  size_t key_start_pos = 0;
  size_t key_end_pos = key.find("-");
  CHECK(key_end_pos != std::string::npos) << "Could not parse key " << key;
  return key.substr(key_start_pos, key_end_pos - key_start_pos);
}

// Returns the set of unique ssrc identifiers in the call (e.g. ssrc_1234,
// ssrc_12356, etc). |stats_dict| is the .stats dict from one peer connection.
static std::set<std::string> FindAllSsrcIdentifiers(
    const base::DictionaryValue& stats_dict) {
  std::set<std::string> result;
  base::DictionaryValue::Iterator stats_iterator(stats_dict);

  while (!stats_iterator.IsAtEnd()) {
    if (stats_iterator.key().find("ssrc_") != std::string::npos)
      result.insert(ExtractSsrcIdentifier(stats_iterator.key()));
    stats_iterator.Advance();
  }
  return result;
}

namespace test {

void PrintBweForVideoMetrics(const base::DictionaryValue& pc_dict,
                             const std::string& modifier,
                             const std::string& video_codec) {
  std::string video_modifier =
      video_codec.empty() ? modifier : modifier + "_" + video_codec;
  const std::string kBweStatsKey = "bweforvideo";
  std::string value;
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googAvailableSendBandwidth", kBweStatsKey), &value));
  perf_test::PrintResult("bwe_stats", video_modifier, "available_send_bw",
                         value, "bit/s", false);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googAvailableReceiveBandwidth", kBweStatsKey), &value));
  perf_test::PrintResult("bwe_stats", video_modifier, "available_recv_bw",
                         value, "bit/s", false);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googTargetEncBitrate", kBweStatsKey), &value));
  perf_test::PrintResult("bwe_stats", video_modifier, "target_enc_bitrate",
                         value, "bit/s", false);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googActualEncBitrate", kBweStatsKey), &value));
  perf_test::PrintResult("bwe_stats", video_modifier, "actual_enc_bitrate",
                         value, "bit/s", false);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googTransmitBitrate", kBweStatsKey), &value));
  perf_test::PrintResult("bwe_stats", video_modifier, "transmit_bitrate", value,
                         "bit/s", false);
}

void PrintMetricsForAllStreams(const base::DictionaryValue& pc_dict,
                               const std::string& modifier,
                               const std::string& video_codec) {
  PrintMetricsForSendStreams(pc_dict, modifier, video_codec);
  PrintMetricsForRecvStreams(pc_dict, modifier, video_codec);
}

void PrintMetricsForSendStreams(const base::DictionaryValue& pc_dict,
                                const std::string& modifier,
                                const std::string& video_codec) {
  std::string video_modifier =
      video_codec.empty() ? modifier : modifier + "_" + video_codec;
  const base::DictionaryValue* stats_dict;
  ASSERT_TRUE(pc_dict.GetDictionary("stats", &stats_dict));
  std::set<std::string> ssrc_identifiers = FindAllSsrcIdentifiers(*stats_dict);

  auto ssrc_iterator = ssrc_identifiers.begin();
  for (; ssrc_iterator != ssrc_identifiers.end(); ++ssrc_iterator) {
    const std::string& ssrc = *ssrc_iterator;
    MaybePrintResultsForAudioSend(ssrc, pc_dict, modifier);
    MaybePrintResultsForVideoSend(ssrc, pc_dict, video_modifier);
  }
}

void PrintMetricsForRecvStreams(const base::DictionaryValue& pc_dict,
                                const std::string& modifier,
                                const std::string& video_codec) {
  std::string video_modifier =
      video_codec.empty() ? modifier : modifier + "_" + video_codec;
  const base::DictionaryValue* stats_dict;
  ASSERT_TRUE(pc_dict.GetDictionary("stats", &stats_dict));
  std::set<std::string> ssrc_identifiers = FindAllSsrcIdentifiers(*stats_dict);

  auto ssrc_iterator = ssrc_identifiers.begin();
  for (; ssrc_iterator != ssrc_identifiers.end(); ++ssrc_iterator) {
    const std::string& ssrc = *ssrc_iterator;
    MaybePrintResultsForAudioReceive(ssrc, pc_dict, modifier);
    MaybePrintResultsForVideoReceive(ssrc, pc_dict, video_modifier);
  }
}

}  // namespace test
