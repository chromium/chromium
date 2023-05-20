// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/ambient_metrics.h"

#include <string>
#include <vector>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/constants/ambient_theme.h"
#include "ash/constants/ambient_video.h"
#include "ash/public/cpp/ambient/ambient_mode_photo_source.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/test/test_ash_web_view.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {
namespace ambient {
namespace metrics {
using AmbientMetricsTest = testing::Test;

TEST_F(AmbientMetricsTest, AmbientModePhotoSourceArt) {
  AmbientSettings settings;
  settings.topic_source = AmbientModeTopicSource::kArtGallery;

  EXPECT_EQ(AmbientModePhotoSource::kArtGallery,
            AmbientSettingsToPhotoSource(settings));
}

TEST_F(AmbientMetricsTest, AmbientModePhotoSourceGooglePhotosEmpty) {
  AmbientSettings settings;
  settings.topic_source = AmbientModeTopicSource::kGooglePhotos;
  settings.selected_album_ids.clear();

  EXPECT_EQ(AmbientModePhotoSource::kGooglePhotosEmpty,
            AmbientSettingsToPhotoSource(settings));
}

TEST_F(AmbientMetricsTest, AmbientModePhotoSourceGooglePhotosRecentHighlights) {
  AmbientSettings settings;
  settings.topic_source = AmbientModeTopicSource::kGooglePhotos;
  settings.selected_album_ids.clear();
  settings.selected_album_ids.push_back(
      ash::kAmbientModeRecentHighlightsAlbumId);

  EXPECT_EQ(AmbientModePhotoSource::kGooglePhotosRecentHighlights,
            AmbientSettingsToPhotoSource(settings));
}

TEST_F(AmbientMetricsTest, AmbientModePhotoSourceGooglePhotosBoth) {
  AmbientSettings settings;
  settings.topic_source = AmbientModeTopicSource::kGooglePhotos;
  settings.selected_album_ids.clear();
  settings.selected_album_ids.push_back(
      ash::kAmbientModeRecentHighlightsAlbumId);
  settings.selected_album_ids.push_back("abcde");

  EXPECT_EQ(AmbientModePhotoSource::kGooglePhotosBoth,
            AmbientSettingsToPhotoSource(settings));
}

TEST_F(AmbientMetricsTest, AmbientModePhotoSourceGooglePhotosPersonalAlbum) {
  AmbientSettings settings;
  settings.topic_source = AmbientModeTopicSource::kGooglePhotos;
  settings.selected_album_ids.clear();
  settings.selected_album_ids.push_back("abcde");

  EXPECT_EQ(AmbientModePhotoSource::kGooglePhotosPersonalAlbum,
            AmbientSettingsToPhotoSource(settings));
}

TEST(AmbientOrientationMetricsRecorderTest, RecordsEngagementTime) {
  base::ScopedMockElapsedTimersForTest mock_timer;
  base::HistogramTester histogram_tester;
  {
    views::View test_view;
    AmbientOrientationMetricsRecorder recorder(
        &test_view, AmbientUiSettings(AmbientTheme::kFeelTheBreeze));
    test_view.SetSize(gfx::Size(200, 100));
    // No change in size shouldn't count.
    test_view.SetSize(gfx::Size(200, 100));
    test_view.SetSize(gfx::Size(100, 200));
    test_view.SetSize(gfx::Size(200, 100));
  }
  histogram_tester.ExpectTimeBucketCount(
      "Ash.AmbientMode.EngagementTime.FeelTheBreeze.Landscape",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime * 2, 1);
  histogram_tester.ExpectTimeBucketCount(
      "Ash.AmbientMode.EngagementTime.FeelTheBreeze.Portrait",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
}

TEST(AmbientMetricsTest, RecordAmbientModeVideoSmoothness) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  AshWebView::InitParams init_params;
  TestAshWebView view(init_params);
  view.Navigate(net::AppendOrReplaceRef(
      GURL("http://test.com"), R"({"dropped_frames":1,"total_frames":5})"));
  RecordAmbientModeVideoSmoothness(
      &view, AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));
  histogram_tester.ExpectBucketCount(
      "Ash.AmbientMode.VideoSmoothness.Video.Clouds", 80, 1);

  view.Navigate(net::AppendOrReplaceRef(
      GURL("http://test.com"), R"({"dropped_frames":5,"total_frames":5})"));
  RecordAmbientModeVideoSmoothness(
      &view, AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));
  histogram_tester.ExpectBucketCount(
      "Ash.AmbientMode.VideoSmoothness.Video.Clouds", 0, 1);

  view.Navigate(net::AppendOrReplaceRef(
      GURL("http://test.com"), R"({"dropped_frames":0,"total_frames":5})"));
  RecordAmbientModeVideoSmoothness(
      &view, AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));
  histogram_tester.ExpectBucketCount(
      "Ash.AmbientMode.VideoSmoothness.Video.Clouds", 100, 1);
}

TEST(AmbientMetricsTest, RecordAmbientModeVideoSmoothnessInvalidInput) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  AshWebView::InitParams init_params;
  TestAshWebView view(init_params);

  view.Navigate(
      net::AppendOrReplaceRef(GURL("http://test.com"), "invalid-json"));
  RecordAmbientModeVideoSmoothness(
      &view, AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));

  view.Navigate(net::AppendOrReplaceRef(GURL("http://test.com"),
                                        R"({"total_frames":5})"));
  RecordAmbientModeVideoSmoothness(
      &view, AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));

  view.Navigate(net::AppendOrReplaceRef(GURL("http://test.com"),
                                        R"({"dropped_frames":5})"));
  RecordAmbientModeVideoSmoothness(
      &view, AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));

  view.Navigate(net::AppendOrReplaceRef(
      GURL("http://test.com"), R"({"dropped_frames":0,"total_frames":0})"));
  RecordAmbientModeVideoSmoothness(
      &view, AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));

  view.Navigate(net::AppendOrReplaceRef(
      GURL("http://test.com"), R"({"dropped_frames":20,"total_frames":10})"));
  RecordAmbientModeVideoSmoothness(
      &view, AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));

  histogram_tester.ExpectTotalCount(
      "Ash.AmbientMode.VideoSmoothness.Video.Clouds", 0);
}

}  // namespace metrics
}  // namespace ambient
}  // namespace ash
