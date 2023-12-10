// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/auth_setup/set_local_password_input.js';
import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_back_button.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {OobeTypes} from '../../components/oobe_types.js';
import {addSubmitListener} from '../../login_ui_tools.js';


/**
 * UI mode for the dialog.
 * @enum {string}
 */
const LocalPasswordSetupState = {
  PASSWORD: 'password',
  PROGRESS: 'progress',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const LocalPasswordSetupBase = mixinBehaviors(
    [
      OobeI18nBehavior,
      LoginScreenBehavior,
      OobeDialogHostBehavior,
      MultiStepBehavior,
    ],
    PolymerElement);

/**
 * Data that is passed to the screen during onBeforeShow.
 * @typedef {{
 *   showBackButton: boolean,
 * }}
 */
let LocalPasswordSetupScreenData;

/**
 * @polymer
 */
class LocalPasswordSetup extends LocalPasswordSetupBase {
  static get is() {
    return 'local-password-setup-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private
       */
      backButtonVisible_: {
        type: Boolean,
      },
    };
  }

  constructor() {
    super();
    this.backButtonVisible_ = true;
  }

  get EXTERNAL_API() {
    return ['showLocalPasswordSetupFailure'];
  }

  defaultUIStep() {
    return LocalPasswordSetupState.PASSWORD;
  }

  get UI_STEPS() {
    return LocalPasswordSetupState;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('LocalPasswordSetupScreen');
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   * @param {LocalPasswordSetupScreenData} data Screen initial payload
   */
  onBeforeShow(data) {
    this.reset_();
    this.backButtonVisible_ = data['showBackButton'];
  }

  showLocalPasswordSetupFailure() {
    // TODO(b/304963851): Show setup failed message, likely allowing user to
    // retry.
  }

  reset_() {
    this.$.passwordInput.reset();
  }

  onBackClicked_() {
    if (!this.backButtonVisible_) {
      return;
    }
    this.userActed(['back', this.$.passwordInput.value]);
  }

  async onSubmit_() {
    await /** @type {!Object} */ (this.$.passwordInput).validate();
    this.setUIStep(LocalPasswordSetupState.PROGRESS);
    this.userActed(['inputPassword', this.$.passwordInput.value]);
  }

  onDoneClicked_() {
    this.userActed(['done']);
  }

  titleText_(locale, isRecoveryFlow) {
    const key =
        isRecoveryFlow ? 'localPasswordResetTitle' : 'localPasswordSetupTitle';
    return this.i18n(key);
  }
}

customElements.define(LocalPasswordSetup.is, LocalPasswordSetup);
