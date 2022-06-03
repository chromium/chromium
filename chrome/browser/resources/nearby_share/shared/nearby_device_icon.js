// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device-icon' component shows an icon for a nearby
 * device. This might be a user defined image or a generic icon based on device
 * type.
 */

Polymer({
  is: 'nearby-device-icon',

  properties: {
    /**
     * The share target to show the icon for. Expected to start as null, then
     * change to a valid object before this component is shown.
     * @type {?nearbyShare.mojom.ShareTarget}
     */
    shareTarget: {
      type: Object,
      value: null,
    },
  },

  /**
   * @return {string}
   * @private
   */
  getShareTargetIcon_() {
    if (!this.shareTarget) {
      return 'nearby-share:laptop';
    }
    switch (this.shareTarget.type) {
      case nearbyShare.mojom.ShareTargetType.kPhone:
        return 'nearby-share:smartphone';
      case nearbyShare.mojom.ShareTargetType.kTablet:
        return 'nearby-share:tablet';
      case nearbyShare.mojom.ShareTargetType.kLaptop:
        return 'nearby-share:laptop';
      default:
        return 'nearby-share:laptop';
    }
  },
});
