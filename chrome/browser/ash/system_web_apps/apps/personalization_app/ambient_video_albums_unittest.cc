// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/ambient_video_albums.h"

#include <vector>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/constants/ambient_video.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
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

using AmbientVideoAlbumsTest = AmbientAshTestBase;

namespace {

void SetJupiterCustomizationId() {
  auto* fake_statistics_provider = static_cast<system::FakeStatisticsProvider*>(
      system::StatisticsProvider::GetInstance());
  fake_statistics_provider->ClearAllMachineStatistics();
  fake_statistics_provider->SetMachineStatistic(
      system::kCustomizationIdKey,
      std::string(ash::kJupiterScreensaverCustomizationId));
}

}  // namespace

TEST_F(AmbientVideoAlbumsTest, AppendAmbientVideoAlbums) {
  std::vector<mojom::AmbientModeAlbumPtr> albums;
  AppendAmbientVideoAlbums(AmbientVideo::kNewMexico, albums);
  EXPECT_THAT(albums,
              UnorderedElementsAre(
                  Pointee(FieldsAre(kCloudsAlbumId, false, "Cloud Flow", _, _,
                                    mojom::TopicSource::kVideo, _)),
                  Pointee(FieldsAre(kNewMexicoAlbumId, true, "Earth Flow", _, _,
                                    mojom::TopicSource::kVideo, _))));

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

TEST_F(AmbientVideoAlbumsTest, AppendAmbientVideoAlbumsJupiter) {
  SetJupiterCustomizationId();
  std::vector<mojom::AmbientModeAlbumPtr> albums;
  AppendAmbientVideoAlbums(AmbientVideo::kJupiter, albums);
  EXPECT_THAT(
      albums,
      UnorderedElementsAre(
          Pointee(AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kCloudsAlbumId)),
                        Field(&mojom::AmbientModeAlbum::checked, IsFalse()))),
          Pointee(
              AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kNewMexicoAlbumId)),
                    Field(&mojom::AmbientModeAlbum::checked, IsFalse()))),
          Pointee(
              AllOf(Field(&mojom::AmbientModeAlbum::id, Eq(kJupiterAlbumId)),
                    Field(&mojom::AmbientModeAlbum::checked, IsTrue())))));
}

TEST_F(AmbientVideoAlbumsTest, FindAmbientVideoByAlbumId) {
  EXPECT_EQ(FindAmbientVideoByAlbumId(kCloudsAlbumId), AmbientVideo::kClouds);
  EXPECT_EQ(FindAmbientVideoByAlbumId(kNewMexicoAlbumId),
            AmbientVideo::kNewMexico);
  EXPECT_FALSE(FindAmbientVideoByAlbumId("UnknownAlbumId"));
}

TEST_F(AmbientVideoAlbumsTest, FindAmbientVideoByAlbumIdJupiter) {
  EXPECT_FALSE(FindAmbientVideoByAlbumId(kJupiterAlbumId));

  SetJupiterCustomizationId();
  EXPECT_EQ(FindAmbientVideoByAlbumId(kJupiterAlbumId), AmbientVideo::kJupiter);
}

}  // namespace ash::personalization_app
