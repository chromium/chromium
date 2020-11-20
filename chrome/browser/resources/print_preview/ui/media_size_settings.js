// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './print_preview_shared_css.js';
import './settings_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsBehavior} from './settings_behavior.js';
import {SelectOption} from './settings_select.js';

Polymer({
  is: 'print-preview-media-size-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior],

  properties: {
    capability: Object,

    disabled: Boolean,
  },

  observers:
      ['onMediaSizeSettingChange_(settings.mediaSize.*, capability.option)'],

  /** @private */
  onMediaSizeSettingChange_() {
    if (!this.capability) {
      return;
    }
    const valueToSet = JSON.stringify(this.getSettingValue('mediaSize'));
    for (const option of
         /** @type {!Array<!SelectOption>} */ (this.capability.option)) {
      if (JSON.stringify(option) === valueToSet) {
        this.$$('print-preview-settings-select').selectValue(valueToSet);
        return;
      }
    }

    const defaultOption = this.capability.option.find(o => !!o.is_default) ||
        this.capability.option[0];
    this.setSetting('mediaSize', defaultOption);
  },
});
