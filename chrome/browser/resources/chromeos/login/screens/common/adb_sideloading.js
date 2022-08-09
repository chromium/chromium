// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying ARC ADB sideloading screen.
 */

import {PolymerElement, html, mixinBehaviors} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import '../../components/oobe_icons.m.js';
import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.m.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.m.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.m.js';
import '../../components/common_styles/common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.m.js';

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const AdbSideloadingState = {
  ERROR: 'error',
  SETUP: 'setup',
};

/**
 * The constants need to be synced with EnableAdbSideloadingScreenView::UIState
 * @enum {number}
 */
const ADB_SIDELOADING_SCREEN_STATE = {
  ERROR: 1,
  SETUP: 2,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const AdbSideloadingBase = mixinBehaviors([OobeI18nBehavior,
  LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @polymer
 */
class AdbSideloading extends AdbSideloadingBase {
  static get is() {
    return 'adb-sideloading-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();
  }

  get EXTERNAL_API() {
    return ['setScreenState'];
  }

  get UI_STEPS() {
    return AdbSideloadingState;
  }

  defaultUIStep() {
    return AdbSideloadingState.SETUP;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('EnableAdbSideloadingScreen');
  }

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  onBeforeShow(data) {
    this.setScreenState(ADB_SIDELOADING_SCREEN_STATE.SETUP);
  }

  /**
   * Sets UI state for the dialog to show corresponding content.
   * @param {ADB_SIDELOADING_SCREEN_STATE} state
   */
  setScreenState(state) {
    if (state == ADB_SIDELOADING_SCREEN_STATE.ERROR) {
      this.setUIStep(AdbSideloadingState.ERROR);
    } else if (state == ADB_SIDELOADING_SCREEN_STATE.SETUP) {
      this.setUIStep(AdbSideloadingState.SETUP);
    }
  }

  /**
   * On-tap event handler for enable button.
   *
   * @private
   */
  onEnableTap_() {
    this.userActed('enable-pressed');
  }

  /**
   * On-tap event handler for cancel button.
   *
   * @private
   */
  onCancelTap_() {
    this.userActed('cancel-pressed');
  }

  /**
   * On-tap event handler for learn more link.
   *
   * @private
   */
  onLearnMoreTap_() {
    this.userActed('learn-more-link');
  }
}

customElements.define(AdbSideloading.is, AdbSideloading);
