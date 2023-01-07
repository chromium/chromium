// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'file-type-select' displays the available file types in a dropdown.
 */
Polymer({
  is: 'file-type-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {boolean} */
    disabled: Boolean,

    /** @type {string} */
    selectedFileType: {
      type: String,
      notify: true,
    },
  },
});
