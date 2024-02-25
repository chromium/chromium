// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './shared_style.css.js';

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './browser_tabs_metadata_form.html.js';
import {BrowserTabsMetadataModel, ImageType, imageTypeToStringMap} from './types.js';

Polymer({
  is: 'browser-tabs-metadata-form',

  _template: getTemplate(),

  properties: {
    /** @type{BrowserTabsMetadataModel} */
    browserTabMetadata: {
      type: Object,
      notify: true,
      computed: 'getMetadata_(isValid, url_, title_, lastAccessedTimeStamp_, ' +
          'favicon_)',
    },

    /** True if the fields are all filled out. */
    isValid: {
      type: Boolean,
      computed: 'getIsValid_(url_, title_, lastAccessedTimeStamp_, favicon_)',
      reflectToAttribute: true,
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

    /** @private{ImageType} */
    favicon_: {
      type: Number,
      value: ImageType.PINK,
    },

    /** @private */
    faviconList_: {
      type: Array,
      value: () => {
        return [
          ImageType.NONE,
          ImageType.PINK,
          ImageType.RED,
          ImageType.GREEN,
          ImageType.BLUE,
          ImageType.YELLOW,
        ];
      },
      readonly: true,
    },
  },

  /**
   * @param {ImageType} faviconType
   * @return {String}
   * @private
   */
  getImageTypeName_(faviconType) {
    return imageTypeToStringMap.get(faviconType);
  },

  /** @private */
  onFaviconSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#faviconList'));
    this.favicon_ = this.faviconList_[select.selectedIndex];
  },

  /**
   * @return{boolean}
   * @private
   */
  getIsValid_() {
    return !!this.url_ && !!this.title_ && this.lastAccessedTimeStamp_ > -1 &&
        this.favicon_ !== ImageType.NONE;
  },

  /**
   * @return{BrowserTabsMetadataModel}
   * @private
   */
  getMetadata_() {
    return {
      isValid: this.isValid,
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

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_(lhs, rhs) {
    return lhs === rhs;
  },
});
