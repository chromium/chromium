// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for OS install screen.
 */

(function() {
const UIState = {
  INTRO: 'intro',
  CONFIRM: 'confirm',
  IN_PROGRESS: 'in-progress',
  FAILED: 'failed',
  NO_DESTINATION_DEVICE_FOUND: 'no-destination-device-found',
  SUCCESS: 'success',
};

Polymer({
  is: 'os-install-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'showStep',
  ],

  UI_STEPS: UIState,

  /**
   * @return {string}
   */
  defaultUIStep() {
    return UIState.INTRO;
  },

  ready() {
    this.initializeLoginScreen('OsInstallScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Set and show screen step.
   * @param {string} step screen step.
   */
  showStep(step) {
    this.setUIStep(step);
  },

  onIntroNextButtonPressed_() {
    this.userActed('os-install-intro-next');
  },

  onConfirmNextButtonPressed_() {
    this.userActed('os-install-confirm-next');
  },

  onErrorSendFeedbackButtonPressed_() {
    this.userActed('os-install-error-send-feedback');
  },

  onErrorShutdownButtonPressed_() {
    this.userActed('os-install-error-shutdown');
  },

  onSuccessShutdownButtonPressed_() {
    this.userActed('os-install-success-shutdown');
  },

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getIntroBodyHtml_(locale) {
    return this.i18nAdvanced('osInstallDialogIntroBody');
  },

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getConfirmBodyHtml_(locale) {
    return this.i18nAdvanced(
        'osInstallDialogConfirmBody', {tags: ['p', 'ul', 'li']});
  },

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getErrorNoDestContentHtml_(locale) {
    return this.i18nAdvanced(
        'osInstallDialogErrorNoDestContent', {tags: ['p', 'ul', 'li']});
  },

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getErrorFailedSubtitleHtml_(locale) {
    return this.i18nAdvanced(
        'osInstallDialogErrorFailedSubtitle', {tags: ['p']});
  },
});
})();
