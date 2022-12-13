// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/quick_unlock/setup_pin_keyboard.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.m.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {recordLockScreenProgress} from '//resources/ash/common/quick_unlock/lock_screen_constants.js';
import {assert, assertNotReached} from '//resources/ash/common/assert.js';
import {dom, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.m.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE, SCREEN_GAIA_SIGNIN} from '../../components/display_manager_types.js';
import {OobeTypes} from '../../components/oobe_types.js';


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
const PinSetupBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @polymer
 */
class PinSetup extends PinSetupBase {

  static get is() {
    return 'pin-setup-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

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
          return recordLockScreenProgress;
        },
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
