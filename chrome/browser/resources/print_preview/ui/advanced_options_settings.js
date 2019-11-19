// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './advanced_settings_dialog.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {Settings} from '../data/model.js';

Polymer({
  is: 'print-preview-advanced-options-settings',

  _template: html`{__html_template__}`,

  properties: {
    disabled: Boolean,

    /** @type {!Destination} */
    destination: Object,

    /** @type {!Settings} */
    settings: Object,

    /** @private {boolean} */
    showAdvancedDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onButtonClick_: function() {
    this.showAdvancedDialog_ = true;
  },

  /** @private */
  onDialogClose_: function() {
    this.showAdvancedDialog_ = false;
    this.$.button.focus();
  },
});
