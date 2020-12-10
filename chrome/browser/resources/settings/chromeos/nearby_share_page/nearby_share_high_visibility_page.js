// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-high-visibility-page' component is opened when the
 * user is broadcast in high-visibility mode. The user may cancel to stop high
 * visibility mode at any time.
 */
Polymer({
  is: 'nearby-share-high-visibility-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {string}
     */
    deviceName: {
      notify: true,
      type: String,
      value: 'DEVICE_NAME_NOT_SET',
    },

    /**
     * Timestamp in milliseconds since unix epoch of when high visibility will
     * be turned off.
     * @type {number}
     */
    shutoffTimestamp: {
      type: Number,
      value: 0,
    },
  },

  /**
   * Calculated value of remaining seconds of high visibility mode.
   * @private {number}
   */
  remainingTimeInSeconds_: 0,

  /** @private {number} */
  remainingTimeIntervalId_: -1,

  /** @override */
  attached() {
    this.calculateRemainingTime_();
    this.remainingTimeIntervalId_ = setInterval(() => {
      this.calculateRemainingTime_();
    }, 1000);
  },

  /** @override */
  detached() {
    if (this.remainingTimeIntervalId_ === -1) {
      clearInterval(this.remainingTimeIntervalId_);
      this.remainingTimeIntervalId_ = -1;
    }
  },

  /** @private */
  calculateRemainingTime_() {
    const now = new Date().getTime();
    const remainingTimeInMs =
        this.shutoffTimestamp > now ? this.shutoffTimestamp - now : 0;
    this.remainingTimeInSeconds_ = Math.trunc(remainingTimeInMs / 1000);
  },

  /**
   * @return {string} localized string
   * @protected
   */
  getSubTitle_() {
    let timeValue = '';
    if (this.remainingTimeInSeconds_ > 60) {
      timeValue = this.i18n(
          'nearbyShareHighVisibilitySubTitleMinutes',
          Math.ceil(this.remainingTimeInSeconds_ / 60));
    } else {
      timeValue = this.i18n(
          'nearbyShareHighVisibilitySubTitleSeconds',
          this.remainingTimeInSeconds_);
    }

    return this.i18n(
        'nearbyShareHighVisibilitySubTitle', this.deviceName, timeValue);
  },
});
