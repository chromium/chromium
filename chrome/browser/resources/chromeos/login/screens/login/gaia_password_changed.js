// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for GAIA password changed screen.
 */

import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {addSubmitListener} from '../../login_ui_tools.js';


/**
 * UI mode for the dialog.
 * @enum {string}
 */
const GaiaPasswordChangedUIState = {
  PASSWORD: 'password',
  FORGOT: 'forgot',
  RECOVERY: 'setup-recovery',
  PROGRESS: 'progress',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const GaiaPasswordChangedBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @typedef {{
 *   oldPasswordInput:  CrInputElement,
 *   oldPasswordInput2:  CrInputElement,
 *   cancel:  OobeTextButton,
 *   tryAgain:  OobeTextButton,
 *   proceedAnyway:  OobeTextButton,
 * }}
 */
GaiaPasswordChangedBase.$;

/**
 * @polymer
 */
class GaiaPasswordChanged extends GaiaPasswordChangedBase {
  static get is() {
    return 'gaia-password-changed-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      email: {
        type: String,
        value: '',
      },

      password_: {
        type: String,
        value: '',
      },

      passwordInvalid_: {
        type: Boolean,
        value: false,
      },

      disabled: {
        type: Boolean,
        value: false,
      },

      passwordInput_: Object,

      isCryptohomeRecoveryUIFlowEnabled_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isCryptohomeRecoveryUIFlowEnabled'),
      },
      isDarkModeActive_: {type: Boolean, value: false},
    };
  }

  defaultUIStep() {
    return GaiaPasswordChangedUIState.PASSWORD;
  }

  get UI_STEPS() {
    return GaiaPasswordChangedUIState;
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return [
      'showWrongPasswordError',
      'suggestRecovery',
    ];
  }
  // clang-format on

  /**
   * @override
   */
  ready() {
    super.ready();
    this.initializeLoginScreen('GaiaPasswordChangedScreen');

    this.passwordInput_ = this.isCryptohomeRecoveryUIFlowEnabled_ ?
        this.$.oldPasswordInput2 :
        this.$.oldPasswordInput;
    addSubmitListener(this.passwordInput_, this.submit_.bind(this));
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.PASSWORD_CHANGED;
  }

  // Invoked just before being shown. Contains all the data for the screen.
  onBeforeShow(data) {
    this.reset();
    this.email = data && 'email' in data && data.email;
    this.passwordInvalid_ = data && 'showError' in data && data.showError;
    if (this.isCryptohomeRecoveryUIFlowEnabled_) {
      this.$.tryAgain.textKey = 'oldPasswordHint';
      this.$.proceedAnyway.textKey = 'continueAndDeleteDataButton';
    }
  }

  reset() {
    this.setUIStep(GaiaPasswordChangedUIState.PASSWORD);
    this.clearPassword();
    this.disabled = false;
  }

  /**
   * Called when Screen fails to authenticate with
   * provided password.
   */
  showWrongPasswordError() {
    this.clearPassword();
    this.disabled = false;
    this.passwordInvalid_ = true;
    this.setUIStep(GaiaPasswordChangedUIState.PASSWORD);
  }

  /**
   * Called when password was successfully updated
   * and it is possible to set up recovery for the user.
   */
  suggestRecovery() {
    this.disabled = false;
    this.setUIStep(GaiaPasswordChangedUIState.RECOVERY);
  }

  /**
   * Returns the src of the illustration.
   * @private
   */
  getImageSource_() {
    return this.isDarkModeActive_ ? 'images/security_lock_dark.svg' :
                                    'images/security_lock_light.svg';
  }

  /**
   * Returns the subtitle message for the data loss warning screen.
   * @param {string} locale The i18n locale.
   * @param {string} email The email address that the user is trying to recover.
   * @returns {string} The translated subtitle message.
   */
  getDataLossWarningSubtitleMessage_(locale, email) {
    return this.i18nAdvancedDynamic(
        locale, 'dataLossWarningSubtitle', {substitutions: [email]});
  }

  /**
   * @private
   */
  submit_() {
    if (this.disabled) {
      return;
    }
    if (!this.passwordInput_.validate()) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUIState.PROGRESS);
    this.disabled = true;
    this.userActed(['migrate-user-data', this.passwordInput_.value]);
  }

  /** @private */
  onForgotPasswordClicked_() {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUIState.FORGOT);
    this.clearPassword();
  }

  /** @private */
  onTryAgainClicked_() {
    this.setUIStep(GaiaPasswordChangedUIState.PASSWORD);
  }

  /** @private */
  onBackButtonClicked_() {
    this.setUIStep(GaiaPasswordChangedUIState.PASSWORD);
  }

  /** @private */
  onAnimationFinish_() {
    this.focus();
  }

  clearPassword() {
    this.password_ = '';
    this.passwordInvalid_ = false;
  }

  /** @private */
  onProceedClicked_() {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUIState.PROGRESS);
    this.disabled = true;
    this.clearPassword();
    this.userActed('resync');
  }

  onNoRecovery_() {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUIState.PROGRESS);
    this.disabled = true;
    this.clearPassword();
    this.userActed('no-recovery');
  }

  onSetRecovery_() {
    if (this.disabled) {
      return;
    }
    this.setUIStep(GaiaPasswordChangedUIState.PROGRESS);
    this.disabled = true;
    this.clearPassword();
    this.userActed('setup-recovery');
  }

  /** @private */
  onCancel_() {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.userActed('cancel');
  }
}

customElements.define(GaiaPasswordChanged.is, GaiaPasswordChanged);
