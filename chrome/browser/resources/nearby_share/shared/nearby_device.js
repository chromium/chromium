// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/cr_icons_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './nearby_shared_icons.js';
import './nearby_device_icon.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview The 'nearby-device' component shows details of a remote device.
 */

Polymer({
  _template: html`{__html_template__}`,
  is: 'nearby-device',

  properties: {
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
  },

  ready: function() {
    this.updateStyles(
        {'--target-image-size': this.properties.targetImageSize.value + 'px'});
    this.listenToTargetImageLoad_();
  },

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
    return this.shareTarget.imageUrl.url + '=s' +
        this.properties.targetImageSize.value;
  },

  /** @private */
  listenToTargetImageLoad_() {
    const autoImg = this.$$('#share-target-image');
    if (autoImg.complete && autoImg.naturalHeight !== 0) {
      this.onTargetImageLoad_();
    } else {
      autoImg.onload = () => {
        this.onTargetImageLoad_();
      };
    }
  },

  /** @private */
  onTargetImageLoad_() {
    this.$$('#share-target-image').style.display = 'inline';
    this.$$('#icon').style.display = 'none';
  }
});
