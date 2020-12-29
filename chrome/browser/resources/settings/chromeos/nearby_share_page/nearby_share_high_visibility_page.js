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
      observer: 'announceRemainingTime_',
    },

    /** @private {?nearbyShare.mojom.RegisterReceiveSurfaceResult} */
    registerResult: {
      type: nearbyShare.mojom.RegisterReceiveSurfaceResult,
      value: null,
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
          'registerResult)'
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

    Polymer.IronA11yAnnouncer.requestAvailability();
    this.announceRemainingTime_(this.remainingTimeInSeconds_);
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
   * minute to avoid overwhelming user.
   * @param {number} remainingSeconds
   * @private
   */
  announceRemainingTime_(remainingSeconds) {
    // Skip announcement for 0 seconds left to avoid alerting on time out.
    // There is a separate time out alert shown in the error section.
    if (remainingSeconds <= 0 || remainingSeconds % 60 !== 0) {
      return;
    }

    const timeValue = this.i18n(
        'nearbyShareHighVisibilitySubTitleMinutes',
        Math.ceil(this.remainingTimeInSeconds_ / 60));

    const announcement = this.i18n(
        'nearbyShareHighVisibilitySubTitle', this.deviceName, timeValue);

    this.fire('iron-announce', {text: announcement});
  },
});
