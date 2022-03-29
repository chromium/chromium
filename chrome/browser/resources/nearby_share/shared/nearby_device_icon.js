// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/cr_icons_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './nearby_shared_icons.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview The 'nearby-device-icon' component shows an icon for a nearby
 * device. This might be a user defined image or a generic icon based on device
 * type.
 */

Polymer({
  _template: html`{__html_template__}`,
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
