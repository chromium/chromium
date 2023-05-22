// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/ambient_video_albums.h"

#include <vector>

#include "ash/constants/ambient_video.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::personalization_app {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

TEST(AmbientVideoAlbumsTest, AppendAmbientVideoAlbums) {
  std::vector<mojom::AmbientModeAlbumPtr> albums;
  AppendAmbientVideoAlbums(AmbientVideo::kNewMexico, albums);
  EXPECT_THAT(albums,
              UnorderedElementsAre(
                  Pointee(FieldsAre(kCloudsAlbumId, false, "Cloud Flow", _, _,
                                    AmbientModeTopicSource::kVideo, _)),
                  Pointee(FieldsAre(kNewMexicoAlbumId, true, "Earth Flow", _, _,
                                    AmbientModeTopicSource::kVideo, _))));

  albums.clear();
  AppendAmbientVideoAlbums(AmbientVideo::kClouds, albums);
  EXPECT_THAT(
      albums,
      UnorderedElementsAre(
          Pointee(AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kCloudsAlbumId)),
                        Field(&mojom::AmbientModeAlbum::checked, IsTrue()))),
          Pointee(
              AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kNewMexicoAlbumId)),
                    Field(&mojom::AmbientModeAlbum::checked, IsFalse())))));
}

TEST(AmbientVideoAlbumsTest, FindAmbientVideoByAlbumId) {
  EXPECT_EQ(FindAmbientVideoByAlbumId(kCloudsAlbumId), AmbientVideo::kClouds);
  EXPECT_EQ(FindAmbientVideoByAlbumId(kNewMexicoAlbumId),
            AmbientVideo::kNewMexico);
  EXPECT_FALSE(FindAmbientVideoByAlbumId("UnknownAlbumId"));
}

}  // namespace ash::personalization_app
