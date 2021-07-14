// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SIMPLE_GEO_POSITION_H_
#define ASH_PUBLIC_CPP_SIMPLE_GEO_POSITION_H_

namespace ash {
// Represents a geolocation position fix. It's "simple" because it doesn't
// expose all the parameters of the position interface as defined by the
// Geolocation API Specification:
//   https://dev.w3.org/geo/api/spec-source.html#position_interface
// The NightLightController is only interested in valid latitude and
// longitude. It also doesn't require any specific accuracy. The more accurate
// the positions, the more accurate sunset and sunrise times calculations.
// However, an IP-based geoposition is considered good enough.
struct SimpleGeoposition {
  bool operator==(const SimpleGeoposition& other) const {
    return latitude == other.latitude && longitude == other.longitude;
  }
  double latitude;
  double longitude;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SIMPLE_GEO_POSITION_H_
