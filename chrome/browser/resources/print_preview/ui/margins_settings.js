// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MarginsType} from '../data/margins.js';

import {SelectBehavior} from './select_behavior.js';
import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-margins-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, SelectBehavior],

  properties: {
    disabled: Boolean,

    /** Mirroring the enum so that it can be used from HTML bindings. */
    MarginsTypeEnum: Object,
  },

  observers: ['onMarginsSettingChange_(settings.margins.value)'],

  /** @override */
  ready: function() {
    this.MarginsTypeEnum = MarginsType;
  },

  /**
   * @param {*} newValue The new value of the margins setting.
   * @private
   */
  onMarginsSettingChange_: function(newValue) {
    this.selectedValue =
        /** @type {!MarginsType} */ (newValue).toString();
  },

  /** @param {string} value The new select value. */
  onProcessSelectChange: function(value) {
    this.setSetting('margins', parseInt(value, 10));
  },

  /**
   * @param {boolean} globallyDisabled Value of the |disabled| property.
   * @param {number} pagesPerSheet Number of pages per sheet.
   * @return {boolean} Whether the margins settings button should be disabled.
   * @private
   */
  getMarginsSettingsDisabled_: function(globallyDisabled, pagesPerSheet) {
    return globallyDisabled || pagesPerSheet > 1;
  },
});
