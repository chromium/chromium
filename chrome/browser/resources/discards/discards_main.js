// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './database_tab.js';
import './discards_tab.js';
import './graph_tab.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


Polymer({
  is: 'discards-main',

  _template: html`{__html_template__}`,

  properties: {
    selected: {
      type: Number,
      value: 0,
      observer: 'selectedChanged_',
    },

    path: {
      type: String,
      value: '',
      observer: 'pathChanged_',
    },

    tabs: {
      type: Array,
      value: () => ['Discards', 'Database', 'Graph'],
    },
  },

  /**
   * Updates the location hash on selection change.
   * @param {number} newValue
   * @param {number|undefined} oldValue
   * @private
   */
  selectedChanged_(newValue, oldValue) {
    if (oldValue !== undefined) {
      // The first tab is special-cased to the empty path.
      const defaultTab = this.tabs[0].toLowerCase();
      const tabName = this.tabs[newValue].toLowerCase();
      this.path = '/' + (tabName === defaultTab ? '' : tabName);
    }
  },

  /**
   * Returns the index of the currently selected tab corresponding to the
   * path or zero if no match.
   * @param {string} path
   * @return {number}
   * @private
   */
  selectedFromPath_(path) {
    const index = this.tabs.findIndex(tab => path === tab.toLowerCase());
    return Math.max(index, 0);
  },

  /**
   * Updates the selection property on path change.
   * @param {string} newValue
   * @param {string|undefined} oldValue
   * @private
   */
  pathChanged_(newValue, oldValue) {
    this.selected = this.selectedFromPath_(newValue.substr(1));
  },
});
