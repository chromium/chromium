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
  IN_PROGRESS: 'in_progress',
  ERROR: 'error',
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
    'showConfirmStep',
    'showInProgressStep',
    'showErrorStep',
    'showSuccessStep',
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

  showConfirmStep() {
    this.setUIStep(UIState.CONFIRM);
  },

  showInProgressStep() {
    this.setUIStep(UIState.IN_PROGRESS);
  },

  showErrorStep() {
    this.setUIStep(UIState.ERROR);
  },

  showSuccessStep() {
    this.setUIStep(UIState.SUCCESS);
  },

  onIntroNextButtonPressed_() {
    this.userActed('os-install-intro-next');
  },

  onConfirmNextButtonPressed_() {
    this.userActed('os-install-confirm-next');
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
});
})();
