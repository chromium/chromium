// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_radio_button/cr_card_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
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
const GaiaInfoScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @enum {string}
 */
const GaiaInfoStep = {
  OVERVIEW: 'overview',
};

/**
 * User type for setting up the device.
 * @enum {string}
 */
const UserCreationFlowType = {
  MANUAL: 'manual',
  QUICKSTART: 'quickstart',
};

/**
 * Available user actions.
 * @enum {string}
 */
const UserAction = {
  BACK: 'back',
  MANUAL: 'manual',
  QUICKSTART: 'quickstart',
};

/**
 * @polymer
 */
class GaiaInfoScreen extends GaiaInfoScreenElementBase {
  static get is() {
    return 'gaia-info-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The currently selected flow type.
       * @type {string}
       * @private
       */
      selectedFlowType_: {
        type: String,
        value: '',
      },
      /**
       * Whether Quick start feature is enabled. If it's enabled the quick start
       * button will be shown in the gaia info screen.
       * @type {boolean}
       * @private
       */
      isQuickStartVisible_: {
        type: Boolean,
        value: false,
      },
    };
  }

  get EXTERNAL_API() {
    return ['setQuickStartVisible'];
  }

  get UI_STEPS() {
    return GaiaInfoStep;
  }

  onBeforeShow() {
    this.selectedFlowType_ = '';
    this.setAnimationPlaying_(true);
  }

  onBeforeHide() {
    this.setAnimationPlaying_(false);
  }

  defaultUIStep() {
    return GaiaInfoStep.OVERVIEW;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('GaiaInfoScreen');
  }

  setQuickStartVisible() {
    this.isQuickStartVisible_ = true;
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.GAIA_INFO;
  }

  onNextClicked_() {
    if (this.isQuickStartVisible_ &&
        this.selectedFlowType_ == UserCreationFlowType.QUICKSTART) {
      this.userActed(UserAction.QUICKSTART);
    } else {
      this.userActed(UserAction.MANUAL);
    }
  }

  onBackClicked_() {
    this.userActed(UserAction.BACK);
  }

  isNextButtonEnabled_(isQuickStartVisible, selectedFlowType) {
    return (!this.isQuickStartVisible_) || this.selectedFlowType_;
  }

  /**
   * Play or pause the lottie animation in the legacy flow.
   * @param {boolean} play - whether play or pause the animation.
   * @private
   */
  setAnimationPlaying_(play) {
    const gaiaInfoAnimation =
        this.shadowRoot.querySelector('#gaiaInfoAnimation');
    if (gaiaInfoAnimation) {
      gaiaInfoAnimation.playing = play;
    }
  }
}

customElements.define(GaiaInfoScreen.is, GaiaInfoScreen);
