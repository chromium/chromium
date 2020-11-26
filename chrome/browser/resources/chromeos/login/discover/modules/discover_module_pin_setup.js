// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'discover-pin-setup-card',

  behaviors: [DiscoverModuleBehavior],
});

{
  /** @const */
  let PIN_SETUP_STEPS = {
    LOADING: 'loading',
    PASSWORD: 'password',
    START: 'start',
    CONFIRM: 'confirm',
    DONE: 'done',
  };

  // Time out for quickUnlock API.
  let kApiTimeout = 10000;

  Polymer({
    is: 'discover-pin-setup-module',

    behaviors: [DiscoverModuleBehavior],

    properties: {
      /**
       * True when "Learn more" link should be hidden.
       */
      learnMoreLinkHidden: {
        type: Boolean,
        value: false,
      },

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

      /** @type{PIN_SETUP_STEPS} */
      step_: {
        type: Number,
        value: PIN_SETUP_STEPS.LOADING,
        observer: 'onStepChanged_',
      },

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
       * When this flag is true, Discover UI is displayed as part of FirstRun
       * UI.
       */
      firstRun: {
        type: Boolean,
        value: false,
      },

      /**
       * Value of user password input field.
       * @private
       */
      password_: {
        type: String,
        value: '',
        observer: 'onTypedPasswordChange_',
      },

      /**
       * Helper property which marks password as valid/invalid.
       * @private
       */
      passwordInvalid_: {
        type: Boolean,
        value: false,
      },

      /**
       * Should be true when device has support for PIN login.
       * @private
       */
      hasLoginSupport_: {
        type: Boolean,
        value: false,
      },
    },

    /**
     * True when in first run mode and primary user password has been requested.
     * @private
     */
    firstRunUserPasswordRequested_: false,

    /**
     * Timeout ID for automatic skip timer.
     * @private
     */
    autoSkipTimer_: undefined,

    /**
     * This is called when locale is changed.
     * @override
     */
    updateLocalizedContent() {
      this.behaviors.forEach((behavior) => {
        if (behavior.updateLocalizedContent)
          behavior.updateLocalizedContent.call(this);
      });
      this.$.pinKeyboard.i18nUpdateLocale();
    },

    /**
     * Starts automatic skip timer.
     * @private
     */
    startAutoSkipTimer_() {
      if (this.autoSkipTimer_ !== undefined)
        return;

      this.autoSkipTimer_ = setTimeout(() => {
        console.error('autoSkipTimer triggered!');
        this.onSkipButton_();
      }, kApiTimeout);
    },

    /**
     * Kills automatic skip timer.
     * @private
     */
    stopAutoSkipTimer_() {
      if (this.autoSkipTimer_)
        clearTimeout(this.autoSkipTimer_);

      this.autoSkipTimer_ = undefined;
    },

    /**
     * step_ observer. Makes sure default focus follows step change, and kills
     * autoskip timer if spinner is hidden.
     * @private
     */
    onStepChanged_() {
      let dialogs = this.root.querySelectorAll('oobe-dialog');
      for (let dialog of dialogs) {
        if (dialog.hidden)
          continue;

        dialog.focus();
        break;
      }
      if (this.step_ == PIN_SETUP_STEPS.LOADING)
        return;

      this.stopAutoSkipTimer_();
    },

    /** @override */
    show() {
      this.discoverCallWithReply(
          'discover.pinSetup.getHasLoginSupport', [], (is_available) => {
            this.hasLoginSupport_ = is_available;
          });

      if (this.firstRun) {
        this.getFirstRunUserPassword_();
      } else {
        this.step_ = PIN_SETUP_STEPS.PASSWORD;
      }
    },

    /**
     * Starts fetching QuickUnlock token.
     * @private
     */
    getToken_(password) {
      this.step_ = PIN_SETUP_STEPS.LOADING;
      this.startAutoSkipTimer_();

      this.quickUnlockPrivate_.getAuthToken(password, (tokenInfo) => {
        if (chrome.runtime.lastError) {
          console.error(
              'getAuthToken(): Failed: ' + chrome.runtime.lastError.message);
          if (this.firstRun) {
            // Trigger 'token expired'
            this.authToken_ = '';
          } else {
            this.step_ = PIN_SETUP_STEPS.PASSWORD;
            this.passwordInvalid_ = true;
            // Select the whole password if user entered an incorrect password.
            this.$.passwordInput.select();
          }
          return;
        }
        this.setToken(tokenInfo);
      });
    },

    /**
     * Starts fetching primary user password.
     * @private
     */
    getFirstRunUserPassword_() {
      this.startAutoSkipTimer_();
      this.discoverCallWithReply(
          'discover.pinSetup.getUserPassword', [], (password) => {
            if (chrome.runtime.lastError) {
              console.error(
                  'getUserPassword() failed: ' +
                  chrome.runtime.lastError.message);
              // Trigger 'token expired'
              this.authToken_ = '';
              return;
            }
            this.getToken_(password);
          });
    },

    /**
     * Receives new AuthToken.
     * @private
     */
    setToken(tokenInfo) {
      this.authToken_ = tokenInfo.token;
      // Subtract time from the expiration time to account for IPC delays.
      // Treat values less than the minimum as 0 for testing.
      const IPC_SECONDS = 2;
      const lifetimeMs = tokenInfo.lifetimeSeconds > IPC_SECONDS ?
          (tokenInfo.lifetimeSeconds - IPC_SECONDS) * 1000 :
          0;
      this.clearAuthTokenTimeoutId_ = setTimeout(() => {
        this.authToken_ = '';
      }, lifetimeMs);

      this.step_ = PIN_SETUP_STEPS.START;
    },

    /**
     * Called when the authToken_ changes. If the authToken_ is NOT valid,
     * skips module.
     * @private
     */
    onAuthTokenChanged_() {
      this.password_ = '';
      if (!this.authToken_) {
        this.setModes = null;
        this.step_ = PIN_SETUP_STEPS.LOADING;
        this.onSkipButton_();
        return;
      }
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
        this.step_ = PIN_SETUP_STEPS.CONFIRM;
    },

    /** @private */
    onPinSubmit_() {
      this.$.pinKeyboard.doSubmit();
    },

    /** @private */
    onSetPinDone_() {
      this.step_ = PIN_SETUP_STEPS.DONE;
    },

    /** @private */
    onSkipButton_() {
      this.password_ = '';
      this.authToken_ = '';

      this.stopAutoSkipTimer_();
      this.$.pinKeyboard.resetState();
      this.fire('module-continue');
    },

    /** @private */
    onBackButton_() {
      this.password_ = '';
      this.$.pinKeyboard.resetState();
      this.step_ = PIN_SETUP_STEPS.START;
    },

    /** @private */
    onNextButton_() {
      this.onPinSubmit_();
    },

    /** @private */
    onDoneButton_() {
      this.password_ = '';
      this.authToken_ = '';
      this.$.pinKeyboard.resetState();
      this.fire('module-continue');
    },

    /**
     * Returns true if current UI step is different from expected.
     * @param {PIN_SETUP_STEPS} current_step
     * @param {PIN_SETUP_STEPS} expected_step
     * @private
     */
    isStepHidden_(current_step, expected_step) {
      return current_step != expected_step;
    },

    /**
     * Returns true if "Pin setup" dialog is hidden.
     * @param {PIN_SETUP_STEPS} current_step
     * @private
     */
    isPinSetupHidden_(current_step) {
      return !['start', 'confirm', 'done'].includes(current_step);
    },

    /**
     * Returns true if "Next" button is disabled.
     * @param {PIN_SETUP_STEPS} step this.step_
     * @param {Boolean} enableSubmit this.enableSubmit_
     * @private
     */
    isNextDisabled_(step, enableSubmit) {
      return step != PIN_SETUP_STEPS.DONE && !enableSubmit;
    },

    /**
     * Triggers "Next" button if "Enter" key is pressed when entering password.
     * @param {Event} e Keyboard event.
     * @private
     */
    onKeypress_(e) {
      if (e.key != 'Enter')
        return;

      this.onPasswordSubmitButton_();
    },

    /** @private */
    onPasswordSubmitButton_() {
      if (!this.password_)
        return;

      let password = this.password_;
      this.password_ = '';
      this.getToken_(password);
    },

    /** @private */
    onTypedPasswordChange_() {
      this.passwordInvalid_ = false;
    },
  });
}
