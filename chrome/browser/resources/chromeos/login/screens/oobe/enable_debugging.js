// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enable developer features screen implementation.
 */

import '//resources/cr_elements/action_link.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';


/**
 * Possible UI states of the enable debugging screen.
 * These values must be kept in sync with EnableDebuggingScreenHandler::UIState
 * in C++ code and the order of the enum must be the same.
 * @enum {string}
 */
const EnableDebuggingState = {
  ERROR: 'error',
  NONE: 'none',
  REMOVE_PROTECTION: 'remove-protection',
  SETUP: 'setup',
  WAIT: 'wait',
  DONE: 'done',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const EnableDebuggingBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @typedef {{
 *   removeProtectionProceedButton:  OobeTextButton,
 *   password:  CrInputElement,
 *   okButton:  OobeTextButton,
 *   errorOkButton: OobeTextButton
 * }}
 */
 EnableDebuggingBase.$;

/**
 * @polymer
 */
class EnableDebugging extends EnableDebuggingBase {
  static get is() {
    return 'enable-debugging-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  get EXTERNAL_API() {
    return ['updateState'];
  }

  static get properties() {
    return {
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
    };
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('EnableDebuggingScreen');
  }

  defaultUIStep() {
    return EnableDebuggingState.NONE;
  }

  get UI_STEPS() {
    return EnableDebuggingState;
  }

  /**
   * Returns a control which should receive an initial focus.
   */
  get defaultControl() {
    if (this.uiStep == EnableDebuggingState.REMOVE_PROTECTION) {
      return this.$.removeProtectionProceedButton;
    } else if (this.uiStep == EnableDebuggingState.SETUP) {
      return this.$.password;
    } else if (this.uiStep == EnableDebuggingState.DONE) {
      return this.$.okButton;
    } else if (this.uiStep == EnableDebuggingState.ERROR) {
      return this.$.errorOkButton;
    } else {
      return null;
    }
  }

  /**
   * Cancels the enable debugging screen and drops the user back to the
   * network settings.
   */
  cancel() {
    this.userActed('cancel');
  }

  /**
   * Update UI for corresponding state of the screen.
   * @param {number} state
   */
  updateState(state) {
    // Use `state + 1` as index to locate the corresponding EnableDebuggingState
    this.setUIStep(Object.values(EnableDebuggingState)[state + 1]);

    if (this.defaultControl) {
      this.defaultControl.focus();
    }
  }

  computePasswordsMatch_(password, password2) {
    return (password.length == 0 && password2.length == 0) ||
        (password == password2 && password.length >= 4);
  }

  onHelpLinkClicked_() {
    this.userActed('learnMore');
  }

  onRemoveButtonClicked_() {
    this.userActed('removeRootFSProtection');
  }

  onEnableButtonClicked_() {
    this.userActed(['setup', this.password_]);
    this.password_ = '';
    this.passwordRepeat_ = '';
  }

  onOKButtonClicked_() {
    this.userActed('done');
  }
}

customElements.define(EnableDebugging.is, EnableDebugging);
