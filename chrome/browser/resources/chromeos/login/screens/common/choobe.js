// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for theme selection screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {OobeScreensList} from '../../components/oobe_screens_list.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const ChoobeScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);


const ChoobeStep = {
  OVERVIEW: 'overview',
};

/**
 * Available user actions.
 * @enum {string}
 */
const UserAction = {
  SKIP: 'choobeSkip',
  NEXT: 'choobeSelect',
};

/**
 * @polymer
 */
class ChoobeScreen extends ChoobeScreenElementBase {
  static get is() {
    return 'choobe-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      numberOfSelectedScreens_: {
        type: Number,
        value: 0,
      },
    };
  }

  get UI_STEPS() {
    return ChoobeStep;
  }

  defaultUIStep() {
    return ChoobeStep.OVERVIEW;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('ChoobeScreen');
  }

  onBeforeShow(data) {
    if ('screens' in data) {
      this.$.screensList.init(data['screens']);
    }
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  onNextClicked_() {
    const screenSelected = this.$.screensList.getScreenSelected();
    this.userActed([UserAction.NEXT, screenSelected]);
  }

  onSkip_() {
    this.userActed(UserAction.SKIP);
  }

  canProceed_() {
    return this.numberOfSelectedScreens_ > 0;
  }
}
customElements.define(ChoobeScreen.is, ChoobeScreen);