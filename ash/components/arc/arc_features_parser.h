// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ARC_FEATURES_PARSER_H_
#define ASH_COMPONENTS_ARC_ARC_FEATURES_PARSER_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"

namespace arc {

// Build properties that have been extracted from the ARC feature file.
struct BuildPropsMapping {
  BuildPropsMapping();

  BuildPropsMapping(const BuildPropsMapping&);
  BuildPropsMapping& operator=(const BuildPropsMapping&);

  BuildPropsMapping(BuildPropsMapping&& other);
  BuildPropsMapping& operator=(BuildPropsMapping&& other);

  ~BuildPropsMapping();

  // Build fingerprint from the "ro.build.fingerprint" property. e.g.
  // "google/hatch/hatch_cheets:13/R118-15602.0.0/10753998:userdebug/dev-keys".
  std::string fingerprint;

  // SDK version from the "ro.build.version.sdk" property. e.g. "33".
  std::string sdk_version;

  // Human-readable release version number from the "ro.build.version.release"
  // property. e.g. "13".
  std::string release_version;

  // ABI list from the "ro.product.cpu.abilist" property. e.g.
  // "x86_64,arm64-v8a,x86,armeabi-v7a,armeabi"
  std::string abi_list;
};

// This struct contains an ARC available feature map, unavailable feature set
// and ARC build property map.
struct ArcFeatures {
  // Key is the feature name. Value is the feature version.
  using FeatureVersionMapping = std::map<std::string, int>;

  // Each item in the vector is the feature name.
  using FeatureList = std::vector<std::string>;

  ArcFeatures();

  ArcFeatures(const ArcFeatures&) = delete;
  ArcFeatures& operator=(const ArcFeatures&) = delete;

  ArcFeatures(ArcFeatures&& other);
  ArcFeatures& operator=(ArcFeatures&& other);

  ~ArcFeatures();

  // This map contains all ARC system available features. For each feature, it
  // has the name and version. Unavailable features have been filtered out from
  // this map.
  FeatureVersionMapping feature_map;

  // This list contains all ARC unavailable feature names.
  FeatureList unavailable_features;

  // Struct for build properties that are referenced in Ash.
  BuildPropsMapping build_props;

  std::string play_store_version;
};

// Parses JSON files for Android system available features and build properties.
//
// A feature JSON file looks like this:
// {
//   "features": [
//     {
//       "name": "android.hardware.location",
//       "version: 2
//     },
//     {
//       "name": "android.hardware.location.network",
//       "version": 0
//     }
//   ],
//   "unavailable_features": [
//     "android.hardware.usb.accessory",
//     "android.software.live_tv"
//   ],
//   "properties": {
//     "ro.product.cpu.abilist": "x86_64,x86,armeabi-v7a,armeabi",
//     "ro.build.version.sdk": "25"
//   },
//   "play_store_version": "81010860"
// }
class ArcFeaturesParser {
 public:
  ArcFeaturesParser(const ArcFeaturesParser&) = delete;
  ArcFeaturesParser& operator=(const ArcFeaturesParser&) = delete;

  // Get ARC system available features.
  static void GetArcFeatures(
      base::OnceCallback<void(std::optional<ArcFeatures>)> callback);

  // Given an input feature JSON, return ARC features. This method is for
  // testing only.
  static std::optional<ArcFeatures> ParseFeaturesJsonForTesting(
      std::string_view input_json);

  // Overrides the ArcFeatures returned by GetArcFeatures, for testing only.
  // Does not take ownership of |getter|, it must be alive when GetArcFeatures
  // is called.
  static void SetArcFeaturesGetterForTesting(
      base::RepeatingCallback<std::optional<ArcFeatures>()>* getter);
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ARC_FEATURES_PARSER_H_
