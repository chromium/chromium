// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enable developer features screen implementation.
 */

'use strict';

(function() {

/**
 * Possible UI states of the enable debugging screen.
 * These values must be kept in sync with EnableDebuggingScreenHandler::UIState
 * in C++ code and the order of the enum must be the same.
 * @enum {string}
 */
const UI_STATE = {
  ERROR: 'error',
  NONE: 'none',
  REMOVE_PROTECTION: 'remove-protection',
  SETUP: 'setup',
  WAIT: 'wait',
  DONE: 'done',
};

Polymer({
  is: 'oobe-debugging-element',

  behaviors: [
    OobeI18nBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: ['updateState'],

  properties: {
    /**
     * Current value of password input field.
     */
    password_: {type: String, value: ''},

    /**
     * Current value of repeat password input field.
     */
    passwordRepeat_: {type: String, value: ''},

    /**
     * Whether password input fields are matching.
     */
    passwordsMatch_: {
      type: Boolean,
      computed: 'computePasswordsMatch_(password_, passwordRepeat_)',
    },
  },

  ready() {
    this.initializeLoginScreen('EnableDebuggingScreen', {
      resetAllowed: false,
    });
  },

  defaultUIStep() {
    return UI_STATE.NONE;
  },

  UI_STEPS: UI_STATE,

  /**
   * Returns a control which should receive an initial focus.
   */
  get defaultControl() {
    if (this.uiStep == UI_STATE.REMOVE_PROTECTION)
      return this.$.removeProtectionProceedButton;
    else if (this.uiStep == UI_STATE.SETUP)
      return this.$.password;
    else if (this.uiStep == UI_STATE.DONE)
      return this.$.okButton;
    else if (this.uiStep == UI_STATE.ERROR)
      return this.$.errorOkButton;
  },

  /**
   * Cancels the enable debugging screen and drops the user back to the
   * network settings.
   */
  cancel() {
    this.userActed('cancel');
  },

  /**
   * Update UI for corresponding state of the screen.
   * @param {number} state
   * @suppress {missingProperties}
   */
  updateState(state) {
    // Use `state + 1` as index to locate the corresponding UI_STATE
    this.setUIStep(Object.values(UI_STATE)[state + 1]);

    if (this.defaultControl)
      this.defaultControl.focus();

    if (Oobe.getInstance().currentScreen === this)
      Oobe.getInstance().updateScreenSize(this);
  },

  computePasswordsMatch_(password, password2) {
    return (password.length == 0 && password2.length == 0) ||
        (password == password2 && password.length >= 4);
  },

  onHelpLinkClicked_() {
    this.userActed('learnMore');
  },

  onRemoveButtonClicked_() {
    this.userActed('removeRootFSProtection');
  },

  onEnableButtonClicked_() {
    chrome.send('enableDebuggingOnSetup', [this.password_]);
    this.password_ = '';
    this.passwordRepeat_ = '';
  },

  onOKButtonClicked_() {
    this.userActed('done');
  },

});
})();
