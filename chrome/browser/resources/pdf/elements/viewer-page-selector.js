// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-vars.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'viewer-page-selector',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * The number of pages the document contains.
     */
    docLength: {type: Number, value: 1, observer: 'docLengthChanged_'},

    /**
     * The current page being viewed (1-based). A change to pageNo is mirrored
     * immediately to the input field. A change to the input field is not
     * mirrored back until pageNoCommitted() is called and change-page is fired.
     */
    pageNo: {
      type: Number,
      value: 1,
    },
  },

  /** @return {!HTMLInputElement} */
  get pageSelector() {
    return /** @type {!HTMLInputElement} */ (this.$.pageselector);
  },

  pageNoCommitted() {
    const page = parseInt(this.pageSelector.value, 10);

    if (!isNaN(page) && page <= this.docLength && page > 0) {
      this.fire('change-page', {page: page - 1, origin: 'pageselector'});
    } else {
      this.pageSelector.value = this.pageNo.toString();
    }
    this.pageSelector.blur();
  },

  /** @private */
  docLengthChanged_() {
    const numDigits = this.docLength.toString().length;
    this.style.setProperty('--page-length-digits', `${numDigits}`);
  },

  select() {
    this.pageSelector.select();
  },

  /**
   * @return {boolean} True if the selector input field is currently focused.
   */
  isActive() {
    return this.shadowRoot.activeElement === this.pageSelector;
  },

  /**
   * Immediately remove any non-digit characters.
   * @private
   */
  onInput_() {
    this.pageSelector.value = this.pageSelector.value.replace(/[^\d]/, '');
  },
});
