// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './logging_tab.js';
import './phonehub_tab.js';
import './shared_style.css.js';

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './multidevice_internals.html.js';

Polymer({
  is: 'multidevice-internals',

  _template: getTemplate(),

  properties: {
    /** @private */
    selectedTabIndex_: {
      type: Number,
      value: 0,
      observer: 'selectedTabChanged_',
    },

    /** @private */
    path_: {
      type: String,
      value: '',
      observer: 'pathChanged_',
    },

    /** @private */
    tabNames_: {
      type: Array,
      value: () => ['Logs', 'PhoneHub'],
      readonly: true,
    },

  },

  /**
   * Updates the current tab location to reflect selection change
   * @param {number} newValue
   * @param {number|undefined} oldValue
   * @private
   */
  selectedTabChanged_(newValue, oldValue) {
    if (!oldValue) {
      return;
    }
    const defaultTab = this.tabNames_[0].toLowerCase();
    const lowerCaseTabName = this.tabNames_[newValue].toLowerCase();
    this.path_ =
        '/' + (lowerCaseTabName === defaultTab ? '' : lowerCaseTabName);
  },

  /**
   * Returns the index of the currently selected tab corresponding to the
   * path or zero if no match.
   * @param {string} path
   * @return {number}
   * @private
   */
  selectedTabFromPath_(path) {
    const index = this.tabNames_.findIndex(tab => path === tab.toLowerCase());
    if (index < 0) {
      return 0;
    }
    return index;
  },

  /**
   * Updates the selection property on path change.
   * @param {string} newValue
   * @param {string|undefined} oldValue
   * @private
   */
  pathChanged_(newValue, oldValue) {
    this.selectedTabIndex_ = this.selectedTabFromPath_(newValue.substr(1));
  },
});
