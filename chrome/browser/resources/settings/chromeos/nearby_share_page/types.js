// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of all possible data usage options for Nearby Share.
 * Note: This must be kept in sync with DataUsage in
 * chrome/browser/nearby_sharing/nearby_constants.h
 * @enum {number}
 */
export const NearbyShareDataUsage = {
  UNKNOWN: 0,
  OFFLINE: 1,
  ONLINE: 2,
  WIFI_ONLY: 3,
};

/**
 * Takes a string and returns a value of the NearbyShareDataUsage enum.
 * @param {string} s string representation of the data usage value
 * @return {!nearbyShare.mojom.DataUsage} enum value
 */
export function dataUsageStringToEnum(s) {
  switch ((parseInt(s, 10))) {
    case NearbyShareDataUsage.OFFLINE:
      return /** @type {!nearbyShare.mojom.DataUsage} */ (
          NearbyShareDataUsage.OFFLINE);
    case NearbyShareDataUsage.ONLINE:
      return /** @type {!nearbyShare.mojom.DataUsage} */ (
          NearbyShareDataUsage.ONLINE);
    case NearbyShareDataUsage.WIFI_ONLY:
      return /** @type {!nearbyShare.mojom.DataUsage} */ (
          NearbyShareDataUsage.WIFI_ONLY);
    default:
      return /** @type {!nearbyShare.mojom.DataUsage} */ (
          NearbyShareDataUsage.UNKNOWN);
  }
}
