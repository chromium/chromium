// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for TPM error screen.
 */

/* #js_imports_placeholder */

/**
 * UI state for the dialog.
 * @enum {string}
 */
const tpmUIState = {
  DEFAULT: 'default',
  TPM_OWNED: 'tpm-owned',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const TPMErrorMessageElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior],
    Polymer.Element);

class TPMErrorMessage extends TPMErrorMessageElementBase {
  static get is() {
    return 'tpm-error-message-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      osName_: {
        type: String,
        computed: 'updateOSName_(isBranded)',
      },

      isBranded: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    this.isBranded = false;
    this.osName_ = this.updateOSName_();
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('TPMErrorMessageScreen', {
      resetAllowed: true,
    });
  }

  /** @override */
  get EXTERNAL_API() {
    return ['setStep', 'setIsBrandedBuild'];
  }

  get UI_STEPS() {
    return tpmUIState;
  }

  /**
   * @return {string}
   */
  defaultUIStep() {
    return tpmUIState.DEFAULT;
  }

  /**
   * @param {string} step
   */
  setStep(step) {
    this.setUIStep(step);
  }

  /**
   * @param {string} locale
   * @param {string} osName
   * @return {string}
   * @private
   */
  getTPMOwnedFailureContent_(locale, osName) {
    return this.i18nAdvanced('errorTPMOwnedContent', {substitutions: [osName]});
  }

  onRestartTap_() {
    this.userActed('reboot-system');
  }

  /**
   * @override
   * @suppress {missingProperties}
   */
  get defaultControl() {
    return this.$.errorDialog;
  }

  /**
   * @param {boolean} is_branded
   */
  setIsBrandedBuild(is_branded) {
    this.isBranded = is_branded;
  }

  /**
   * @return {string} OS name
   */
  updateOSName_() {
    return this.isBranded ? loadTimeData.getString('osInstallCloudReadyOS') :
                            loadTimeData.getString('osInstallChromiumOS');
  }
}

customElements.define(TPMErrorMessage.is, TPMErrorMessage);
