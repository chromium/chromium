// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'multi-page-checkbox' displays the checkbox for starting a multi-page scan.
 */
Polymer({
  is: 'multi-page-checkbox',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** {boolean} */
    multiPageScanChecked: {
      type: Boolean,
      notify: true,
    },

    /** @type {boolean} */
    disabled: Boolean,
  },

  /** @private */
  onCheckboxClick_() {
    if (this.disabled) {
      return;
    }

    this.multiPageScanChecked = !this.multiPageScanChecked;
  },
});
