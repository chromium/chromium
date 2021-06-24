// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device' component shows details of a remote device.
 */

Polymer({
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

    // Need to sanitize the image source for WebUIs with chrome://image/?<URL>.
    // Also adds the parameter to resize to the desired size.
    const chromeSafePrefix = 'chrome://image/?';
    return chromeSafePrefix + this.shareTarget.imageUrl.url + '=s' +
        this.properties.targetImageSize.value;
  },

  /** @private */
  listenToTargetImageLoad_() {
    const imageElement = this.$$('#share-target-image');
    if (imageElement.complete && imageElement.naturalHeight !== 0) {
      this.onTargetImageLoad_();
    } else {
      const nearbyDevice = this;
      imageElement.onload = function() {
        nearbyDevice.onTargetImageLoad_();
      };
    }
  },

  /** @private */
  onTargetImageLoad_() {
    this.$$('#share-target-image').style.display = 'inline';
    this.$$('#icon').style.display = 'none';
  }
});