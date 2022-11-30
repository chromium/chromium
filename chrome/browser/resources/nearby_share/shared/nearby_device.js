// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device' component shows details of a remote device.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './nearby_shared_icons.html.js';
import './nearby_device_icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_device.html.js';

/** @polymer */
export class NearbyDeviceElement extends PolymerElement {
  static get is() {
    return 'nearby-device';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Expected to start as null, then change to a valid object before this
       * component is shown.
       * @type {?nearbyShare.mojom.ShareTarget}
       */
      shareTarget: {
        type: Object,
        value: null,
      },

      /**
       * Whether this share target is selected.
       * @type {boolean}
       */
      isSelected: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** @const {number} Size of the target image/icon in pixels. */
      targetImageSize: {
        type: Number,
        readOnly: true,
        value: 26,
      },
    };
  }

  ready() {
    super.ready();

    this.updateStyles({'--target-image-size': this.targetImageSize + 'px'});
    this.listenToTargetImageLoad_();
  }

  /**
   * @return {!string} The URL of the target image.
   * @private
   */
  getTargetImageUrl_() {
    if (!(this.shareTarget && this.shareTarget.imageUrl &&
          this.shareTarget.imageUrl.url &&
          this.shareTarget.imageUrl.url.length)) {
      return '';
    }

    // Adds the parameter to resize to the desired size.
    return this.shareTarget.imageUrl.url + '=s' + this.targetImageSize;
  }

  /** @private */
  listenToTargetImageLoad_() {
    const autoImg = this.shadowRoot.querySelector('#share-target-image');
    if (autoImg.complete && autoImg.naturalHeight !== 0) {
      this.onTargetImageLoad_();
    } else {
      autoImg.onload = () => {
        this.onTargetImageLoad_();
      };
    }
  }

  /** @private */
  onTargetImageLoad_() {
    this.shadowRoot.querySelector('#share-target-image').style.display =
        'inline';
    this.shadowRoot.querySelector('#icon').style.display = 'none';
  }
}

customElements.define(NearbyDeviceElement.is, NearbyDeviceElement);
