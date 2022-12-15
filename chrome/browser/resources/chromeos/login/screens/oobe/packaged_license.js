// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Packaged License screen.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.m.js';
import '../../components/common_styles/oobe_common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';

import {afterNextRender, html, mixinBehaviors, Polymer, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
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
const PackagedLicenseScreenBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @typedef {{
 *   packagedLicenseDialog:  OobeAdaptiveDialog,
 * }}
 */
PackagedLicenseScreenBase.$;

/**
 * @polymer
 */
class PackagedLicenseScreen extends PackagedLicenseScreenBase {
  static get is() {
    return 'packaged-license-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('PackagedLicenseScreen');
  }

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$.packagedLicenseDialog;
  }

  /**
   * On-tap event handler for Don't Enroll button.
   * @private
   */
  onDontEnrollButtonPressed_() {
    this.userActed('dont-enroll');
  }

  /**
   * On-tap event handler for Enroll button.
   * @private
   */
  onEnrollButtonPressed_() {
    this.userActed('enroll');
  }
}

customElements.define(PackagedLicenseScreen.is, PackagedLicenseScreen);
