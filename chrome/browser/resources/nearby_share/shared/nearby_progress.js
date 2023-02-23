// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-progress' component shows a progress indicator for
 * a Nearby Share transfer to a remote device. It shows device icon and name,
 * and a circular progress bar that can show either progress as a percentage or
 * an animation if the percentage is indeterminate.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import './nearby_shared_icons.html.js';
import './nearby_device_icon.js';

import {ShareTarget} from '/mojo/nearby_share.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_progress.html.js';

/** @polymer */
export class NearbyProgressElement extends PolymerElement {
  static get is() {
    return 'nearby-progress';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The share target to show the progress for. Expected to start as null,
       * then change to a valid object before this component is shown.
       * @type {?ShareTarget}
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
    };
  }

  ready() {
    super.ready();

    this.updateStyles({'--target-image-size': this.targetImageSize + 'px'});
    this.listenToTargetImageLoad_();
  }

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
  }

  /**
   * Allow focusing on the progress bar. Ignored by Chromevox otherwise.
   * @return {number} The tabindex to be applied to the progress wheel.
   */
  getProgressBarTabIndex_() {
    if (this.showIndeterminateProgress && !this.hasError) {
      return 0;
    }
    return -1;
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

customElements.define(NearbyProgressElement.is, NearbyProgressElement);
