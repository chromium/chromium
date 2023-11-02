// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Constants used for logging the pin unlock setup uma.
 */

/**
 * Name of the pin unlock setup uma histogram.
 * @type {string}
 */
const PinUnlockUmaHistogramName = 'Settings.PinUnlockSetup';

/**
 * Stages the user can enter while setting up pin unlock.
 * @enum {number}
 */
export const LockScreenProgress = {
  START_SCREEN_LOCK: 0,
  ENTER_PASSWORD_CORRECTLY: 1,
  CHOOSE_PIN_OR_PASSWORD: 2,
  ENTER_PIN: 3,
  CONFIRM_PIN: 4,
  MAX_BUCKET: 5,
};

/**
 * Helper function to send the progress of the pin setup to be recorded in the
 * histogram.
 * @param {LockScreenProgress} currentProgress
 */
export const recordLockScreenProgress = function(currentProgress) {
  if (currentProgress >= LockScreenProgress.MAX_BUCKET) {
    console.error(
        'Expected a enumeration value of ' + LockScreenProgress.MAX_BUCKET +
        ' or lower: Received ' + currentProgress + '.');
    return;
  }
  chrome.send('metricsHandler:recordInHistogram', [
    PinUnlockUmaHistogramName,
    currentProgress,
    LockScreenProgress.MAX_BUCKET,
  ]);
};
