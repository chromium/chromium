// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {addSubmitListener} from '../../login_ui_tools.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const LocalDataLossWarningBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, OobeDialogHostBehavior],
    PolymerElement);

/**
 * @polymer
 */
class LocalDataLossWarning extends LocalDataLossWarningBase {
  static get is() {
    return 'local-data-loss-warning-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      email: {
        type: String,
        value: '',
      },

      disabled: {
        type: Boolean,
      },

      isOwner: {
        type: Boolean,
      },
    };
  }

  constructor() {
    super();
    this.disabled = false;
  }

  /**
   * @override
   */
  ready() {
    super.ready();
    this.initializeLoginScreen('LocalDataLossWarningScreen');
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.PASSWORD_CHANGED;
  }

  /**
   * Invoked just before being shown. Contains all the data for the screen.
   */
  onBeforeShow(data) {
    this.isOwner = data['isOwner'];
    this.email = data['email'];
  }

  /**
   * Returns the subtitle message for the data loss warning screen.
   * @param {string} locale The i18n locale.
   * @param {string} email The email address that the user is trying to recover.
   * @returns {string} The translated subtitle message.
   */
  getDataLossWarningSubtitleMessage_(locale, email) {
    return this.i18nAdvancedDynamic(
        locale, 'dataLossWarningSubtitle', {substitutions: [email]});
  }

  /** @private */
  onProceedClicked_() {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.userActed('recreateUser');
  }

  /** @private */
  onResetClicked_() {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.userActed('powerwash');
  }

  onBackButtonClicked_() {
    if (this.disabled) {
      return;
    }
    this.userActed('back');
  }
}

customElements.define(LocalDataLossWarning.is, LocalDataLossWarning);
