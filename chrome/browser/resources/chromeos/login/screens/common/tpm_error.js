// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for TPM error screen.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.m.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.m.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';


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
const TPMErrorMessageElementBase = mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior], PolymerElement);

/**
 * @typedef {{
 *   errorDialog:  OobeAdaptiveDialog,
 * }}
 */
TPMErrorMessageElementBase.$;

class TPMErrorMessage extends TPMErrorMessageElementBase {
  static get is() {
    return 'tpm-error-message-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {}

  constructor() {
    super();
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('TPMErrorMessageScreen');
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
