// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-progress' component shows a progress indicator for
 * a Nearby Share transfer to a remote device. It shows device icon and name,
 * and a circular progress bar that can show either progress as a percentage or
 * an animation if the percentage is indeterminate.
 */

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/cr_icons_css.m.js';
import './nearby_shared_icons.js';
import './nearby_device_icon.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'nearby-progress',

  properties: {
    /**
     * The share target to show the progress for. Expected to start as null,
     * then change to a valid object before this component is shown.
     * @type {?nearbyShare.mojom.ShareTarget}
     */
    shareTarget: {
      type: Object,
      value: null,
    },

    /**
     * If true, displays an animation representing an unknown amount of
     * progress; otherwise, the progress bar is hidden.
     * @type {boolean}
     */
    showIndeterminateProgress: {
      type: Boolean,
      value: false,
    },

    /**
     * If true, then set progress stroke to red, stop any animation, show
     * 100% instead, and set icons to grey. If |showProgress| is |NONE|, then
     * the progress bar is still hidden.
     * @type {boolean}
     */
    hasError: {
      type: Boolean,
      value: false,
    },

    /** @const {number} Size of the target image/icon in pixels. */
    targetImageSize: {
      type: Number,
      readOnly: true,
      value: 68,
    },
  },

  ready: function() {
    this.updateStyles(
        {'--target-image-size': this.properties.targetImageSize.value + 'px'});
    this.listenToTargetImageLoad_();
  },

  /**
   * @return {string} The css class to be applied to the progress wheel.
   */
  getProgressWheelClass_() {
    const classes = [];
    if (this.hasError) {
      classes.push('has-error');
    }
    if (this.showIndeterminateProgress) {
      classes.push('indeterminate-progress');
    } else {
      classes.push('hidden');
    }
    return classes.join(' ');
  },

  /**
   * Allow focusing on the progress bar. Ignored by Chromevox otherwise.
   * @return {number} The tabindex to be applied to the progress wheel.
   */
  getProgressBarTabIndex_() {
    if (this.showIndeterminateProgress && !this.hasError) {
      return 0;
    }
    return -1;
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
