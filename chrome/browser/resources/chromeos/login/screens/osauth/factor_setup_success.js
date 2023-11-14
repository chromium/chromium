// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for signin fatal error.
 */

import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const FactorSetupSuccessBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @typedef {{
 *   retryButton:  OobeTextButton,
 * }}
 */
FactorSetupSuccessBase.$;

// LINT.IfChange
/**
 * Set of modified factors, determine title/subtitle.
 * @enum {string}
 */
const ModifiedFactors = {
  ONLINE_PASSWORD: 'online',
  LOCAL_PASSWORD: 'local',
  ONLINE_PASSWORD_AND_PIN: 'online+pin',
  LOCAL_PASSWORD_AND_PIN: 'local+pin',
  PIN: 'pin',
};

/**
 * Determines if factors were changed as a part of
 * initial setup (set) or during recovery (updated)
 * @enum {string}
 */
const ChangeMode = {
  INITIAL_SETUP: 'set',
  RECOVERY_FLOW: 'update',
};


const ACTION_PROCEED = 'proceed';
// LINT.ThenChange(/chrome/browser/ash/login/screens/osauth/factor_setup_success_screen.cc)

/**
 * @polymer
 */
class FactorSetupSuccessScreen extends FactorSetupSuccessBase {
  static get is() {
    return 'factor-setup-success-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }


  static get properties() {
    return {
      /**
       * @private
       */
      hasNextStep_: {
        type: Boolean,
        value: true,
      },
      /**
       * @private
       */
      factors_: {
        type: String,
        value: ModifiedFactors.ONLINE_PASSWORD,
      },
      /**
       * @private
       */
      changeMode_: {
        type: String,
        value: ChangeMode.INITIAL_SETUP,
      },
    };
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('FactorSetupSuccessScreen');
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.BLOCKING;
  }

  /**
   * Invoked just before being shown. Contains all the data for the screen.
   */
  onBeforeShow(data) {
    this.factors_ = data['modifiedFactors'];
    this.changeMode_ = data['changeMode'];
    this.hasNextStep_ = this.changeMode_ === ChangeMode.INITIAL_SETUP;
  }

  getTitle_(locale, factors, changeMode) {
    if (changeMode === ChangeMode.INITIAL_SETUP) {
      if (factors === ModifiedFactors.LOCAL_PASSWORD) {
        return this.i18n('factorSuccessTitleLocalPasswordSet');
      }
      // Add more strings here once we support more combinations of factors.
      // Fallback option:
      return this.i18n('factorSuccessTitleLocalPasswordSet');
    } else {
      if (factors === ModifiedFactors.LOCAL_PASSWORD) {
        return this.i18n('factorSuccessTitleLocalPasswordUpdated');
      }
      // Add more strings here once we support more combinations of factors.
      // Fallback option:
      return this.i18n('factorSuccessTitleLocalPasswordUpdated');
    }
  }

  getSubtitle_(locale, factors, changeMode) {
    if (factors === ModifiedFactors.LOCAL_PASSWORD) {
      return this.i18n('factorSuccessSubtitleLocalPassword');
    }
    return undefined;
  }

  onProceed_() {
    this.userActed(ACTION_PROCEED);
  }
}

customElements.define(FactorSetupSuccessScreen.is, FactorSetupSuccessScreen);
