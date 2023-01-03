// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview wrong HWID screen implementation.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/buttons/oobe_text_button.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE, SCREEN_GAIA_SIGNIN} from '../../components/display_manager_types.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const WrongHWIDBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @polymer
 */
class WrongHWID extends WrongHWIDBase {
  static get is() {
    return 'wrong-hwid-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }



  static get properties() {
    return {};
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('WrongHWIDMessageScreen');
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.WRONG_HWID_WARNING;
  }

  onSkip_() {
    this.userActed('skip-screen');
  }
}

customElements.define(WrongHWID.is, WrongHWID);
