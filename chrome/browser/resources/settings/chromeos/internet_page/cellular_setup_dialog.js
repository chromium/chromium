// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-cellular-setup-dialog' embeds the <cellular-setup>
 * that is shared with OOBE in a dialog with OS Settings stylizations.
 */
import '//resources/cr_components/chromeos/cellular_setup/cellular_setup.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../../settings_shared_css.js';

import {CellularSetupDelegate} from '//resources/cr_components/chromeos/cellular_setup/cellular_setup_delegate.m.js';
import {Button, ButtonBarState, ButtonState, CellularSetupPageName} from '//resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CellularSetupSettingsDelegate} from './cellular_setup_settings_delegate.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-cellular-setup-dialog',

  behaviors: [I18nBehavior],

  properties: {

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
  },

  /** @override */
  created() {
    this.delegate_ = new CellularSetupSettingsDelegate();
  },

  listeners: {
    'exit-cellular-setup': 'onExitCellularSetup_',
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private*/
  onExitCellularSetup_() {
    this.$.dialog.close();
  },

  /**
   * @param {string} title
   * @returns {boolean}
   * @private
   */
  shouldShowDialogTitle_(title) {
    return !!this.dialogTitle_;
  },

  /**
   * @return {string}
   * @private
   */
  getDialogHeader_() {
    if (this.dialogHeader_) {
      return this.dialogHeader_;
    }

    return this.i18n('cellularSetupDialogTitle');
  },
});
