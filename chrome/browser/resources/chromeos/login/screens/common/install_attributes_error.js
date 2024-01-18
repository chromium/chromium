// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for install attributes error screen.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';

import {getTemplate} from './install_attributes_error.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 */
const InstallAttributesErrorMessageElementBase =
    mixinBehaviors([OobeI18nBehavior, LoginScreenBehavior], PolymerElement);

/**
 * @typedef {{
 *   errorDialog:  OobeAdaptiveDialog,
 * }}
 */
InstallAttributesErrorMessageElementBase.$;

class InstallAttributesErrorMessage extends
    InstallAttributesErrorMessageElementBase {
  static get is() {
    return 'install-attributes-error-message-element';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Whether the restart is required for powerwash to be available. */
      isRestartRequired_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    this.isRestartRequired_ = false;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('InstallAttributesErrorMessageScreen');
  }

  onPowerwashTap_() {
    this.userActed('powerwash-pressed');
  }

  onRestartTap_() {
    this.userActed('reboot-system');
  }

  onBeforeShow(data) {
    this.isRestartRequired_ = data['restartRequired'];
  }

  /**
   * @override
   */
  get defaultControl() {
    return this.$.errorDialog;
  }
}

customElements.define(
    InstallAttributesErrorMessage.is, InstallAttributesErrorMessage);
