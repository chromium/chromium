// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device-icon' component shows an icon for a nearby
 * device. This might be a user defined image or a generic icon based on device
 * type.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './nearby_share_target_types.mojom-lite.js';
import './nearby_share.mojom-lite.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'nearby-device-icon',

  _template: html`{__html_template__}`,

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
