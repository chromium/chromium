// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for OS trial screen.
 */

(function() {

/**
 * Trial option for setting up the device.
 * @enum {string}
 */
const TrialOption = {
  INSTALL: 'install',
  TRY: 'try',
};

Polymer({
  is: 'os-trial-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
  ],

  EXTERNAL_API: [
    'setIsBrandedBuild',
  ],

  properties: {
    /**
     * The currently selected trial option.
     */
    selectedTrialOption: {
      type: String,
      value: TrialOption.INSTALL,
    },

    osName_: {
      type: String,
      computed: 'updateOSName_(isBranded)',
    },

    isBranded: {
      type: Boolean,
      value: true,
    },
  },

  ready() {
    this.initializeLoginScreen('OsTrialScreen', {
      resetAllowed: true,
    });
  },

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getSubtitleHtml_(locale, osName) {
    return this.i18nAdvanced('osTrialSubtitle', {substitutions: [osName]});
  },

  /**
   * This is the 'on-click' event handler for the 'next' button.
   * @private
   */
  onNextButtonClick_() {
    if (this.selectedTrialOption == TrialOption.TRY)
      this.userActed('os-trial-try');
    else
      this.userActed('os-trial-install');
  },

  /**
   * This is the 'on-click' event handler for the 'back' button.
   * @private
   */
  onBackButtonClick_() {
    this.userActed('os-trial-back');
  },

  /**
   * @param {boolean} is_branded
   */
  setIsBrandedBuild(is_branded) {
    this.isBranded = is_branded;
  },

  /**
   * @return {string} OS name
   */
  updateOSName_() {
    return this.isBranded ? loadTimeData.getString('osInstallCloudReadyOS') :
                            loadTimeData.getString('osInstallChromiumOS');
  },
});
})();
