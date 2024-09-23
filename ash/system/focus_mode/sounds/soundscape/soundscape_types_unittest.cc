// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"

#include <optional>

#include "ash/system/focus_mode/sounds/soundscape/test/test_data.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kLocale[] = "en-US";
constexpr char kTrackUrl[] = "/tracks/track.mp3";
constexpr char kUuid[] = "bee58263-412c-4f6c-9eb5-7abacc89b2e8";

base::Value TestTrack() {
  base::Value::Dict dict;
  dict.Set("name", "Test Track");
  dict.Set("path", kTrackUrl);

  // The JSON format supports an artist field but it's currently unused.
  dict.Set("artist", "Not parsed");
  return base::Value(std::move(dict));
  ;
}

base::Value TestPlaylist() {
  base::Value::List track_list;
  for (int i = 0; i < 3; i++) {
    track_list.Append(TestTrack());
  }

  base::Value::List name_list =
      base::Value::List()
          .Append(base::Value::Dict()
                      .Set("locale", "en-US")
                      .Set("name", "World's Most Awesome Playlist"))
          .Append(
              base::Value::Dict()
                  .Set("locale", "es-US")
                  .Set("name",
                       "La lista de reproducción más impresionante del mundo"));
  base::Value::Dict dict;
  dict.Set("uuid", kUuid);
  dict.Set("name", std::move(name_list));
  dict.Set("thumbnail", "/thumbs/pic_123453.png");
  dict.Set("tracks", std::move(track_list));
  return base::Value(std::move(dict));
}

TEST(SoundscapeTypeTests, ParseTrack) {
  base::Value test_value = TestTrack();
  std::optional<SoundscapeTrack> track = SoundscapeTrack::FromValue(test_value);
  ASSERT_TRUE(track);
  EXPECT_THAT(track->name, testing::Eq("Test Track"));
  EXPECT_THAT(track->path, testing::Eq(kTrackUrl));
}

TEST(SoundscapeTypeTests, ParseTrack_Empty) {
  // Not a dictionary.
  EXPECT_THAT(
      SoundscapeTrack::FromValue(base::Value(base::Value::Type::STRING)),
      testing::Eq(std::nullopt));
  // Empty dictionary.
  EXPECT_THAT(SoundscapeTrack::FromValue(base::Value(base::Value::Type::DICT)),
              testing::Eq(std::nullopt));
}

TEST(SoundscapeTypeTests, ParsePlaylist) {
  base::Value test_value = TestPlaylist();
  std::optional<SoundscapePlaylist> playlist =
      SoundscapePlaylist::FromValue(kLocale, test_value);
  ASSERT_TRUE(playlist);
  EXPECT_THAT(playlist->name, testing::Eq("World's Most Awesome Playlist"));

  EXPECT_TRUE(playlist->uuid.is_valid());
  EXPECT_THAT(playlist->uuid, testing::Eq(base::Uuid::ParseLowercase(kUuid)));

  EXPECT_THAT(playlist->tracks, testing::SizeIs(3));
  EXPECT_THAT(playlist->tracks,
              testing::Each(testing::Field(&SoundscapeTrack::name,
                                           testing::Eq("Test Track"))));
}

TEST(SoundscapeTypeTests, ParsePlaylist_NonEnglish) {
  base::Value test_value = TestPlaylist();
  std::optional<SoundscapePlaylist> playlist =
      SoundscapePlaylist::FromValue("es-US", test_value);
  ASSERT_TRUE(playlist);
  EXPECT_THAT(
      playlist->name,
      testing::Eq("La lista de reproducción más impresionante del mundo"));
}

TEST(SoundscapeTypeTests, ParsePlaylist_EnglishFallback) {
  base::Value test_value = TestPlaylist();
  // ZZ is a user defined country code and ia is the language code for
  // Interlingua (for which we do not have a mapping).
  const std::string locale = "ia-ZZ";
  std::optional<SoundscapePlaylist> playlist =
      SoundscapePlaylist::FromValue(locale, test_value);
  ASSERT_TRUE(playlist);
  EXPECT_THAT(playlist->name, testing::Eq("World's Most Awesome Playlist"));
}

TEST(SoundscapeTypeTests, ParsePlaylist_Empty) {
  EXPECT_THAT(SoundscapePlaylist::FromValue(
                  kLocale, base::Value(base::Value::Type::NONE)),
              testing::Eq(std::nullopt));
  EXPECT_THAT(SoundscapePlaylist::FromValue(
                  kLocale, base::Value(base::Value::Type::DICT)),
              testing::Eq(std::nullopt));
}

TEST(SoundscapeTypeTests, ParseConfiguration) {
  std::optional<SoundscapeConfiguration> config =
      SoundscapeConfiguration::ParseConfiguration("fr-CA", kTestConfig);
  ASSERT_TRUE(config);
  EXPECT_THAT(config->playlists, testing::SizeIs(4));
  EXPECT_THAT(
      config->playlists,
      testing::ElementsAre(
          testing::Field(&SoundscapePlaylist::name, "Musique Classique"),
          testing::Field(&SoundscapePlaylist::name, "Nature"),
          testing::Field(&SoundscapePlaylist::name, "Flux"),
          testing::Field(&SoundscapePlaylist::name, "Ambiance")));
}

TEST(SoundscapeTypeTests, ParseConfiguration_LangOnly) {
  std::optional<SoundscapeConfiguration> config =
      SoundscapeConfiguration::ParseConfiguration("fr", kTestConfig);
  ASSERT_TRUE(config);
  EXPECT_THAT(config->playlists, testing::SizeIs(4));
  EXPECT_THAT(config->playlists,
              testing::Contains(testing::Field(&SoundscapePlaylist::name,
                                               "Musique Classique")));
}

TEST(SoundscapeTypeTests, ParseConfiguration_Empty) {
  std::optional<SoundscapeConfiguration> config =
      SoundscapeConfiguration::ParseConfiguration(kLocale, "");
  EXPECT_THAT(config, testing::Eq(std::nullopt));
}

TEST(SoundscapeTypeTests, ParseConfiguration_Malformed) {
  std::optional<SoundscapeConfiguration> config =
      SoundscapeConfiguration::ParseConfiguration(kLocale, "{lksjdfksjdfj}");
  EXPECT_THAT(config, testing::Eq(std::nullopt));
}

TEST(SoundscapeTypeTests, ParseConfiguration_InvalidLocale) {
  std::optional<SoundscapeConfiguration> config =
      SoundscapeConfiguration::ParseConfiguration("foo", kTestConfig);
  EXPECT_THAT(config, testing::Eq(std::nullopt));
}

}  // namespace
}  // namespace ash
