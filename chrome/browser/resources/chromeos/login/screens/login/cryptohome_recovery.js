// Copyright 2022 The Chromium Authors. All rights reserved.
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
const CryptohomeRecoveryUIState = {
  LOADING: 'loading',
  DONE: 'done',
  ERROR: 'error',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const CryptohomeRecoveryBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @polymer
 */
class CryptohomeRecovery extends CryptohomeRecoveryBase {
  static get is() {
    return 'cryptohome-recovery-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }

  defaultUIStep() {
    return CryptohomeRecoveryUIState.LOADING;
  }

  get UI_STEPS() {
    return CryptohomeRecoveryUIState;
  }

  get EXTERNAL_API() {
    return ['onRecoverySucceeded', 'onRecoveryFailed'];
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('CryptohomeRecoveryScreen');
  }

  // Invoked just before being shown. Contains all the data for the screen.
  onBeforeShow(data) {}

  reset() {
    this.setUIStep(CryptohomeRecoveryUIState.LOADING);
  }

  /**
   * Called when Cryptohome recovery succeeded.
   */
  onRecoverySucceeded() {
    this.setUIStep(CryptohomeRecoveryUIState.DONE);
  }

  /**
   * Called when Cryptohome recovery failed.
   */
  onRecoveryFailed() {
    this.setUIStep(CryptohomeRecoveryUIState.ERROR);
  }

  /**
   * Enter old password button click handler.
   * @private
   */
  onOldPasswordEntered_() {
    this.userActed('enter-old-password');
  }

  /**
   * Retry button click handler.
   * @private
   */
  onRetry_() {
    this.userActed('retry');
  }

  /**
   * Done button click handler.
   * @private
   */
  onDone_() {
    this.userActed('done');
  }
}

customElements.define(CryptohomeRecovery.is, CryptohomeRecovery);
