// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_features_parser.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcFeaturesParserTest : public testing::Test {
 public:
  ArcFeaturesParserTest() = default;

  ArcFeaturesParserTest(const ArcFeaturesParserTest&) = delete;
  ArcFeaturesParserTest& operator=(const ArcFeaturesParserTest&) = delete;
};

constexpr const char kValidJson[] = R"json({"features": [
      {
        "name": "com.google.android.feature.GOOGLE_BUILD",
        "version": 0
      },
      {
        "name": "com.google.android.feature.GOOGLE_EXPERIENCE",
        "version": 2
      }
    ],
    "unavailable_features": [],
    "properties": {
      "ro.product.cpu.abilist": "x86_64,x86,armeabi-v7a,armeabi",
      "ro.build.version.sdk": "30",
      "ro.build.version.release": "11",
      "ro.build.fingerprint": "google/hatch/hatch_cheets:foo"
    },
    "play_store_version": "81010860"})json";

constexpr const char kValidJsonWithUnavailableFeature[] =
    R"json({"features": [
      {
        "name": "android.software.home_screen",
        "version": 0
      },
      {
        "name": "com.google.android.feature.GOOGLE_EXPERIENCE",
        "version": 0
      }
    ],
    "unavailable_features": ["android.software.location"],
    "properties": {
      "ro.product.cpu.abilist": "x86_64,x86,armeabi-v7a,armeabi",
      "ro.build.version.sdk": "30",
      "ro.build.version.release": "11",
      "ro.build.fingerprint": "google/hatch/hatch_cheets:foo"
    },
    "play_store_version": "81010860"})json";

constexpr const char kValidJsonFeatureEmptyName[] =
    R"json({"features": [
      {
        "name": "android.hardware.faketouch",
        "version": 0
      },
      {
        "name": "android.hardware.location",
        "version": 0
      },
      {
        "name": "",
        "version": 0
      }
    ],
    "unavailable_features": ["android.software.home_screen", ""],
    "properties": {
      "ro.product.cpu.abilist": "x86_64,x86,armeabi-v7a,armeabi",
      "ro.build.version.sdk": "30",
      "ro.build.version.release": "11",
      "ro.build.fingerprint": "google/hatch/hatch_cheets:foo"
    },
    "play_store_version": "81010860"})json";

constexpr const char kInvalidJsonWithMissingFields[] =
    R"json({"invalid_root": [
      {
        "name": "android.hardware.location"
      },
      {
        "name": "android.hardware.location.network"
      }
    ],
    "invalid_root_second": [],
    "invalid_root_third": {}})json";

TEST_F(ArcFeaturesParserTest, ParseEmptyJson) {
  std::optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(std::string_view());
  EXPECT_EQ(arc_features, std::nullopt);
}

TEST_F(ArcFeaturesParserTest, ParseInvalidJson) {
  std::optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(
          kInvalidJsonWithMissingFields);
  EXPECT_EQ(arc_features, std::nullopt);
}

TEST_F(ArcFeaturesParserTest, ParseValidJson) {
  std::optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(kValidJson);
  auto feature_map = arc_features->feature_map;
  auto unavailable_features = arc_features->unavailable_features;
  auto build_props = arc_features->build_props;
  auto play_store_version = arc_features->play_store_version;
  EXPECT_EQ(feature_map.size(), 2u);
  EXPECT_EQ(unavailable_features.size(), 0u);
  EXPECT_EQ(play_store_version, "81010860");

  EXPECT_EQ(build_props.release_version, "11");
  EXPECT_EQ(build_props.sdk_version, "30");
  EXPECT_EQ(build_props.abi_list, "x86_64,x86,armeabi-v7a,armeabi");
  EXPECT_EQ(build_props.fingerprint, "google/hatch/hatch_cheets:foo");
}

TEST_F(ArcFeaturesParserTest, ParseValidJsonWithUnavailableFeature) {
  std::optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(
          kValidJsonWithUnavailableFeature);
  auto feature_map = arc_features->feature_map;
  auto unavailable_features = arc_features->unavailable_features;
  auto build_props = arc_features->build_props;
  auto play_store_version = arc_features->play_store_version;
  EXPECT_EQ(feature_map.size(), 2u);
  EXPECT_EQ(unavailable_features.size(), 1u);
  EXPECT_EQ(play_store_version, "81010860");
}

TEST_F(ArcFeaturesParserTest, ParseValidJsonWithEmptyFeatureName) {
  std::optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(
          kValidJsonFeatureEmptyName);
  EXPECT_EQ(arc_features, std::nullopt);
}

TEST_F(ArcFeaturesParserTest, ParseValidJsonWithSystemAbiListProperty) {
  constexpr const char kValidJsonWithSystemAbiList[] = R"json({"features": [],
    "unavailable_features": [],
    "properties": {
      "ro.system.product.cpu.abilist": "x86_64,x86,armeabi-v7a,armeabi",
      "ro.build.version.sdk": "30",
      "ro.build.version.release": "11",
      "ro.build.fingerprint": "google/hatch/hatch_cheets:foo"
    },
    "play_store_version": "81010860"})json";

  std::optional<ArcFeatures> arc_features =
      ArcFeaturesParser::ParseFeaturesJsonForTesting(
          kValidJsonWithSystemAbiList);
  EXPECT_EQ(arc_features->build_props.abi_list,
            "x86_64,x86,armeabi-v7a,armeabi");
}

}  // namespace
}  // namespace arc
