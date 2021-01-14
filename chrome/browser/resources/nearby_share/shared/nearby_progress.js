// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-progress' component shows a progress indicator for
 * a Nearby Share transfer to a remote device. It shows device icon and name,
 * and a circular progress bar that can show either progress as a percentage or
 * an animation if the percentage is indeterminate.
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
     * If true, displays an animation representing an unknown amount of
     * progress; otherwise, the progress bar is hidden.
     * @type {boolean}
     */
    showIndeterminateProgress: {
      type: Boolean,
      value: false,
    },

    // TODO(crbug.com/1165852) Remove percentage option, not used in practice
    /**
     * The progress percentage to display, expressed as a number between 0 and
     * 100. If null, then an animation representing an indeterminate state is
     * shown, unless |hasError| is true.
     * @type {number}
     */
    progressPercentage: {
      type: Number,
      value: 0,
      observer: 'updateProgress_',
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
    } else if (!this.progressPercentage) {
      classes.push('hidden');
    }
    return classes.join(' ');
  },

  /**
   * Updates the css to set the progress percentage displayed to |value|.
   * @param {?number} value
   */
  updateProgress_(value) {
    this.updateStyles({'--progress-percentage': value});
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
});
