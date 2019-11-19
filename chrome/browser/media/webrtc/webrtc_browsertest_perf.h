// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_PERF_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_PERF_H_

#include <string>

namespace base {
class DictionaryValue;
}

namespace test {

// These functions takes parsed data (on one peer connection) from the
// peerConnectionDataStore object that is backing webrtc-internals and prints
// metrics they consider interesting using testing/perf/perf_test.h primitives.
// The idea is to put as many webrtc-related metrics as possible into the
// dashboard and thereby track it for regressions.
//
// These functions expect to run under googletest and will use EXPECT_ and
// ASSERT_ macros to signal failure. They take as argument one peer connection's
// stats data and a |modifier| to append to each result bucket. For instance, if
// the modifier is '_oneway', the rtt stat will be logged as goog_rtt in
// the video_tx_oneway bucket.
// If |video_codec| is a non-empty string, the codec name is appended last for
// video metrics, e.g. 'video_tx_oneway_VP9'.
void PrintBweForVideoMetrics(const base::DictionaryValue& pc_dict,
                             const std::string& modifier,
                             const std::string& video_codec);
void PrintMetricsForAllStreams(const base::DictionaryValue& pc_dict,
                               const std::string& modifier,
                               const std::string& video_codec);
void PrintMetricsForSendStreams(const base::DictionaryValue& pc_dict,
                                const std::string& modifier,
                                const std::string& video_codec);
void PrintMetricsForRecvStreams(const base::DictionaryValue& pc_dict,
                                const std::string& modifier,
                                const std::string& video_codec);

bool WriteCompareVideosOutputAsHistogram(const std::string& test_label,
                                         const std::string& output);

}  // namespace test

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_PERF_H_
