// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './viewer-thumbnail.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ViewerThumbnailBarElement extends PolymerElement {
  static get is() {
    return 'viewer-thumbnail-bar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      activePage: Number,

      docLength: Number,

      /** @private {Array<number>} */
      pageNumbers_: {
        type: Array,
        computed: 'computePageNumbers_(docLength)',
      },
    };
  }

  /**
   * @return {!Array<number>} The array of page numbers.
   * @private
   */
  computePageNumbers_() {
    return Array.from({length: this.docLength}, (_, i) => i + 1);
  }

  /**
   * @param {number} page
   * @return {boolean} Whether the page is the current page.
   * @private
   */
  isActivePage_(page) {
    return this.activePage === page;
  }
}

customElements.define(ViewerThumbnailBarElement.is, ViewerThumbnailBarElement);
