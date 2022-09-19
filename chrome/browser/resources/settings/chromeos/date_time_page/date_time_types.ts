// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Describes values of
 * prefs.generated.resolve_timezone_by_geolocation_method_short. Must be kept
 * in sync with TimeZoneResolverManager::TimeZoneResolveMethod enum.
 */
export enum TimeZoneAutoDetectMethod {
  DISABLED = 0,
  IP_ONLY = 1,
  SEND_WIFI_ACCESS_POINTS = 2,
  SEND_ALL_LOCATION_INFO = 3,
}
