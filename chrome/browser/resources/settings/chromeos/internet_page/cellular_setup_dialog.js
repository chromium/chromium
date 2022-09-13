// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-cellular-setup-dialog' embeds the <cellular-setup>
 * that is shared with OOBE in a dialog with OS Settings stylizations.
 */
import 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_setup.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../settings_shared.css.js';

import {CellularSetupDelegate} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_setup_delegate.js';
import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CellularSetupSettingsDelegate} from './cellular_setup_settings_delegate.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OsSettingsCellularSetupDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class OsSettingsCellularSetupDialogElement extends
    OsSettingsCellularSetupDialogElementBase {
  static get is() {
    return 'os-settings-cellular-setup-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Name of cellular dialog page to be selected.
       * @type {!CellularSetupPageName}
       */
      pageName: String,

      /**
       * @private {!CellularSetupDelegate}
       */
      delegate_: Object,

      /*** @private */
      dialogTitle_: {
        type: String,
      },

      /*** @private */
      dialogHeader_: {
        type: String,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    this.delegate_ = new CellularSetupSettingsDelegate();
  }

  ready() {
    super.ready();

    this.addEventListener('exit-cellular-setup', this.onExitCellularSetup_);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  /** @private*/
  onExitCellularSetup_() {
    this.$.dialog.close();
  }

  /**
   * @param {string} title
   * @returns {boolean}
   * @private
   */
  shouldShowDialogTitle_(title) {
    return !!this.dialogTitle_;
  }

  /**
   * @return {string}
   * @private
   */
  getDialogHeader_() {
    if (this.dialogHeader_) {
      return this.dialogHeader_;
    }

    return this.i18n('cellularSetupDialogTitle');
  }
}

customElements.define(
    OsSettingsCellularSetupDialogElement.is,
    OsSettingsCellularSetupDialogElement);
