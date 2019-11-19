// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectBehavior} from './select_behavior.js';
import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-color-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, SelectBehavior],

  properties: {
    disabled: Boolean,

    /** @private {boolean} */
    disabled_: {
      type: Boolean,
      computed: 'computeDisabled_(disabled, settings.color.setByPolicy)',
    },
  },

  observers: ['onColorSettingChange_(settings.color.value)'],

  /**
   * @param {*} newValue The new value of the color setting.
   * @private
   */
  onColorSettingChange_: function(newValue) {
    this.selectedValue = /** @type {boolean} */ (newValue) ? 'color' : 'bw';
  },

  /**
   * @param {boolean} disabled Whether color selection is disabled.
   * @param {boolean} managed Whether color selection is managed.
   * @return {boolean} Whether drop-down should be disabled.
   * @private
   */
  computeDisabled_: function(disabled, managed) {
    return !!(disabled || managed);
  },

  /** @param {string} value The new select value. */
  onProcessSelectChange: function(value) {
    this.setSetting('color', value == 'color');
  },
});
