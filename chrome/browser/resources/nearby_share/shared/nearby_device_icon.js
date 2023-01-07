// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device-icon' component shows an icon for a nearby
 * device. This might be a user defined image or a generic icon based on device
 * type.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './nearby_shared_icons.html.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_device_icon.html.js';

/** @polymer */
export class NearbyDeviceIconElement extends PolymerElement {
  static get is() {
    return 'nearby-device-icon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The share target to show the icon for. Expected to start as null, then
       * change to a valid object before this component is shown.
       * @type {?nearbyShare.mojom.ShareTarget}
       */
      shareTarget: {
        type: Object,
        value: null,
      },
    };
  }

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
  }
}

customElements.define(NearbyDeviceIconElement.is, NearbyDeviceIconElement);
