// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_hint_cache.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class LiteVideoHintCacheTest : public testing::Test {
 public:
  LiteVideoHintCacheTest() = default;
  ~LiteVideoHintCacheTest() override = default;

  void SetUp() override { ConfigHintCacheWithParams({}); }

  void ConfigHintCacheWithParams(
      const std::map<std::string, std::string>& params) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kLiteVideo, params);
    hint_cache_ = std::make_unique<lite_video::LiteVideoHintCache>();
  }

  void DisableLiteVideo() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({}, {features::kLiteVideo});
  }

  lite_video::LiteVideoHintCache* hint_cache() { return hint_cache_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<lite_video::LiteVideoHintCache> hint_cache_;
};

TEST_F(LiteVideoHintCacheTest, ValidHintAvailable) {
  base::HistogramTester histogram_tester;
  GURL url("https://LiteVideo.com");
  ConfigHintCacheWithParams(
      {{"lite_video_origin_hints", "{\"litevideo.com\": 123}"}});
  absl::optional<lite_video::LiteVideoHint> hint =
      hint_cache()->GetHintForNavigationURL(url);
  ASSERT_TRUE(hint);
  EXPECT_EQ(123, hint->target_downlink_bandwidth_kbps());
  EXPECT_EQ(lite_video::features::LiteVideoKilobytesToBufferBeforeThrottle(),
            hint->kilobytes_to_buffer_before_throttle());
  EXPECT_EQ(lite_video::features::LiteVideoTargetDownlinkRTTLatency(),
            hint->target_downlink_rtt_latency());
  histogram_tester.ExpectUniqueSample("LiteVideo.OriginHints.ParseResult", true,
                                      1);
}

TEST_F(LiteVideoHintCacheTest, NoHintAvailableForURL) {
  base::HistogramTester histogram_tester;
  GURL url("https://NoVideo.com");
  ConfigHintCacheWithParams(
      {{"lite_video_origin_hints", "{\"litevideo.com\": 123}"}});
  EXPECT_FALSE(hint_cache()->GetHintForNavigationURL(url));
  histogram_tester.ExpectUniqueSample("LiteVideo.OriginHints.ParseResult", true,
                                      1);
}

TEST_F(LiteVideoHintCacheTest, NoHintInvalidJSON_InvalidTargetBandwidth) {
  base::HistogramTester histogram_tester;
  GURL url("https://LiteVideo.com");
  ConfigHintCacheWithParams(
      {{"lite_video_origin_hints", "{\"litevideo.com\": 123f}"}});
  EXPECT_FALSE(hint_cache()->GetHintForNavigationURL(url));
  histogram_tester.ExpectUniqueSample("LiteVideo.OriginHints.ParseResult",
                                      false, 1);
}

TEST_F(LiteVideoHintCacheTest, NoHintInvalidJSON_ParseError) {
  base::HistogramTester histogram_tester;
  GURL url("https://LiteVideo.com");
  ConfigHintCacheWithParams(
      {{"lite_video_origin_hints", "{\"litevideo.com\" 123f}"}});
  EXPECT_FALSE(hint_cache()->GetHintForNavigationURL(url));
  histogram_tester.ExpectUniqueSample("LiteVideo.OriginHints.ParseResult",
                                      false, 1);
}

TEST_F(LiteVideoHintCacheTest, LiteVideoDisabled) {
  base::HistogramTester histogram_tester;
  DisableLiteVideo();
  GURL url("https://LiteVideo.com");
  EXPECT_FALSE(hint_cache()->GetHintForNavigationURL(url));
  histogram_tester.ExpectTotalCount("LiteVideo.OriginHints.ParseResult", 0);
}
