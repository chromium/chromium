// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'urgent-password-expiry-notification' is a notification that
 * warns the user their password is about to expire - but it is a large
 * notification that is shown in the center of the screen.
 * It is implemented not using the notification system, but as a system dialog.
 */

const ONE_SECOND_IN_MS = 1000;
const ONE_MINUTE_IN_MS = ONE_SECOND_IN_MS * 60;
const ONE_HOUR_IN_MS = ONE_MINUTE_IN_MS * 60;

Polymer({
  is: 'urgent-password-expiry-notification',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private {string} */
    title_: {
      type: String,
      value: '',
    },
  },

  /** @type {?Date} */
  expirationTime_: null,

  /** @type {?number} */
  countDownIntervalId_: null,

  /** @type {?number} */
  countDownIntervalMs_: null,

  /** @override */
  attached() {
    this.$.dialog.showModal();
    if (loadTimeData.valueExists('initialTitle')) {
      this.title_ = loadTimeData.getString('initialTitle');
    }

    if (loadTimeData.valueExists('expirationTime')) {
      const expirationTimeStr = loadTimeData.getString('expirationTime');
      const expirationTimeMs = parseInt(expirationTimeStr);
      if (isNaN(expirationTimeMs)) {
        console.error('Bad expiration time: ' + expirationTimeStr);
      } else {
        this.expirationTime_ = new Date(expirationTimeMs);
        this.ensureCountDownCalledOftenEnough_();
      }
    }
  },

  /** @private */
  ensureCountDownCalledOftenEnough_() {
    var nowMs = Date.now();
    if (nowMs > this.expirationTime_) {
      // Already expired - no need to keep updating UI.
      this.stopCountDownCalls_();
    } else if (nowMs >= (this.expirationTime_ - 2 * ONE_HOUR_IN_MS)) {
      // Expires in the next two hours - update UI every minute.
      this.ensureCountDownCalledWithInterval_(ONE_MINUTE_IN_MS);
    } else {
      // Expires some time in the future - update UI every hour.
      this.ensureCountDownCalledWithInterval_(ONE_HOUR_IN_MS);
    }
  },

  /** @private */
  ensureCountDownCalledWithInterval_(intervalMs) {
    if (this.countDownIntervalMs_ == intervalMs) {
      return;
    }
    this.stopCountDownCalls_();
    this.countDownIntervalId_ =
        setInterval(this.countDown_.bind(this), intervalMs);
    this.countDownIntervalMs_ = intervalMs;
  },

  /** @private */
  stopCountDownCalls_() {
    if (!this.countDownIntervalId_) {
      return;
    }
    clearInterval(this.countDownIntervalId_);
    this.countDownIntervalId_ = null;
    this.countDownIntervalMs_ = null;
  },

  /** @private */
  countDown_() {
    this.ensureCountDownCalledOftenEnough_();
    const msUntilExpiry = this.expirationTime_ - Date.now();
    cr.sendWithPromise('getTitleText', msUntilExpiry).then((title) => {
      this.title_ = title;
    });
  },

  /** @private */
  onButtonTap_() {
    chrome.send('continue');
  },

});
