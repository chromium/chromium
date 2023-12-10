// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const CryptohomeRecoverySetupUIState = {
  LOADING: 'loading',
  ERROR: 'error',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const CryptohomeRecoverySetupBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @polymer
 */
class CryptohomeRecoverySetup extends CryptohomeRecoverySetupBase {
  static get is() {
    return 'cryptohome-recovery-setup-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }

  defaultUIStep() {
    return CryptohomeRecoverySetupUIState.LOADING;
  }

  get UI_STEPS() {
    return CryptohomeRecoverySetupUIState;
  }

  get EXTERNAL_API() {
    return [
      'setLoadingState',
      'onSetupFailed',
    ];
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('CryptohomeRecoverySetupScreen');
  }

  reset() {
    this.setUIStep(CryptohomeRecoverySetupUIState.LOADING);
  }

  /**
   * Called to show the spinner in the UI.
   */
  setLoadingState() {
    this.setUIStep(CryptohomeRecoverySetupUIState.LOADING);
  }

  /**
   * Called when Cryptohome recovery setup failed.
   */
  onSetupFailed() {
    this.setUIStep(CryptohomeRecoverySetupUIState.ERROR);
  }

  /**
   * Skip button click handler.
   * @private
   */
  onSkip_() {
    this.userActed('skip');
  }

  /**
   * Retry button click handler.
   * @private
   */
  onRetry_() {
    this.userActed('retry');
  }
}

customElements.define(CryptohomeRecoverySetup.is, CryptohomeRecoverySetup);
