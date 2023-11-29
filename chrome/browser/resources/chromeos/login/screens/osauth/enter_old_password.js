// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
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
const EnterOldPasswordUIState = {
  PASSWORD: 'password',
  PROGRESS: 'progress',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const EnterOldPasswordBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @typedef {{
 *   oldPasswordInput:  CrInputElement,
 *   proceedAnyway:  OobeTextButton,
 * }}
 */
EnterOldPasswordBase.$;

/**
 * @polymer
 */
class EnterOldPassword extends EnterOldPasswordBase {
  static get is() {
    return 'enter-old-password-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      password_: {
        type: String,
        value: '',
      },

      passwordInvalid: {
        type: Boolean,
        value: false,
      },

      disabled: {
        type: Boolean,
        value: false,
      },

      passwordInput_: Object,
    };
  }

  defaultUIStep() {
    return EnterOldPasswordUIState.PASSWORD;
  }

  get UI_STEPS() {
    return EnterOldPasswordUIState;
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return [
      'showWrongPasswordError',
    ];
  }
  // clang-format on

  /**
   * @override
   */
  ready() {
    super.ready();
    this.initializeLoginScreen('EnterOldPasswordScreen');

    this.passwordInput_ = this.$.oldPasswordInput;
    addSubmitListener(this.passwordInput_, this.submit_.bind(this));
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.PASSWORD_CHANGED;
  }

  /**
   * Invoked just before being shown. Contains all the data for the screen.
   */
  onBeforeShow(data) {
    this.reset();
  }

  reset() {
    this.setUIStep(EnterOldPasswordUIState.PASSWORD);
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
    this.passwordInvalid = true;
    this.setUIStep(EnterOldPasswordUIState.PASSWORD);
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
    this.setUIStep(EnterOldPasswordUIState.PROGRESS);
    this.disabled = true;
    this.userActed(['submit', this.passwordInput_.value]);
  }

  /** @private */
  onForgotPasswordClicked_() {
    if (this.disabled) {
      return;
    }
    this.userActed('forgot');
  }

  /** @private */
  onAnimationFinish_() {
    this.focus();
  }

  clearPassword() {
    this.password_ = '';
    this.passwordInvalid = false;
  }
}

customElements.define(EnterOldPassword.is, EnterOldPassword);
