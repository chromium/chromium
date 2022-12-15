// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design App Downloading
 * screen.
 */

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_cr_lottie.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const AppDownloadingBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

class AppDownloading extends AppDownloadingBase {
  static get is() {
    return 'app-downloading-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }


  static get properties() {
    return {};
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('AppDownloadingScreen');
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$['app-downloading-dialog'];
  }

  /** Called when dialog is shown */
  onBeforeShow() {
    if (this.$.downloadingApps) {
      this.$.downloadingApps.playing = true;
    }
  }

  /** Called when dialog is hidden */
  onBeforeHide() {
    if (this.$.downloadingApps) {
      this.$.downloadingApps.playing = false;
    }
  }

  /** @private */
  onContinue_() {
    this.userActed('appDownloadingContinueSetup');
  }
}

customElements.define(AppDownloading.is, AppDownloading);
