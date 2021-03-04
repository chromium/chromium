// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-high-visibility-page' component is opened when the
 * user is broadcast in high-visibility mode. The user may cancel to stop high
 * visibility mode at any time.
 */

/**
 * Represents the current error state, if one exists.
 * @enum {number}
 */
const NearbyVisibilityErrorState = {
  TIMED_OUT: 0,
  NO_CONNECTION_MEDIUM: 1,
  TRANSFER_IN_PROGRESS: 2,
  SOMETHING_WRONG: 3,
};

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
     * DOMHighResTimeStamp in milliseconds of when high visibility will be
     * turned off.
     * @type {number}
     */
    shutoffTimestamp: {
      type: Number,
      value: 0,
    },

    /**
     * Calculated value of remaining seconds of high visibility mode.
     * Initialized to -1 to differentiate it from timed out state.
     * @private {number}
     */
    remainingTimeInSeconds_: {
      type: Number,
      value: -1,
    },

    /** @private {?nearbyShare.mojom.RegisterReceiveSurfaceResult} */
    registerResult: {
      type: nearbyShare.mojom.RegisterReceiveSurfaceResult,
      value: null,
    },

    /**
     * @type {boolean}
     */
    nearbyProcessStopped: {
      type: Boolean,
      value: false,
    },

    /**
     * @type {boolean}
     */
    startAdvertisingFailed: {
      type: Boolean,
      value: false,
    },

    /**
     * A null |setupState_| indicates that the operation has not yet started.
     * @private {?NearbyVisibilityErrorState}
     */
    errorState_: {
      type: Number,
      value: null,
      computed:
          'computeErrorState_(shutoffTimestamp, remainingTimeInSeconds_,' +
          'registerResult, nearbyProcessStopped, startAdvertisingFailed)'
    }

  },

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
    if (this.remainingTimeIntervalId_ !== -1) {
      clearInterval(this.remainingTimeIntervalId_);
      this.remainingTimeIntervalId_ = -1;
    }
  },

  /** @private */
  calculateRemainingTime_() {
    if (this.shutoffTimestamp === 0) {
      return;
    }

    const now = performance.now();
    const remainingTimeInMs =
        this.shutoffTimestamp > now ? this.shutoffTimestamp - now : 0;
    this.remainingTimeInSeconds_ = Math.ceil(remainingTimeInMs / 1000);
  },

  /**
   * @return {boolean}
   * @protected
   */
  highVisibilityTimedOut_() {
    // High visibility session is timed out only if remaining seconds is 0 AND
    // timestamp is also set.
    return (this.remainingTimeInSeconds_ === 0) &&
        (this.shutoffTimestamp !== 0);
  },

  /**
   * @return {?NearbyVisibilityErrorState}
   * @protected
   */
  computeErrorState_() {
    if (this.registerResult ===
        nearbyShare.mojom.RegisterReceiveSurfaceResult.kNoConnectionMedium) {
      return NearbyVisibilityErrorState.NO_CONNECTION_MEDIUM;
    }
    if (this.registerResult ===
        nearbyShare.mojom.RegisterReceiveSurfaceResult.kTransferInProgress) {
      return NearbyVisibilityErrorState.TRANSFER_IN_PROGRESS;
    }
    if (this.highVisibilityTimedOut_()) {
      return NearbyVisibilityErrorState.TIMED_OUT;
    }
    if (this.registerResult ===
            nearbyShare.mojom.RegisterReceiveSurfaceResult.kFailure ||
        this.nearbyProcessStopped || this.startAdvertisingFailed) {
      return NearbyVisibilityErrorState.SOMETHING_WRONG;
    }
    return null;
  },


  /**
   * @return {string} localized string
   * @protected
   */
  getErrorTitle_() {
    switch (this.errorState_) {
      case NearbyVisibilityErrorState.TIMED_OUT:
        return this.i18n('nearbyShareErrorTimeOut');
      case NearbyVisibilityErrorState.NO_CONNECTION_MEDIUM:
        return this.i18n('nearbyShareErrorNoConnectionMedium');
      case NearbyVisibilityErrorState.TRANSFER_IN_PROGRESS:
        return this.i18n('nearbyShareErrorTransferInProgressTitle');
      case NearbyVisibilityErrorState.SOMETHING_WRONG:
        return this.i18n('nearbyShareErrorCantReceive');
      default:
        return '';
    }
  },

  /**
   * @return {string} localized string
   * @protected
   */
  getErrorDescription_() {
    switch (this.errorState_) {
      case NearbyVisibilityErrorState.TIMED_OUT:
        return this.i18nAdvanced('nearbyShareHighVisibilityTimeoutText');
      case NearbyVisibilityErrorState.NO_CONNECTION_MEDIUM:
        return this.i18n('nearbyShareErrorNoConnectionMediumDescription');
      case NearbyVisibilityErrorState.TRANSFER_IN_PROGRESS:
        return this.i18n('nearbyShareErrorTransferInProgressDescription');
      case NearbyVisibilityErrorState.SOMETHING_WRONG:
        return this.i18n('nearbyShareErrorSomethingWrong');
      default:
        return '';
    }
  },

  /**
   * @return {string} localized string
   * @protected
   */
  getSubTitle_() {
    if (this.remainingTimeInSeconds_ === -1) {
      return '';
    }

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

  /**
   * Announce the remaining time for screen readers. Only announce once per
   * minute to avoid overwhelming user. Though this gets called once every
   * second, the value returned only changes each minute.
   * @return {string} The alternate page subtitle to be used as an aria-live
   *     announcement for screen readers.
   * @private
   */
  getA11yAnnouncedSubTitle_() {
    // Skip announcement for 0 seconds left to avoid alerting on time out.
    // There is a separate time out alert shown in the error section.
    if (this.remainingTimeInSeconds_ === 0) {
      return '';
    }
    const remainingMinutes = this.remainingTimeInSeconds_ > 0 ?
        Math.ceil(this.remainingTimeInSeconds_ / 60) :
        5;

    const timeValue =
        this.i18n('nearbyShareHighVisibilitySubTitleMinutes', remainingMinutes);

    return this.i18n(
        'nearbyShareHighVisibilitySubTitle', this.deviceName, timeValue);
  },
});
