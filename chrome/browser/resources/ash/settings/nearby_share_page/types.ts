// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataUsage} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

/**
 * Enumeration of all possible data usage options for Nearby Share.
 * Note: This must be kept in sync with DataUsage in
 * chrome/browser/nearby_sharing/nearby_constants.h
 */
export enum NearbyShareDataUsage {
  UNKNOWN = 0,
  OFFLINE = 1,
  ONLINE = 2,
  WIFI_ONLY = 3,
}

/**
 * Takes a string and returns a value of the NearbyShareDataUsage enum.
 * @param s string representation of the data usage value
 * @return enum value
 */
export function dataUsageStringToEnum(s: string): DataUsage {
  switch (parseInt(s, 10)) {
    case NearbyShareDataUsage.OFFLINE:
      return DataUsage.kOffline;
    case NearbyShareDataUsage.ONLINE:
      return DataUsage.kOnline;
    case NearbyShareDataUsage.WIFI_ONLY:
      return DataUsage.kWifiOnly;
    default:
      return DataUsage.kUnknown;
  }
}
