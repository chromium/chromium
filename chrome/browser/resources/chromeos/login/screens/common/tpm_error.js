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
  DBUS_ERROR: 'dbus-error',
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

/**
 * @typedef {{
 *   errorDialog:  OobeAdaptiveDialogElement,
 * }}
 */
TPMErrorMessageElementBase.$;

class TPMErrorMessage extends TPMErrorMessageElementBase {
  static get is() {
    return 'tpm-error-message-element';
  }

  /* #html_template_placeholder */

  static get properties() {}

  constructor() {
    super();
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('TPMErrorMessageScreen', {
      resetAllowed: true,
    });
  }

  /** @override */
  get EXTERNAL_API() {
    return [
      'setStep',
    ];
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
   * @return {string}
   * @private
   */
  getTPMOwnedFailureContent_(locale) {
    return this.i18nAdvanced('errorTPMOwnedContent');
  }

  onRestartTap_() {
    this.userActed('reboot-system');
  }

  /**
   * @override
   */
  get defaultControl() {
    return this.$.errorDialog;
  }
}

customElements.define(TPMErrorMessage.is, TPMErrorMessage);
