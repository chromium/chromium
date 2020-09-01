// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_metrics.h"

#include <string>
#include <vector>

#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace metrics
}  // namespace ambient
}  // namespace ash