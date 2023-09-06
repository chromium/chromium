// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_ui_settings.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/constants/ambient_video.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ash::personalization_app::mojom::AmbientTheme;
using ::testing::Eq;

}  // namespace

class AmbientUiSettingsTest : public ::testing::Test {
 protected:
  AmbientUiSettingsTest() {
    base::Value::Dict default_settings;
    default_settings.Set(ambient::prefs::kAmbientUiSettingsFieldTheme,
                         static_cast<int>(kDefaultAmbientTheme));
    test_pref_service_.registry()->RegisterDictionaryPref(
        ambient::prefs::kAmbientUiSettings, std::move(default_settings));
  }

  TestingPrefServiceSimple test_pref_service_;
};

TEST_F(AmbientUiSettingsTest, DefaultConstructor) {
  EXPECT_THAT(AmbientUiSettings().theme(), Eq(kDefaultAmbientTheme));
}

TEST_F(AmbientUiSettingsTest, DefaultAmbientUiSettings) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, personalization_app::GetTimeOfDayDisabledFeatures());

  ASSERT_FALSE(features::IsTimeOfDayScreenSaverEnabled());

  // No prior set up for kAmbientUiSettings prefs. Without TOD features,
  // kDefaultAmbientTheme (kSlideShow) is set as default.
  test_pref_service_.SetDict(ambient::prefs::kAmbientUiSettings,
                             base::Value::Dict());
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(kDefaultAmbientTheme));

  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      personalization_app::GetTimeOfDayEnabledFeatures(), {});
  ASSERT_TRUE(features::IsTimeOfDayScreenSaverEnabled());
  // No prior set up for kAmbientUiSettings prefs. With TOD features, kVideo is
  // set as default.
  test_pref_service_.SetDict(ambient::prefs::kAmbientUiSettings,
                             base::Value::Dict());
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(AmbientTheme::kVideo));
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).video(),
      Eq(kDefaultAmbientVideo));
}

TEST_F(AmbientUiSettingsTest, PrefManagement) {
  AmbientUiSettings().WriteToPrefService(test_pref_service_);
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(kDefaultAmbientTheme));

  AmbientUiSettings(AmbientTheme::kFloatOnBy)
      .WriteToPrefService(test_pref_service_);
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(AmbientTheme::kFloatOnBy));

  AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds)
      .WriteToPrefService(test_pref_service_);
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(AmbientTheme::kVideo));
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).video(),
      Eq(AmbientVideo::kClouds));
}

TEST_F(AmbientUiSettingsTest, HandlesCorruptedPrefStorage) {
  {
    base::Value::Dict invalid_settings;
    invalid_settings.Set(ambient::prefs::kAmbientUiSettingsFieldTheme,
                         static_cast<int>(AmbientTheme::kMaxValue) + 1);
    test_pref_service_.SetDict(ambient::prefs::kAmbientUiSettings,
                               std::move(invalid_settings));
  }
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(kDefaultAmbientTheme));
  {
    base::Value::Dict invalid_settings;
    invalid_settings.Set(ambient::prefs::kAmbientUiSettingsFieldTheme,
                         static_cast<int>(AmbientTheme::kVideo));
    invalid_settings.Set(ambient::prefs::kAmbientUiSettingsFieldVideo,
                         static_cast<int>(AmbientVideo::kMaxValue) + 1);
    test_pref_service_.SetDict(ambient::prefs::kAmbientUiSettings,
                               std::move(invalid_settings));
  }
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(kDefaultAmbientTheme));

  AmbientUiSettings(AmbientTheme::kFloatOnBy)
      .WriteToPrefService(test_pref_service_);
  EXPECT_THAT(
      AmbientUiSettings::ReadFromPrefService(test_pref_service_).theme(),
      Eq(AmbientTheme::kFloatOnBy));
}

TEST_F(AmbientUiSettingsTest, CrashesWithInvalidSettings) {
  EXPECT_DEATH_IF_SUPPORTED(AmbientUiSettings settings(AmbientTheme::kVideo),
                            "");
}

TEST_F(AmbientUiSettingsTest, ToString) {
  EXPECT_THAT(AmbientUiSettings().ToString(), Eq("SlideShow"));
  EXPECT_THAT(AmbientUiSettings(AmbientTheme::kFeelTheBreeze).ToString(),
              Eq("FeelTheBreeze"));
  EXPECT_THAT(
      AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds).ToString(),
      Eq("Video.Clouds"));
  // The video setting should be ignored.
  EXPECT_THAT(
      AmbientUiSettings(AmbientTheme::kSlideshow, AmbientVideo::kNewMexico)
          .ToString(),
      Eq("SlideShow"));
}

}  // namespace ash
