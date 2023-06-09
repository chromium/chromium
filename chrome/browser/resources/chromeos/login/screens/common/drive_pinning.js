// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for drive pinning screen.
 */

import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const DrivePinningScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * Enum to represent steps on the drive pinning screen.
 * Currently there is only one step, but we still use
 * MultiStepBehavior because it provides implementation of
 * things like processing 'focus-on-show' class
 * @enum {string}
 */
const DrivePinningStep = {
  OVERVIEW: 'overview',
};


/**
 * Available user actions.
 * @enum {string}
 */
const UserAction = {
  ACCEPT: 'driveAccept',
  DECLINE: 'driveDecline',
};

/**
 * @polymer
 */
class DrivePinningScreen extends DrivePinningScreenElementBase {
  static get is() {
    return 'drive-pinning-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Available free space in the disk.
       * @private {String}
       */
      freeSpace_: {
        type: String,
      },

      /**
       * Required space by the drive for pinning.
       * @private {String}
       */
      requiredSpace_: {
        type: String,
      },
    };
  }

  get EXTERNAL_API() {
    return ['setRequiredSpaceInfo'];
  }

  get UI_STEPS() {
    return DrivePinningStep;
  }

  defaultUIStep() {
    return DrivePinningStep.OVERVIEW;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('DrivePinningScreen');
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }


  getSpaceDescription_(locale, requiredSpace, freeSpace) {
    if (requiredSpace && freeSpace) {
      return this.i18nDynamic(
          locale, 'DevicePinningScreenSpaceDescription', requiredSpace,
          freeSpace);
    }
    return '';
  }

  /**
   * Set the required space and free space information.
   */
  setRequiredSpaceInfo(requiredSpace, freeSpace) {
    this.requiredSpace_ = requiredSpace;
    this.freeSpace_ = freeSpace;
  }

  onAcceptButtonClicked_() {
    this.userActed(UserAction.ACCEPT);
  }

  onDeclineButtonClicked_() {
    this.userActed(UserAction.DECLINE);
  }
}

customElements.define(DrivePinningScreen.is, DrivePinningScreen);
