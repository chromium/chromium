// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_GEOLOCATION_ACCESS_LEVEL_H_
#define ASH_CONSTANTS_GEOLOCATION_ACCESS_LEVEL_H_

namespace ash {

// This enum defines the access levels of the Privacy Hub Geolocation feature.
// Affects the entire ChromeOS system and all client applications.
// Don't modify or reorder the enum elements. New values can be added at the
// end. These values shall be in sync with the
// `DeviceLoginScreenGeolocationAccessLevelProto::GeolocationAccessLevel` and
// //tools/metrics/histograms/metadata/chromeos/enums.xml.
enum class GeolocationAccessLevel {
  kDisallowed = 0,
  kAllowed = 1,
  kOnlyAllowedForSystem = 2,
  kMaxValue = kOnlyAllowedForSystem,
};

}  // namespace ash

#endif  // ASH_CONSTANTS_GEOLOCATION_ACCESS_LEVEL_H_
