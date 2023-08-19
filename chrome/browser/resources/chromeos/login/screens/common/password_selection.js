// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';

/**
 * Type of the password for setting up for the user.
 * @enum {string}
 */
const PasswordType = {
  LOCAL_PASSWORD: 'local-password',
  GAIA_PASSWORD: 'gaia-password',
};


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const PasswordSelectionBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @polymer
 */
class PasswordSelection extends PasswordSelectionBase {
  static get is() {
    return 'password-selection-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The currently selected password type.
       */
      selectedPasswordType: {
        type: String,
      },

      /**
       * Enum values for `selectedPasswordType`.
       * @private {PasswordType}
       */
      passwordTypeEnum_: {
        readOnly: true,
        type: Object,
        value: PasswordType,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('PasswordSelectionScreen');
  }

  // Invoked just before being shown. Contains all the data for the screen.
  onBeforeShow(data) {
    this.selectedPasswordType = PasswordType.LOCAL_PASSWORD;
  }

  onBackClicked_() {
    this.userActed('back');
  }

  onNextClicked_() {
    this.userActed(this.selectedPasswordType);
  }
}

customElements.define(PasswordSelection.is, PasswordSelection);
