// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for smart privacy protection screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const SmartPrivacyProtectionScreenElementBase = mixinBehaviors(
    [OobeDialogHostBehavior, OobeI18nBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @polymer
 */
class SmartPrivacyProtectionScreen extends
    SmartPrivacyProtectionScreenElementBase {
  static get is() {
    return 'smart-privacy-protection-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * True screen lock is enabled.
       * @private
       */
      isQuickDimEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isQuickDimEnabled');
        },
        readOnly: true,
      },

      /**
       * Indicates whether user is minor mode user (e.g. under age of 18).
       * @private
       */
      isMinorMode_: {
        type: Boolean,
        // TODO(dkuzmin): change the default value once appropriate capability
        // is available on C++ side.
        value: true,
      },
    };
  }

  get EXTERNAL_API() {
    return ['setIsMinorMode'];
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('SmartPrivacyProtectionScreen');
  }

  /**
   * Set the minor mode flag, which controls whether we could use nudge
   * techinuque on the UI.
   * @param {boolean} isMinorMode
   */
  setIsMinorMode(isMinorMode) {
    this.isMinorMode_ = isMinorMode;
  }

  onTurnOnButtonClicked_() {
    this.userActed('continue-feature-on');
  }

  onNoThanksButtonClicked_() {
    this.userActed('continue-feature-off');
  }
}

customElements.define(
    SmartPrivacyProtectionScreen.is, SmartPrivacyProtectionScreen);
