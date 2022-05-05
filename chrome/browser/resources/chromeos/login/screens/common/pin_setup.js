// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

const PinSetupState = {
  START: 'start',
  CONFIRM: 'confirm',
  DONE: 'done',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
 const PinSetupBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
  Polymer.Element);

/**
 * @polymer
 */
class PinSetup extends PinSetupBase {

  static get is() {
    return 'pin-setup-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
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

      /**
       * Indicates whether user is a child account.
       * @type {boolean}
       */
      isChildAccount_: {
        type: Boolean,
        value: false,
      },
    };
  }

  get EXTERNAL_API() {
    return ['setHasLoginSupport'];
  }

  get UI_STEPS() {
    return PinSetupState;
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('PinSetupScreen');
  }

  defaultUIStep() {
    return PinSetupState.START;
  }

  /**
   * @param {OobeTypes.PinSetupScreenParameters} data
   */
  onBeforeShow(data) {
    this.$.pinKeyboard.resetState();
    this.authToken_ = data.auth_token;
    this.isChildAccount_ = data.is_child_account;
  }

  /**
   * Configures message on the final page depending on whether the PIN can
   *  be used to log in.
   */
  setHasLoginSupport(hasLoginSupport) {
    this.hasLoginSupport_ = hasLoginSupport;
  }

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
  }

  /** @private */
  onIsConfirmStepChanged_() {
    if (this.isConfirmStep_) {
      this.setUIStep(PinSetupState.CONFIRM);
    }
  }

  /** @private */
  onPinSubmit_() {
    this.$.pinKeyboard.doSubmit();
  }

  /** @private */
  onSetPinDone_() {
    this.setUIStep(PinSetupState.DONE);
  }

  /** @private */
  onSkipButton_() {
    this.authToken_ = '';
    this.$.pinKeyboard.resetState();
    if (this.uiStep === PinSetupState.CONFIRM) {
      this.userActed('skip-button-in-flow');
    } else {
      this.userActed('skip-button-on-start');
    }
  }

  /** @private */
  onBackButton_() {
    this.$.pinKeyboard.resetState();
    this.setUIStep(PinSetupState.START);
  }

  /** @private */
  onNextButton_() {
    this.onPinSubmit_();
  }

  /** @private */
  onDoneButton_() {
    this.authToken_ = '';
    this.$.pinKeyboard.resetState();
    this.userActed('done-button');
  }
}

customElements.define(PinSetup.is, PinSetup);
