// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './shared_style.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserTabsMetadataModel, FaviconType} from './types.js';

/**
 * Maps a FaviconType to its title label in the dropdown.
 * @type {!Map<FaviconType, String>}
 */
const faviconTypeToStringMap = new Map([
  [FaviconType.PINK, 'Pink'],
  [FaviconType.RED, 'Red'],
  [FaviconType.GREEN, 'Green'],
  [FaviconType.BLUE, 'Blue'],
  [FaviconType.YELLOW, 'Yellow'],
]);

Polymer({
  is: 'browser-tabs-metadata-form',

  _template: html`{__html_template__}`,

  properties: {
    /** @type{BrowserTabsMetadataModel} */
    browserTabMetadata: {
      type: Object,
      notify: true,
      computed: 'getMetadata_(url_, title_, lastAccessedTimeStamp_, favicon_)',
    },

    /** @private */
    url_: {
      type: String,
      value: 'https://www.google.com/',
    },

    /** @private */
    title_: {
      type: String,
      value: 'Google',
    },

    /** @private */
    lastAccessedTimeStamp_: {
      type: Number,
      value: Date.now(),
    },

    /** @private{FaviconType} */
    favicon_: {
      type: Number,
      value: FaviconType.PINK,
    },

    /** @private */
    faviconList_: {
      type: Array,
      value: () => {
        return [
          FaviconType.PINK,
          FaviconType.RED,
          FaviconType.GREEN,
          FaviconType.BLUE,
          FaviconType.YELLOW,
        ];
      },
      readonly: true,
    },
  },

  /**
   * @param {FaviconType} faviconType
   * @return {String}
   * @private
   */
  getFaviconTypeName_(faviconType) {
    return faviconTypeToStringMap.get(faviconType);
  },

  /** @private */
  onFaviconSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#faviconList'));
    this.favicon_ = this.faviconList_[select.selectedIndex];
  },

  /**
   * @return{BrowserTabsMetadataModel}
   * @private
   */
  getMetadata_() {
    return {
      url: this.url_,
      title: this.title_,
      lastAccessedTimeStamp: this.lastAccessedTimeStamp_,
      favicon: this.favicon_,
    };
  },

  /** @private */
  onLastAccessTimeStampChanged_() {
    const inputValue = this.$$('#lastAccessedTimeStampInput').value;
    if (inputValue < 0) {
      this.lastAccessedTimeStamp_ = 0;
      return;
    }

    this.lastAccessedTimeStamp_ = Number(inputValue);
  },
});
