// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for add child screen.
 */


import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_radio_button/cr_card_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/hd_iron_icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {Oobe} from '../../cr_ui.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const AddChildScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * Sign in method for setting up the device for child.
 * @enum {string}
 */
const AddChildSignInMethod = {
  CREATE: 'create',
  SIGNIN: 'signin',

};

/**
 * Available user actions.
 * @enum {string}
 */
const UserAction = {
  CREATE: 'child-account-create',
  SIGNIN: 'child-signin',
  BACK: 'child-back',
};

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const AddChildUIStep = {
  OVERVIEW: 'overview',
};

/**
 * @typedef {{
 *   learnMoreDialog:  OobeModalDialog,
 *   learnMoreLink: HTMLAnchorElement,
 * }}
 */
AddChildScreenElementBase.$;


/**
 * @polymer
 */
class AddChildScreen extends AddChildScreenElementBase {
  static get is() {
    return 'add-child-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The currently selected sign in method.
       */
      selectedSignInMethod: {
        type: String,
      },
    };
  }

  constructor() {
    super();
    this.selectedSignInMethod = '';
  }


  get EXTERNAL_API() {
    return [];
  }

  onBeforeShow() {
    this.selectedSignInMethod = '';
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('AddChildScreen');
  }

  get UI_STEPS() {
    return AddChildUIStep;
  }

  defaultUIStep() {
    return AddChildUIStep.OVERVIEW;
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.GAIA_SIGNIN;
  }

  cancel() {
    this.onBackClicked_();
  }

  onBackClicked_() {
    this.userActed(UserAction.BACK);
  }

  onNextClicked_() {
    if (this.selectedSignInMethod === AddChildSignInMethod.CREATE) {
      this.userActed(UserAction.CREATE);
    } else if (this.selectedSignInMethod === AddChildSignInMethod.SIGNIN) {
      this.userActed(UserAction.SIGNIN);
    }
  }

  onLearnMoreClicked_() {
    this.$.learnMoreDialog.showDialog();
  }

  focusLearnMoreLink_() {
    this.$.learnMoreLink.focus();
  }
}

customElements.define(AddChildScreen.is, AddChildScreen);
