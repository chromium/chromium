// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

const UIState = {
  START: 'start',
  CONFIRM: 'confirm',
  DONE: 'done',
};

Polymer({
  is: 'pin-setup-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'setHasLoginSupport',
  ],

  UI_STEPS: UIState,

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  },

  properties: {
    /**
     * Flag from <setup-pin-keyboard>.
     * @private
     */
    enableSubmit_: {
      type: Boolean,
      value: false,
    },

    /**
     * Flag from <setup-pin-keyboard>.
     * @private
     */
    isConfirmStep_: {
      type: Boolean,
      value: false,
      observer: 'onIsConfirmStepChanged_',
    },

    /** QuickUnlockPrivate API token. */
    authToken_: {
      type: String,
      observer: 'onAuthTokenChanged_',
    },

    setModes: Object,

    /**
     * Interface for chrome.quickUnlockPrivate calls. May be overridden by
     * tests.
     * @type {QuickUnlockPrivate}
     * @private
     */
    quickUnlockPrivate_: {type: Object, value: chrome.quickUnlockPrivate},

    /**
     * writeUma is a function that handles writing uma stats. It may be
     * overridden for tests.
     *
     * @type {Function}
     * @private
     */
    writeUma_: {
      type: Object,
      value() {
        return settings.recordLockScreenProgress;
      }
    },

    /**
     * Should be true when device has support for PIN login.
     * @private
     */
    hasLoginSupport_: {
      type: Boolean,
      value: false,
    },
  },  // properties

  ready() {
    this.initializeLoginScreen('PinSetupScreen', {
      resetAllowed: true,
    });
  },

  /**
   * This is called when locale is changed.
   * Overridden from LoginScreenBehavior.
   */
  updateLocalizedContent() {
    this.behaviors.forEach((behavior) => {
      if (behavior.updateLocalizedContent)
        behavior.updateLocalizedContent.call(this);
    });
    this.$.pinKeyboard.i18nUpdateLocale();
  },

  defaultUIStep() {
    return UIState.START;
  },

  /**
   * @param {OobeTypes.PinSetupScreenParameters} data
   */
  onBeforeShow(data) {
    this.authToken_ = data.auth_token;
  },

  /**
   * Configures message on the final page depending on whether the PIN can
   *  be used to log in.
   */
  setHasLoginSupport(hasLoginSupport) {
    this.hasLoginSupport = hasLoginSupport;
  },

  /**
   * Called when the authToken_ changes. If the authToken_ is NOT valid,
   * skips module.
   * @private
   */
  onAuthTokenChanged_() {
    this.setModes = (modes, credentials, onComplete) => {
      this.quickUnlockPrivate_.setModes(
          this.authToken_, modes, credentials, () => {
            let result = true;
            if (chrome.runtime.lastError) {
              console.error(
                  'setModes failed: ' + chrome.runtime.lastError.message);
              result = false;
            }
            onComplete(result);
          });
    };
  },

  /** @private */
  onIsConfirmStepChanged_() {
    if (this.isConfirmStep_)
      this.setUIStep(UIState.CONFIRM);
  },

  /** @private */
  onPinSubmit_() {
    this.$.pinKeyboard.doSubmit();
  },

  /** @private */
  onSetPinDone_() {
    this.setUIStep(UIState.DONE);
  },

  /** @private */
  onSkipButton_() {
    this.authToken_ = '';
    this.$.pinKeyboard.resetState();
    if (this.uiStep === UIState.CONFIRM) {
      this.userActed('skip-button-in-flow');
    } else {
      this.userActed('skip-button-on-start');
    }
  },

  /** @private */
  onBackButton_() {
    this.$.pinKeyboard.resetState();
    this.setUIStep(UIState.START);
  },

  /** @private */
  onNextButton_() {
    this.onPinSubmit_();
  },

  /** @private */
  onDoneButton_() {
    this.authToken_ = '';
    this.$.pinKeyboard.resetState();
    this.userActed('done-button');
  },
});  // Polymer
})();
