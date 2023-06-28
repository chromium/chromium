// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
const UserCreationScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UserCreationUIState = {
  CREATE: 'create',
};

/**
 * User type for setting up the device.
 * @enum {string}
 */
const UserCreationUserType = {
  SELF: 'self',
  CHILD: 'child',
};


class UserCreation extends UserCreationScreenElementBase {
  static get is() {
    return 'user-creation-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The currently selected user type.
       */
      selectedUserType: {
        type: String,
      },

      /**
       * Is the back button visible on the first step of the screen. Back button
       * is visible iff we are in the add person flow.
       * @private
       */
      isBackButtonVisible_: {
        type: Boolean,
      },

      /** @private */
      titleKey_: {
        type: String,
      },

      /** @private */
      subtitleKey_: {
        type: String,
      },
    };
  }

  constructor() {
    super();
    this.selectedUserType = UserCreationUserType.SELF;
    this.selectedSignInMethod = '';
    this.isBackButtonVisible_ = true;
    this.titleKey_ = 'userCreationTitle';
    this.subtitleKey_ = 'userCreationSubtitle';
  }

  /** @override */
  get EXTERNAL_API() {
    return ['setIsBackButtonVisible'];
  }

  /** @override */
  defaultUIStep() {
    return UserCreationUIState.CREATE;
  }

  get UI_STEPS() {
    return UserCreationUIState;
  }

  onBeforeShow() {
    this.selectedUserType = UserCreationUserType.SELF;
    this.titleKey_ = this.isBackButtonVisible_ ? 'userCreationAddPersonTitle' :
                                                 'userCreationTitle';
    this.subtitleKey_ = this.isBackButtonVisible_ ?
        'userCreationAddPersonSubtitle' :
        'userCreationSubtitle';
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('UserCreationScreen');
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.USER_CREATION;
  }

  setIsBackButtonVisible(isVisible) {
    this.isBackButtonVisible_ = isVisible;
  }

  cancel() {
    if (this.isBackButtonVisible_) {
      this.onBackClicked_();
    }
  }

  onBackClicked_() {
    this.userActed('cancel');
  }

  onNextClicked_() {
    if (this.uiStep === UserCreationUIState.CREATE) {
      if (this.selectedUserType === UserCreationUserType.SELF) {
        this.userActed('signin');
      } else if (this.selectedUserType === UserCreationUserType.CHILD) {
        this.userActed('add-child');
      }
    }
  }
}
customElements.define(UserCreation.is, UserCreation);
