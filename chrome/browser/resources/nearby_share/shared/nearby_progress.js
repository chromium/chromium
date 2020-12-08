// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-progress' component shows a progress indicator for
 * a Nearby Share transfer to a remote device. It shows device icon and name,
 * and a circular progress bar that can show either progress as a percentage or
 * an animation if the percentage is unknown.
 */

Polymer({
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
     * The progress percentage to display, expressed as a number between 0 and
     * 100. If null, then an animation representing an indeterminate state is
     * shown, unless |hasError| is true.
     * @type {?number}
     */
    progress: {
      type: Number,
      value: null,
      observer: 'updateProgress_',
    },

    /**
     * If true, then set progress stroke to red, and if in indeterminate mode,
     * stop the animation and show 100% instead.
     * @type {boolean}
     */
    hasError: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @return {string} The css class to be applied to the progress wheel.
   */
  getProgressWheelClass_() {
    if (this.hasError) {
      return 'has-error';
    }
    if (this.progress === null) {
      return 'unknown-progress';
    }
    return '';
  },

  /**
   * Updates the css to set the progress percentage displayed to |value|.
   * @param {?number} value
   */
  updateProgress_(value) {
    if (value !== null && value !== undefined) {
      this.updateStyles({'--progress-percentage': value});
    }
  },

  /**
   * Allow focusing on the progress bar. Ignored by Chromevox otherwise.
   * @return {number} The tabindex to be applied to the progress wheel.
   */
  getProgressBarTabIndex_() {
    if (this.progress && !this.hasError) {
      return 0;
    }
    return -1;
  },
});
